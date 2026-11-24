# Stage 1 driver: L2CAP probe

This is a development prototype, not an installable release.

## What it currently does

When the XP-Mx210BL's Bluetooth HID service PDO starts, the driver:

1. Queries GUID_BTHDDI_PROFILE_DRIVER_INTERFACE.
2. Gets the paired remote device address through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
3. Sends BRB_L2CA_OPEN_CHANNEL to HID Control PSM 0x0011.
4. Sends BRB_L2CA_OPEN_CHANNEL to HID Interrupt PSM 0x0013.
5. Logs the request status, Bluetooth BRB status, negotiated MTUs and channel handles.
6. Closes successfully opened channels during orderly device cleanup.

The initial probe uses basic L2CAP mode and an incoming queue depth of 10. BRB submissions have a 12-second request timeout.

## What it does not do yet

- It does not receive ACL transfer data from the Interrupt channel.
- It does not implement HIDP SET_PROTOCOL / GET_REPORT / SET_REPORT handling.
- It does not parse the gamepad report.
- It does not create a VHF virtual HID device.
- It does not retry the connection automatically if the controller is powered off when the service node starts.

For the first hardware test, the controller should be awake in HOME+X mode when the device node is started. If the node has already started while the controller was off, restart only this Bluetooth HID service device after the controller is awake; do not remove the pairing.

## Build

Open XpMx210BL.vcxproj in Visual Studio 2022 with the WDK integration installed. Select Debug | x64 and build.

The project has not yet been built in this environment. Resolve all compiler and WDK warnings before considering installation.

## Logging

The prototype uses DbgPrintEx with the [XP-Mx210BL] prefix. Capture kernel debug output with a kernel debugger or a trusted debug-output viewer configured to capture kernel messages. The expected messages identify:

- profile interface query result
- remote BD_ADDR retrieval result
- Control PSM open result
- Interrupt PSM open result
- channel-close result

Record the complete output, not just the last line.

## Installation safety

Do not use this INF yet. Before installation, the project needs a WDK build, a review of the generated INF/catalog, and a separate rollback procedure. A kernel-mode driver can crash Windows; test in a recoverable environment and retain a way to restore the inbox profile driver.
