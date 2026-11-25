#include <ntddk.h>
#include <wdf.h>
#include <bthdef.h>
#include <bthioctl.h>
#include <bthddi.h>
#include <hidport.h>
#include <vhf.h>

// Instantiate the Bluetooth profile-interface GUID in this translation unit.
#include <initguid.h>
#include <bthguid.h>

#define XPMX_POOL_TAG 'xMpX'
#define XPMX_CONTROL_PSM 0x0011
#define XPMX_INTERRUPT_PSM 0x0013
#define XPMX_READ_BUFFER_SIZE 672
#define XPMX_RX_LOG_BYTES 16
#define XPMX_HIDP_INPUT 0xA1
#define XPMX_GAMEPAD_REPORT_ID 0x07
#define XPMX_GAMEPAD_PACKET_LENGTH 12
#define XPMX_VHF_REPORT_BUFFER_LENGTH 11

// Extracted verbatim as the Game Pad top-level collection from the
// controller's 208-byte HOME+X SDP report descriptor.
static UCHAR XpmxGamepadReportDescriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x07,
    0x09, 0x01, 0xA1, 0x00, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x32, 0x09, 0x35, 0x15, 0x00, 0x26, 0xFF,
    0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0xC0,
    0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00,
    0x46, 0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95,
    0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01, 0x81,
    0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0F, 0x15,
    0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81,
    0x02, 0x05, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x09, 0xC4, 0x09, 0xC5, 0x95, 0x02, 0x75, 0x08,
    0x81, 0x02, 0x75, 0x08, 0x95, 0x01, 0x81, 0x01,
    0xC0
};

typedef struct _DEVICE_CONTEXT DEVICE_CONTEXT, *PDEVICE_CONTEXT;

typedef struct _CHANNEL_CONTEXT {
    PDEVICE_CONTEXT DeviceContext;
    USHORT Psm;
    PCSTR Name;
    HANDLE Handle;
    KSPIN_LOCK Lock;
    volatile LONG Disconnected;
} CHANNEL_CONTEXT, *PCHANNEL_CONTEXT;

typedef struct _INTERRUPT_READER_CONTEXT {
    PDEVICE_CONTEXT DeviceContext;
    WDFREQUEST Request;
    WDFMEMORY BufferMemory;
    WDFMEMORY BrbMemory;
    struct _BRB_L2CA_ACL_TRANSFER Brb;
    PUCHAR Buffer;
    KDPC ResubmitDpc;
    KEVENT StopEvent;
    volatile LONG Stopping;
    volatile LONG Running;
    volatile LONG Initialized;
} INTERRUPT_READER_CONTEXT, *PINTERRUPT_READER_CONTEXT;

struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFIOTARGET IoTarget;
    BTH_PROFILE_DRIVER_INTERFACE ProfileInterface;
    BTH_DEVICE_INFO RemoteDeviceInfo;
    CHANNEL_CONTEXT ControlChannel;
    CHANNEL_CONTEXT InterruptChannel;
    INTERRUPT_READER_CONTEXT InterruptReader;
    VHFHANDLE VhfHandle;
    BOOLEAN VhfStarted;
    BOOLEAN ProfileInterfaceReady;
    BOOLEAN DeviceInfoReady;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD XpmxEvtDeviceAdd;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT XpmxEvtSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP XpmxEvtSelfManagedIoCleanup;

static VOID XpmxReadCompletion(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
    );

static VOID XpmxReadResubmitDpc(
    _In_ struct _KDPC *Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    );

static
VOID
XpmxLogStatus(
    _In_z_ PCSTR Operation,
    _In_ NTSTATUS Status
    )
{
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        NT_SUCCESS(Status) ? DPFLTR_INFO_LEVEL : DPFLTR_ERROR_LEVEL,
        "[XP-Mx210BL] %s: NTSTATUS=0x%08X\n",
        Operation,
        (ULONG)Status
        );
}

