# Architecture

## Goal

Provide a native Windows gamepad for the XP-7018L / XP-Mx210BL over Bluetooth Classic HID, using the laptop's existing Intel Bluetooth radio.

No Android bridge, no iCade keyboard mapping, no permanently attached USB Bluetooth dongle.

## Device binding

Windows' Bluetooth enumerator creates a PnP node for each remote service. Microsoft's Bluetooth INF model supports hardware IDs formed from the remote service UUID and VID/PID.

The HID service GUID is:

~~~text
00001124-0000-1000-8000-00805F9B34FB
~~~

The controller's observed node contains:

~~~text
BTHENUM\{00001124-0000-1000-8000-00805F9B34FB}_VID&00021949_PID&0402
~~~

The intended INF match is this specific service + VID + PID hardware ID. The purpose is to outrank the generic inbox HID Bluetooth profile match for this controller only.

Do not match the generic BTHENUM HID compatible ID unless deliberately testing broadly. Do not bind to or replace the Intel Bluetooth radio driver.

## Bluetooth transport and connection lifecycle

The profile driver attaches to the controller's HID service PDO. It obtains the Bluetooth profile-driver interface (GUID_BTHDDI_PROFILE_DRIVER_INTERFACE) and uses the default I/O target to send IOCTL_INTERNAL_BTH_SUBMIT_BRB requests to BthPort.sys.

A normal connection attempt:

1. Queries the paired remote-device information through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
2. Logs the remote address, BTH_DEVICE_INFO.flags, and whether BDIF_CONNECTED is set.
3. Opens HID Control L2CAP PSM 0x0011 with BRB_L2CA_OPEN_CHANNEL.
4. Opens HID Interrupt L2CAP PSM 0x0013 with BRB_L2CA_OPEN_CHANNEL.
5. Starts a pending inbound BRB_L2CA_ACL_TRANSFER request on the Interrupt channel.
6. After each successful read, the completion routine queues a DPC that re-submits the read.

The profile callback is channel-specific. When a remote disconnect is indicated, it invalidates the affected channel handle, sets the CloseNow response so the Bluetooth stack can finish tearing down that channel, and cancels the pending Interrupt read when appropriate. A lost HID channel makes the complete gamepad session unusable, so a PASSIVE_LEVEL work item stops the reader, closes any remaining channel, deletes the VHF child, and requests a full Bluetooth ACL disconnect. A later explicit Connect request creates a fresh VHF child after both channels open.

There are exactly two connection triggers in the current design:

- **PnP start:** one initial connection attempt when Windows starts the Bluetooth HID service device.
- **Explicit user request:** one connection attempt after the user clicks Connect in the companion tool.

There is no periodic retry timer, polling loop, or autonomous retry while the controller is unavailable. If a connection attempt fails, the user can power on the controller in HOME+X mode and click Connect again.

### Manual control interface

The driver registers a private PnP device interface:

- GUID: {7F44E8AF-4C2B-4C86-8EF5-CF49E2B5DA18}
- IOCTL: CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS) (0x22E000)
- Access: LocalSystem and elevated Administrators only.
- Request payload: none.

The sequential KMDF queue runs its device-control callback at PASSIVE_LEVEL. A WDF wait lock serializes PnP start, manual connect, and PnP cleanup. The connect routine first stops and drains any previous Interrupt reader, cleans up any still-valid channel, refreshes BTH_DEVICE_INFO, opens both L2CAP channels, and starts a fresh reader. If either channel or the reader fails to start, it cleans up the partial session and returns the failure to the caller.

The companion tools/XpMx210BL-Connect.ps1 enumerates the device interface through the Windows Configuration Manager API and sends the IOCTL when its Connect button is clicked. The script elevates itself because the interface is restricted to administrators. It makes no background requests.

### Why there is no Windows Settings Connect button

The Windows Bluetooth profile-driver model lets a profile driver initiate a remote L2CAP connection with BRB_L2CA_OPEN_CHANNEL. The documented BluetoothSetServiceState API enables or disables a remote service and installs or removes its associated profile driver; it is not a connect/disconnect API. We have not found a documented public driver contract for adding a custom Connect action to the Windows Bluetooth Settings or Quick Settings flyout. This project therefore supplies its own explicit Connect window instead of modifying Windows' Bluetooth UI.

