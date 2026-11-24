#include <ntddk.h>
#include <wdf.h>
#include <bthdef.h>
#include <bthioctl.h>
#include <bthddi.h>

#define XPMX_POOL_TAG 'xMpX'
#define XPMX_CONTROL_PSM 0x0011
#define XPMX_INTERRUPT_PSM 0x0013

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFIOTARGET IoTarget;
    BTH_PROFILE_DRIVER_INTERFACE ProfileInterface;
    BTH_DEVICE_INFO RemoteDeviceInfo;
    HANDLE ControlChannel;
    HANDLE InterruptChannel;
    BOOLEAN ProfileInterfaceReady;
    BOOLEAN DeviceInfoReady;
    volatile LONG RemoteDisconnected;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD XpmxEvtDeviceAdd;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT XpmxEvtSelfManagedIoInit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP XpmxEvtSelfManagedIoCleanup;

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

    status = WdfIoTargetSendInternalIoctlSynchronously(
        Context->IoTarget,
        request,
        IOCTL_INTERNAL_BTHENUM_GET_DEVINFO,
        NULL,
        &output,
        NULL,
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
    _In_ PINDICATION_PARAMETERS_ENHANCED Parameters
    )
{
    PDEVICE_CONTEXT context = (PDEVICE_CONTEXT)CallbackContext;

    UNREFERENCED_PARAMETER(Parameters);

    switch (Indication) {
    case IndicationAddReference:
    case IndicationReleaseReference:
        break;

    case IndicationRemoteDisconnect:
        InterlockedExchange(&context->RemoteDisconnected, 1);
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_WARNING_LEVEL,
            "[XP-Mx210BL] Remote L2CAP disconnect indication received\n"
            );
        break;

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
    _In_ USHORT Psm,
    _Out_ PHANDLE ChannelHandle,
    _In_z_ PCSTR ChannelName
    )
{
    NTSTATUS status;
    struct _BRB_L2CA_OPEN_CHANNEL* brb;

    *ChannelHandle = NULL;

    brb = (struct _BRB_L2CA_OPEN_CHANNEL*)Context->ProfileInterface.BthAllocateBrb(
        BRB_L2CA_OPEN_CHANNEL,
        XPMX_POOL_TAG
        );

    if (brb == NULL) {
        XpmxLogStatus("BthAllocateBrb(OPEN_CHANNEL)", STATUS_INSUFFICIENT_RESOURCES);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    brb->BtAddress = Context->RemoteDeviceInfo.address;
    brb->Psm = Psm;
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
    brb->CallbackContext = Context;
    brb->ReferenceObject = (PVOID)WdfDeviceWdmGetDeviceObject(Context->Device);
    brb->IncomingQueueDepth = 10;

    status = XpmxSendBrbSynchronously(
        Context,
        (PBRB)brb,
        sizeof(*brb)
        );

    if (NT_SUCCESS(status)) {
        *ChannelHandle = brb->ChannelHandle;
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_INFO_LEVEL,
            "[XP-Mx210BL] %s opened: PSM=0x%04X handle=%p outMTU=%u inMTU=%u\n",
            ChannelName,
            Psm,
            brb->ChannelHandle,
            brb->OutResults.Params.Mtu,
            brb->InResults.Params.Mtu
            );
    } else {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_ERROR_LEVEL,
            "[XP-Mx210BL] %s failed: PSM=0x%04X NTSTATUS=0x%08X BRB_STATUS=0x%08X BTH_STATUS=0x%08X\n",
            ChannelName,
            Psm,
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
    _Inout_ PHANDLE ChannelHandle,
    _In_z_ PCSTR ChannelName
    )
{
    NTSTATUS status;
    struct _BRB_L2CA_CLOSE_CHANNEL* brb;

    if (*ChannelHandle == NULL || !Context->ProfileInterfaceReady ||
        !Context->DeviceInfoReady) {
        return;
    }

    brb = (struct _BRB_L2CA_CLOSE_CHANNEL*)Context->ProfileInterface.BthAllocateBrb(
        BRB_L2CA_CLOSE_CHANNEL,
        XPMX_POOL_TAG
        );

    if (brb == NULL) {
        XpmxLogStatus("BthAllocateBrb(CLOSE_CHANNEL)", STATUS_INSUFFICIENT_RESOURCES);
        *ChannelHandle = NULL;
        return;
    }

    brb->BtAddress = Context->RemoteDeviceInfo.address;
    brb->ChannelHandle = *ChannelHandle;

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
            ChannelName,
            *ChannelHandle
            );
    } else {
        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            DPFLTR_WARNING_LEVEL,
            "[XP-Mx210BL] %s close failed: NTSTATUS=0x%08X\n",
            ChannelName,
            (ULONG)status
            );
    }

    Context->ProfileInterface.BthFreeBrb((PBRB)brb);
    *ChannelHandle = NULL;
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

    InterlockedExchange(&context->RemoteDisconnected, 0);

    status = XpmxGetRemoteDeviceInfo(context);
    if (!NT_SUCCESS(status)) {
        // Keep the PnP node started so diagnostics remain available.
        // A device restart with the controller awake can retry the probe.
        return STATUS_SUCCESS;
    }

    status = XpmxOpenL2capChannel(
        context,
        XPMX_CONTROL_PSM,
        &context->ControlChannel,
        "HID Control"
        );

    if (!NT_SUCCESS(status)) {
        return STATUS_SUCCESS;
    }

    status = XpmxOpenL2capChannel(
        context,
        XPMX_INTERRUPT_PSM,
        &context->InterruptChannel,
        "HID Interrupt"
        );

    if (!NT_SUCCESS(status)) {
        XpmxCloseL2capChannel(context, &context->ControlChannel, "HID Control");
        return STATUS_SUCCESS;
    }

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "[XP-Mx210BL] Both HID L2CAP channels are open. Probe milestone passed.\n"
        );

    return STATUS_SUCCESS;
}

VOID
XpmxEvtSelfManagedIoCleanup(
    _In_ WDFDEVICE Device
    )
{
    PDEVICE_CONTEXT context = GetDeviceContext(Device);

    // Close in reverse-open order.
    XpmxCloseL2capChannel(context, &context->InterruptChannel, "HID Interrupt");
    XpmxCloseL2capChannel(context, &context->ControlChannel, "HID Control");
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