static
NTSTATUS
XpmxStartVirtualGamepad(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    VHF_CONFIG config;

    if (Context->VhfHandle != NULL) {
        return STATUS_SUCCESS;
    }

    VHF_CONFIG_INIT(
        &config,
        WdfDeviceWdmGetDeviceObject(Context->Device),
        (USHORT)sizeof(XpmxGamepadReportDescriptor),
        XpmxGamepadReportDescriptor
        );

    // These IDs are taken from the controller's observed SDP/PnP identity.
    config.VendorID = 0x1949;
    config.ProductID = 0x0402;
    config.VhfClientContext = Context;

    status = VhfCreate(&config, &Context->VhfHandle);
    if (!NT_SUCCESS(status)) {
        Context->VhfHandle = NULL;
        XpmxLogStatus("VhfCreate", status);
        return status;
    }

    status = VhfStart(Context->VhfHandle);
    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("VhfStart", status);
        VhfDelete(Context->VhfHandle, TRUE);
        Context->VhfHandle = NULL;
        return status;
    }

    Context->VhfStarted = TRUE;
    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] VHF gamepad child started: VID=0x1949 PID=0x0402 reportID=0x07 descriptorLength=%lu\n",
        (ULONG)sizeof(XpmxGamepadReportDescriptor)
        );

    return STATUS_SUCCESS;
}

static
VOID
XpmxStopVirtualGamepad(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    if (Context->VhfHandle == NULL) {
        Context->VhfStarted = FALSE;
        return;
    }

    // The Interrupt reader must be stopped before this function is called.
    VhfDelete(Context->VhfHandle, TRUE);
    Context->VhfHandle = NULL;
    Context->VhfStarted = FALSE;

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] VHF gamepad child deleted\n"
        );
}

static
HANDLE
XpmxGetChannelHandle(
    _Inout_ PCHANNEL_CONTEXT Channel
    )
{
    KIRQL oldIrql;
    HANDLE handle;

    KeAcquireSpinLock(&Channel->Lock, &oldIrql);
    handle = Channel->Handle;
    KeReleaseSpinLock(&Channel->Lock, oldIrql);

    return handle;
}

static
VOID
XpmxResetChannel(
    _Inout_ PCHANNEL_CONTEXT Channel
    )
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&Channel->Lock, &oldIrql);
    Channel->Handle = NULL;
    InterlockedExchange(&Channel->Disconnected, 0);
    KeReleaseSpinLock(&Channel->Lock, oldIrql);
}

static
VOID
XpmxRequestStopInterruptReader(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;

    if (InterlockedCompareExchange(&reader->Initialized, 0, 0) == 0) {
        return;
    }

    InterlockedExchange(&reader->Stopping, 1);

    if (reader->Request != NULL) {
        (VOID)WdfRequestCancelSentRequest(reader->Request);
    }
}

static
NTSTATUS
XpmxSendBrbSynchronously(
    _In_ PDEVICE_CONTEXT Context,
    _In_ PBRB Brb,
    _In_ ULONG BrbSize
    )
{
    NTSTATUS status;
    WDFREQUEST request = NULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_MEMORY_DESCRIPTOR otherArg1;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->Device;

    status = WdfRequestCreate(
        &attributes,
        Context->IoTarget,
        &request
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfRequestCreate(BRB)", status);
        return status;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
        &otherArg1,
        Brb,
        BrbSize
        );

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, 0);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        WDF_REL_TIMEOUT_IN_SEC(12)
        );

    status = WdfIoTargetSendInternalIoctlOthersSynchronously(
        Context->IoTarget,
        request,
        IOCTL_INTERNAL_BTH_SUBMIT_BRB,
        &otherArg1,
        NULL,
        NULL,
        &sendOptions,
        NULL
        );

    WdfObjectDelete(request);
    return status;
}