The controller's captured SDP record indicates fixed PSMs; a new SDP query is not necessary for this implementation. Basic L2CAP mode is used, with the observed 672-byte MTU and an incoming queue depth of 10.

## HIDP input receive

The Android HOME+X SDP record contains a 208-byte HID Report Descriptor with three top-level collections: Consumer Control (report ID 0x02), Mouse (0x06), and Game Pad (0x07).

The controller's Interrupt channel sends 12-byte gamepad packets:

- Byte 0: HIDP DATA/INPUT header 0xA1.
- Byte 1: Report ID 0x07.
- Bytes 2–11: the ten-byte Game Pad report payload.

The Game Pad payload fields, as declared by the descriptor, are four 8-bit axes (X, Y, Z, Rz), a 4-bit hat plus constant padding, a 16-bit button field, 8-bit Accelerator and Brake values, and constant padding. Exact descriptor bytes and the packet layout are preserved in protocol/.

The Stage 2 capture contained 229 valid-length packets with prefix A1 07 and 150 distinct byte patterns while the controls were exercised. The VHF virtual gamepad has since been tested successfully in joy.cpl, Terraria, and Dead Cells.

## VHF virtual gamepad

The profile driver has a VHF source-device path:

1. During EvtDeviceSelfManagedIoInit, initialize VHF_CONFIG with the source device object and the 97-byte Game Pad-only report descriptor.
2. Call VhfCreate and VhfStart to create and start the virtual HID child.
3. For each 12-byte Interrupt packet, accept only HIDP DATA/INPUT (0xA1) with Report ID 0x07.
4. Strip the HIDP packet header byte 0xA1. Submit the remaining 11 bytes (Report ID 0x07 plus ten payload bytes) through HID_XFER_PACKET and VhfReadReportSubmit.
5. Use VHF's default input-report buffering. No EvtVhfReadyForNextReadReport callback is registered, so the receive buffer can be reused after VhfReadReportSubmit returns.
6. During EvtDeviceSelfManagedIoCleanup, stop/drain the Interrupt reader before calling VhfDelete synchronously.

The 97-byte VHF descriptor is a byte-for-byte slice of the Game Pad top-level Application Collection from the original 208-byte descriptor. The Consumer Control and Mouse collections are intentionally omitted so the virtual child exposes only a gamepad.

The INF registers VHF as a lower filter for the Bluetooth profile source device, and the project links VhfKm.lib. This VHF path has been verified on the target laptop.

No HIDP Control-channel SET_PROTOCOL/GET_REPORT/SET_REPORT transactions or HID output/rumble reports are implemented. The physical controller tested so far has no vibration motors, so haptics are outside the current scope.

## Why not patch hidbth.sys?

Patching a Microsoft inbox binary would be fragile, difficult to maintain, and would bypass the supported profile-driver model. An INF-selected profile driver is a cleaner experiment: Windows binds the custom driver to the specific BTHENUM service instance, while the Intel radio and Microsoft Bluetooth port stack remain in place.

## Safety and rollback principles

- Development stays on branch work/l2cap-probe.
- Do not install a new prototype revision until it builds cleanly and the generated INF/CAT/SYS package has been checked.
- Never replace the Intel radio driver.
- The controller-specific INF must contain the exact HID service + VID + PID match.
- Keep an explicit uninstall/rollback procedure for each published oemNN.inf package.
- Use a separate test machine or a recoverable Windows installation where possible; a kernel driver bug can crash Windows.

## Setup class and virtual HID separation

The INF intentionally uses the Windows Bluetooth setup class, following Microsoft's Bluetooth profile-driver installation model. The current inbox HidBth package classifies its device node as HIDClass because it is itself a HID transport driver; this prototype instead owns the Bluetooth profile service and does not expose raw HID transport callbacks on that PDO. The gamepad applications consume a separate VHF-created HID child, with the HID class stack above VHF.
