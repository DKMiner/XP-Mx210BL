# Stage 2 driver: L2CAP receive probe

This is a development prototype, not a production release.

## What it currently does

When the XP-Mx210BL's Bluetooth HID service PDO starts, the driver:

1. Queries GUID_BTHDDI_PROFILE_DRIVER_INTERFACE.
2. Gets the paired remote device address through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
3. Opens HID Control L2CAP PSM 0x0011 with BRB_L2CA_OPEN_CHANNEL.
4. Opens HID Interrupt L2CAP PSM 0x0013 with BRB_L2CA_OPEN_CHANNEL.
5. Starts one pending inbound BRB_L2CA_ACL_TRANSFER read on the Interrupt channel.
6. Re-submits the read after each successful completion and logs the first 16 bytes of each received transfer.
7. Tracks Control and Interrupt channel handles separately. A remote disconnect invalidates that channel handle, requests immediate channel rundown, and cancels the pending Interrupt read.
8. Stops the reader before orderly channel cleanup.

The initial channels use Basic L2CAP mode, an incoming queue depth of 10, and MTU negotiation up to the observed 672-byte channel MTU. The remote-device-info query and synchronous open/close BRBs have 12-second request timeouts. The continuous read has no BRB timeout; it remains pending until data arrives or the request is canceled.

## What it does not do yet

- It does not implement HIDP SET_PROTOCOL, GET_REPORT, or SET_REPORT transactions on the Control channel.
- It does not validate HIDP framing or parse the controller's report descriptor.
- It does not create a VHF virtual HID device.
- It does not automatically reopen channels after a remote disconnect while the PnP node remains started. Restarting only this Bluetooth HID service device currently retries the connection.
- It does not yet manage channel teardown/reopen across sleep, hibernate, or D0 power transitions.

For testing, pair the controller as usual and wake it in HOME+X mode before starting/restarting the Bluetooth HID service node. Do not remove the pairing to retry the transport. Press buttons or move the controls briefly while DebugView is capturing kernel output; look for [XP-Mx210BL] HID Interrupt RX lines.

## Build

Open XpMx210BL.vcxproj in Visual Studio 2022 with the WDK integration installed. Select Debug | x64 and build.

The Stage 1 transport probe was built and run on the target machine. The Stage 2 receive-reader revision has not yet been built or hardware-tested. Resolve all compiler and WDK warnings before installing the revised package.

## Logging

The driver uses DbgPrintEx with the [XP-Mx210BL] prefix. Capture kernel debug output with DebugView running as administrator and Kernel Capture enabled. The registry component filter should include informational messages (IHVDRIVER=0xF).

Expected receive messages identify:
- Control and Interrupt channel open results and negotiated MTUs
- Continuous Interrupt reader startup
- Individual Interrupt read completions, including transfer length, remaining buffer space, and the first 16 bytes
- Remote disconnect reason and channel identity
- Read cancellation or failure during teardown

Keep captures short while the controller is sending reports because the reader logs each received transfer. Save the complete capture, not only the last line.

## Installation safety

This is still a test-signed kernel-mode prototype. Review the generated INF/CAT/SYS package before installing each revision. Retain the published oemNN.inf name for rollback; remove only the prototype package and do not force-delete unrelated Bluetooth packages.