static
NTSTATUS
XpmxGetRemoteDeviceInfo(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    WDFREQUEST request = NULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_MEMORY_DESCRIPTOR output;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    Context->DeviceInfoReady = FALSE;
    RtlZeroMemory(&Context->RemoteDeviceInfo, sizeof(Context->RemoteDeviceInfo));

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->Device;

    status = WdfRequestCreate(
        &attributes,
        Context->IoTarget,
        &request
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfRequestCreate(GET_DEVINFO)", status);
        return status;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
        &output,
        &Context->RemoteDeviceInfo,
        sizeof(Context->RemoteDeviceInfo)
        );

    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, 0);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
        &sendOptions,
        WDF_REL_TIMEOUT_IN_SEC(12)
        );

    status = WdfIoTargetSendInternalIoctlSynchronously(
        Context->IoTarget,
        request,
        IOCTL_INTERNAL_BTHENUM_GET_DEVINFO,
        NULL,
        &output,
        &sendOptions,
        NULL
        );

    WdfObjectDelete(request);

    if (NT_SUCCESS(status)) {
        Context->DeviceInfoReady = TRUE;
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_INFO_LEVEL,
            "[XP-Mx210BL] Remote BD_ADDR=0x%012I64X\n",
            Context->RemoteDeviceInfo.address
            );
    } else {
        XpmxLogStatus("IOCTL_INTERNAL_BTHENUM_GET_DEVINFO", status);
    }

    return status;
}

static
VOID
XpmxL2capIndicationCallback(
    _In_ PVOID CallbackContext,
    _In_ INDICATION_CODE Indication,
    _In_ PINDICATION_PARAMETERS Parameters
    )
{
    PCHANNEL_CONTEXT channel = (PCHANNEL_CONTEXT)CallbackContext;
    PDEVICE_CONTEXT context = channel->DeviceContext;

    switch (Indication) {
    case IndicationAddReference:
    case IndicationReleaseReference:
        break;

    case IndicationRemoteDisconnect:
    {
        KIRQL oldIrql;
        HANDLE disconnectedHandle = Parameters->ConnectionHandle;
        BOOLEAN matched;

        // This driver is abandoning the channel after a remote disconnect.
        Parameters->Parameters.Disconnect.CloseNow = TRUE;

        KeAcquireSpinLock(&channel->Lock, &oldIrql);

        matched = (channel->Handle == NULL ||
                   channel->Handle == disconnectedHandle);

        if (matched) {
            channel->Handle = NULL;
            InterlockedExchange(&channel->Disconnected, 1);
        }

        KeReleaseSpinLock(&channel->Lock, oldIrql);

        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_WARNING_LEVEL,
            "[XP-Mx210BL] %s remote disconnect: reason=%u handle=%p matched=%u\n",
            channel->Name,
            (ULONG)Parameters->Parameters.Disconnect.Reason,
            disconnectedHandle,
            matched ? 1u : 0u
            );

        if (matched && channel->Psm == XPMX_INTERRUPT_PSM) {
            XpmxRequestStopInterruptReader(context);
        }
        break;
    }

    case IndicationRemoteConnect:
    case IndicationRemoteConfigRequest:
    case IndicationRemoteConfigResponse:
    case IndicationFreeExtraOptions:
    default:
        break;
    }
}

static
NTSTATUS
XpmxOpenL2capChannel(
    _Inout_ PDEVICE_CONTEXT Context,
    _Inout_ PCHANNEL_CONTEXT Channel
    )
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL* brb;
    KIRQL oldIrql;
    BOOLEAN disconnectedDuringOpen;

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context->ProfileInterface.BthAllocateBrb(
        BRB_L2CA_OPEN_CHANNEL,
        XPMX_POOL_TAG
        );

    if (brb == NULL) {
        XpmxLogStatus("BthAllocateBrb(OPEN_CHANNEL)", STATUS_INSUFFICIENT_RESOURCES);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->ProfileInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_OPEN_CHANNEL);

    brb->BtAddress = Context->RemoteDeviceInfo.address;
    brb->Psm = Channel->Psm;
    brb->ChannelFlags = CF_ROLE_EITHER;

    brb->ConfigOut.Flags = CFG_MTU;
    brb->ConfigOut.Mtu.Max = L2CAP_DEFAULT_MTU;
    brb->ConfigOut.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Preferred = L2CAP_DEFAULT_MTU;

    brb->ConfigIn.Flags = CFG_MTU;
    brb->ConfigIn.Mtu.Max = L2CAP_DEFAULT_MTU;
    brb->ConfigIn.Mtu.Min = L2CAP_MIN_MTU;
    brb->ConfigIn.Mtu.Preferred = L2CAP_DEFAULT_MTU;

    brb->CallbackFlags = CALLBACK_DISCONNECT;
    brb->Callback = XpmxL2capIndicationCallback;
    brb->CallbackContext = Channel;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(Context->Device);
    brb->IncomingQueueDepth = 10;

    status = XpmxSendBrbSynchronously(
        Context,
        (PBRB)brb,
        sizeof(*brb)
        );

    if (NT_SUCCESS(status)) {
        KeAcquireSpinLock(&Channel->Lock, &oldIrql);

        disconnectedDuringOpen =
            (InterlockedCompareExchange(&Channel->Disconnected, 0, 0) != 0);

        if (!disconnectedDuringOpen) {
            Channel->Handle = brb->ChannelHandle;
        }

        KeReleaseSpinLock(&Channel->Lock, oldIrql);

        if (disconnectedDuringOpen) {
            status = STATUS_DEVICE_NOT_CONNECTED;
            DbgPrintEx(
                DPFLTR_IHVDRIVER_ID,
                DPFLTR_WARNING_LEVEL,
                "[XP-Mx210BL] %s disconnected while its open request was completing\n",
                Channel->Name
                );
        } else {
            DbgPrintEx(
                DPFLTR_IHVDRIVER_ID,
                DPFLTR_INFO_LEVEL,
                "[XP-Mx210BL] %s opened: PSM=0x%04X handle=%p outMTU=%u inMTU=%u\n",
                Channel->Name,
                Channel->Psm,
                brb->ChannelHandle,
                brb->OutResults.Params.Mtu,
                brb->InResults.Params.Mtu
                );
        }
    } else {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "[XP-Mx210BL] %s failed: PSM=0x%04X NTSTATUS=0x%08X BRB_STATUS=0x%08X BTH_STATUS=0x%08X\n",
            Channel->Name,
            Channel->Psm,
            (ULONG)status,
            (ULONG)brb->Hdr.Status,
            (ULONG)brb->Hdr.BtStatus
            );
    }

    Context->ProfileInterface.BthFreeBrb((PBRB)brb);
    return status;
}

static
VOID
XpmxCloseL2capChannel(
    _Inout_ PDEVICE_CONTEXT Context,
    _Inout_ PCHANNEL_CONTEXT Channel
    )
{
    NTSTATUS status;
    HANDLE handle;
    struct _BRB_L2CA_CLOSE_CHANNEL* brb;

    if (!Context->ProfileInterfaceReady || !Context->DeviceInfoReady) {
        return;
    }

    handle = XpmxGetChannelHandle(Channel);
    if (handle == NULL) {
        return;
    }

    brb = (struct _BRB_L2CA_CLOSE_CHANNEL*)Context->ProfileInterface.BthAllocateBrb(
        BRB_L2CA_CLOSE_CHANNEL,
        XPMX_POOL_TAG
        );

    if (brb == NULL) {
        XpmxLogStatus("BthAllocateBrb(CLOSE_CHANNEL)", STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    Context->ProfileInterface.BthReuseBrb((PBRB)brb, BRB_L2CA_CLOSE_CHANNEL);

    brb->BtAddress = Context->RemoteDeviceInfo.address;
    brb->ChannelHandle = handle;

    status = XpmxSendBrbSynchronously(
        Context,
        (PBRB)brb,
        sizeof(*brb)
        );

    if (NT_SUCCESS(status)) {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_INFO_LEVEL,
            "[XP-Mx210BL] %s closed: handle=%p\n",
            Channel->Name,
            handle
            );
    } else {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_WARNING_LEVEL,
            "[XP-Mx210BL] %s close failed: NTSTATUS=0x%08X\n",
            Channel->Name,
            (ULONG)status
            );
    }

    Context->ProfileInterface.BthFreeBrb((PBRB)brb);

    {
        KIRQL oldIrql;
        KeAcquireSpinLock(&Channel->Lock, &oldIrql);

        if (Channel->Handle == handle) {
            Channel->Handle = NULL;
            InterlockedExchange(&Channel->Disconnected, 1);
        }

        KeReleaseSpinLock(&Channel->Lock, oldIrql);
    }
}

static
NTSTATUS
XpmxInitializeInterruptReader(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;
    size_t bufferSize;

    RtlZeroMemory(reader, sizeof(*reader));
    reader->DeviceContext = Context;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Context->Device;

    status = WdfRequestCreate(
        &attributes,
        Context->IoTarget,
        &reader->Request
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfRequestCreate(InterruptRead)", status);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = reader->Request;

    status = WdfMemoryCreate(
        &attributes,
        NonPagedPoolNx,
        XPMX_POOL_TAG,
        XPMX_READ_BUFFER_SIZE,
        &reader->BufferMemory,
        NULL
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfMemoryCreate(InterruptReadBuffer)", status);
        WdfObjectDelete(reader->Request);
        reader->Request = NULL;
        return status;
    }

    reader->Buffer = (PUCHAR)WdfMemoryGetBuffer(
        reader->BufferMemory,
        &bufferSize
        );

    if (reader->Buffer == NULL || bufferSize < XPMX_READ_BUFFER_SIZE) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        XpmxLogStatus("WdfMemoryGetBuffer(InterruptRead)", status);
        WdfObjectDelete(reader->Request);
        reader->Request = NULL;
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = reader->Request;

    status = WdfMemoryCreatePreallocated(
        &attributes,
        &reader->Brb,
        sizeof(reader->Brb),
        &reader->BrbMemory
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfMemoryCreatePreallocated(InterruptReadBRB)", status);
        WdfObjectDelete(reader->Request);
        reader->Request = NULL;
        return status;
    }

    KeInitializeDpc(
        &reader->ResubmitDpc,
        XpmxReadResubmitDpc,
        Context
        );

    KeInitializeEvent(&reader->StopEvent, NotificationEvent, TRUE);
    InterlockedExchange(&reader->Stopping, 0);
    InterlockedExchange(&reader->Running, 0);
    InterlockedExchange(&reader->Initialized, 1);

    return STATUS_SUCCESS;
}

static
NTSTATUS
XpmxSubmitInterruptRead(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    WDF_REQUEST_REUSE_PARAMS reuseParams;
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;
    HANDLE channelHandle;

    if (InterlockedCompareExchange(&reader->Stopping, 0, 0) != 0) {
        InterlockedExchange(&reader->Running, 0);
        KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
        return STATUS_CANCELLED;
    }

    channelHandle = XpmxGetChannelHandle(&Context->InterruptChannel);
    if (channelHandle == NULL) {
        status = STATUS_DEVICE_NOT_CONNECTED;
        goto failed;
    }

    Context->ProfileInterface.BthReuseBrb(
        (PBRB)&reader->Brb,
        BRB_L2CA_ACL_TRANSFER
        );

    WDF_REQUEST_REUSE_PARAMS_INIT(
        &reuseParams,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_UNSUCCESSFUL
        );

    status = WdfRequestReuse(reader->Request, &reuseParams);
    if (!NT_SUCCESS(status)) {
        goto failed;
    }

    reader->Brb.BtAddress = Context->RemoteDeviceInfo.address;
    reader->Brb.ChannelHandle = channelHandle;
    reader->Brb.TransferFlags =
        ACL_TRANSFER_DIRECTION_IN | ACL_SHORT_TRANSFER_OK;
    reader->Brb.BufferSize = XPMX_READ_BUFFER_SIZE;
    reader->Brb.Buffer = reader->Buffer;
    reader->Brb.BufferMDL = NULL;
    reader->Brb.Timeout = 0;
    reader->Brb.RemainingBufferSize = 0;

    status = WdfIoTargetFormatRequestForInternalIoctlOthers(
        Context->IoTarget,
        reader->Request,
        IOCTL_INTERNAL_BTH_SUBMIT_BRB,
        reader->BrbMemory,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
        );

    if (!NT_SUCCESS(status)) {
        goto failed;
    }

    WdfRequestSetCompletionRoutine(
        reader->Request,
        XpmxReadCompletion,
        reader
        );

    if (!WdfRequestSend(reader->Request, Context->IoTarget, NULL)) {
        status = WdfRequestGetStatus(reader->Request);
        goto failed;
    }

    return STATUS_SUCCESS;

failed:
    if (InterlockedCompareExchange(&reader->Stopping, 0, 0) == 0) {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "[XP-Mx210BL] Interrupt read submission failed: NTSTATUS=0x%08X\n",
            (ULONG)status
            );
    }

    InterlockedExchange(&reader->Running, 0);
    KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
    return status;
}

static
VOID
XpmxLogInterruptPacket(
    _In_reads_bytes_(Length) const UCHAR* Buffer,
    _In_ ULONG Length,
    _In_ ULONG RemainingBufferSize
    )
{
    static const CHAR hexDigits[] = "0123456789ABCDEF";
    CHAR hexBuffer[(XPMX_RX_LOG_BYTES * 3) + 1];
    ULONG i;
    ULONG bytesToLog = (Length < XPMX_RX_LOG_BYTES) ?
        Length : XPMX_RX_LOG_BYTES;
    ULONG outputOffset = 0;

    for (i = 0; i < bytesToLog; ++i) {
        if (i != 0) {
            hexBuffer[outputOffset++] = ' ';
        }

        hexBuffer[outputOffset++] = hexDigits[(Buffer[i] >> 4) & 0x0F];
        hexBuffer[outputOffset++] = hexDigits[Buffer[i] & 0x0F];
    }

    hexBuffer[outputOffset] = '\0';

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] HID Interrupt RX: length=%lu remaining=%lu data=%s\n",
        Length,
        RemainingBufferSize,
        hexBuffer
        );
}

static
VOID
XpmxReadCompletion(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT CompletionContext
    )
{
    PINTERRUPT_READER_CONTEXT reader =
        (PINTERRUPT_READER_CONTEXT)CompletionContext;
    NTSTATUS status = Params->IoStatus.Status;
    ULONG bytesRead;

    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Target);

    if (NT_SUCCESS(status)) {
        bytesRead = reader->Brb.BufferSize;

        if (bytesRead > XPMX_READ_BUFFER_SIZE) {
            bytesRead = XPMX_READ_BUFFER_SIZE;
        }

        if (bytesRead != 0) {
            XpmxLogInterruptPacket(
                reader->Buffer,
                bytesRead,
                reader->Brb.RemainingBufferSize
                );

            // Bluetooth HIDP DATA/INPUT: 0xA1, followed by the HID report ID.
            // VHF's HID_XFER_PACKET reportBuffer includes the report ID byte,
            // while reportId also identifies that same ID.
            if (bytesRead == XPMX_GAMEPAD_PACKET_LENGTH &&
                (bytesRead - 1) == XPMX_VHF_REPORT_BUFFER_LENGTH &&
                reader->Buffer[0] == XPMX_HIDP_INPUT &&
                reader->Buffer[1] == XPMX_GAMEPAD_REPORT_ID &&
                reader->DeviceContext->VhfHandle != NULL &&
                reader->DeviceContext->VhfStarted) {

                HID_XFER_PACKET transferPacket;
                NTSTATUS vhfStatus;

                transferPacket.reportId = XPMX_GAMEPAD_REPORT_ID;
                transferPacket.reportBuffer = reader->Buffer + 1;
                transferPacket.reportBufferLen = bytesRead - 1;

                vhfStatus = VhfReadReportSubmit(
                    reader->DeviceContext->VhfHandle,
                    &transferPacket
                    );

                if (!NT_SUCCESS(vhfStatus)) {
                    DbgPrintEx(
                        DPFLTR_IHVDRIVER_ID,
                        DPFLTR_WARNING_LEVEL,
                        "[XP-Mx210BL] VhfReadReportSubmit failed: NTSTATUS=0x%08X\n",
                        (ULONG)vhfStatus
                        );
                }
            }
        }

        if (InterlockedCompareExchange(&reader->Stopping, 0, 0) != 0) {
            InterlockedExchange(&reader->Running, 0);
            KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
            return;
        }

        if (!KeInsertQueueDpc(&reader->ResubmitDpc, NULL, NULL)) {
            DbgPrintEx(
                DPFLTR_IHVDRIVER_ID,
                DPFLTR_ERROR_LEVEL,
                "[XP-Mx210BL] Could not queue Interrupt read resubmit DPC\n"
                );
            InterlockedExchange(&reader->Running, 0);
            KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
        }
    } else {
        if (status != STATUS_CANCELLED &&
            InterlockedCompareExchange(&reader->Stopping, 0, 0) == 0) {
            DbgPrintEx(
                DPFLTR_IHVDRIVER_ID,
                DPFLTR_WARNING_LEVEL,
                "[XP-Mx210BL] Interrupt read completed: NTSTATUS=0x%08X\n",
                (ULONG)status
                );
        }

        InterlockedExchange(&reader->Running, 0);
        KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
    }
}

static
VOID
XpmxReadResubmitDpc(
    _In_ struct _KDPC *Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    )
{
    PDEVICE_CONTEXT context = (PDEVICE_CONTEXT)DeferredContext;
    PINTERRUPT_READER_CONTEXT reader = &context->InterruptReader;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (InterlockedCompareExchange(&reader->Stopping, 0, 0) != 0) {
        InterlockedExchange(&reader->Running, 0);
        KeSetEvent(&reader->StopEvent, IO_NO_INCREMENT, FALSE);
        return;
    }

    (VOID)XpmxSubmitInterruptRead(context);
}

static
NTSTATUS
XpmxStartInterruptReader(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    NTSTATUS status;
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;

    status = XpmxInitializeInterruptReader(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    InterlockedExchange(&reader->Stopping, 0);
    InterlockedExchange(&reader->Running, 1);
    KeClearEvent(&reader->StopEvent);

    status = XpmxSubmitInterruptRead(Context);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] Continuous HID Interrupt reader started\n"
        );

    return STATUS_SUCCESS;
}

static
VOID
XpmxStopInterruptReader(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;

    if (InterlockedCompareExchange(&reader->Initialized, 0, 0) == 0) {
        return;
    }

    InterlockedExchange(&reader->Stopping, 1);

    if (reader->Request != NULL) {
        (VOID)WdfRequestCancelSentRequest(reader->Request);
    }

    if (InterlockedCompareExchange(&reader->Running, 0, 0) != 0) {
        KeWaitForSingleObject(
            &reader->StopEvent,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
    }
}

static
VOID
XpmxUninitializeInterruptReader(
    _Inout_ PDEVICE_CONTEXT Context
    )
{
    PINTERRUPT_READER_CONTEXT reader = &Context->InterruptReader;

    if (InterlockedCompareExchange(&reader->Initialized, 0, 0) == 0) {
        return;
    }

    if (reader->Request != NULL) {
        WdfObjectDelete(reader->Request);
    }

    RtlZeroMemory(reader, sizeof(*reader));
}

NTSTATUS
XpmxEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    PDEVICE_CONTEXT context;

    UNREFERENCED_PARAMETER(Driver);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDeviceSelfManagedIoInit = XpmxEvtSelfManagedIoInit;
    pnpCallbacks.EvtDeviceSelfManagedIoCleanup = XpmxEvtSelfManagedIoCleanup;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("WdfDeviceCreate", status);
        return status;
    }

    context = GetDeviceContext(device);
    RtlZeroMemory(context, sizeof(*context));

    context->Device = device;
    context->IoTarget = WdfDeviceGetIoTarget(device);

    context->ControlChannel.DeviceContext = context;
    context->ControlChannel.Psm = XPMX_CONTROL_PSM;
    context->ControlChannel.Name = "HID Control";
    KeInitializeSpinLock(&context->ControlChannel.Lock);

    context->InterruptChannel.DeviceContext = context;
    context->InterruptChannel.Psm = XPMX_INTERRUPT_PSM;
    context->InterruptChannel.Name = "HID Interrupt";
    KeInitializeSpinLock(&context->InterruptChannel.Lock);

    status = WdfFdoQueryForInterface(
        device,
        &GUID_BTHDDI_PROFILE_DRIVER_INTERFACE,
        (PINTERFACE)&context->ProfileInterface,
        sizeof(context->ProfileInterface),
        BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,
        NULL
        );

    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("Query GUID_BTHDDI_PROFILE_DRIVER_INTERFACE", status);
        return status;
    }

    context->ProfileInterfaceReady = TRUE;

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] Profile driver attached to the controller HID service node\n"
        );

    return STATUS_SUCCESS;
}

NTSTATUS
XpmxEvtSelfManagedIoInit(
    _In_ WDFDEVICE Device
    )
{
    NTSTATUS status;
    PDEVICE_CONTEXT context = GetDeviceContext(Device);

    XpmxResetChannel(&context->ControlChannel);
    XpmxResetChannel(&context->InterruptChannel);
    context->DeviceInfoReady = FALSE;

    // VHF is created and started for each PnP start cycle. This keeps the
    // virtual HID child lifecycle paired with this Bluetooth service node.
    status = XpmxStartVirtualGamepad(context);
    if (!NT_SUCCESS(status)) {
        // Keep the Bluetooth transport available for diagnostics even if
        // virtual-HID initialization fails.
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_WARNING_LEVEL,
            "[XP-Mx210BL] VHF initialization failed; Bluetooth receive probe will continue\n"
            );
    }

    status = XpmxGetRemoteDeviceInfo(context);
    if (!NT_SUCCESS(status)) {
        // Keep the PnP node started so diagnostics remain available.
        return STATUS_SUCCESS;
    }

    status = XpmxOpenL2capChannel(context, &context->ControlChannel);
    if (!NT_SUCCESS(status)) {
        return STATUS_SUCCESS;
    }

    status = XpmxOpenL2capChannel(context, &context->InterruptChannel);
    if (!NT_SUCCESS(status)) {
        XpmxCloseL2capChannel(context, &context->ControlChannel);
        return STATUS_SUCCESS;
    }

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] Both HID L2CAP channels are open. Probe milestone passed.\n"
        );

    status = XpmxStartInterruptReader(context);
    if (!NT_SUCCESS(status)) {
        XpmxLogStatus("Start continuous HID Interrupt reader", status);
    }

    return STATUS_SUCCESS;
}

VOID
XpmxEvtSelfManagedIoCleanup(
    _In_ WDFDEVICE Device
    )
{
    PDEVICE_CONTEXT context = GetDeviceContext(Device);

    // Stop and drain the pending read before deleting VHF or closing channels.
    XpmxStopInterruptReader(context);

    // VhfDelete must not race with a new VhfReadReportSubmit call.
    XpmxStopVirtualGamepad(context);

    // Close in reverse-open order. Handles are invalidated by the channel
    // callback as soon as a remote disconnect is observed.
    XpmxCloseL2capChannel(context, &context->InterruptChannel);
    XpmxCloseL2capChannel(context, &context->ControlChannel);

    // Keep the request alive until both channel-close operations have
    // completed, so a late disconnect callback cannot touch a deleted request.
    XpmxUninitializeInterruptReader(context);
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(&config, XpmxEvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
        );

    XpmxLogStatus("WdfDriverCreate", status);
    return status;
}
