# XP-Mx210BL driver: VHF gamepad and manual connection

This is a test-signed development prototype, not a production release. The VHF gamepad path is hardware-tested on the target laptop, including Terraria and Dead Cells. The manual Connect interface in the latest branch still needs to be built and tested.

## Current implementation

When the XP-Mx210BL Bluetooth HID service PDO starts, the driver:

1. Creates and starts a VHF virtual HID child using the captured Game Pad-only report descriptor.
2. Queries GUID_BTHDDI_PROFILE_DRIVER_INTERFACE.
3. Gets the paired remote device address and connection flags through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
4. Makes one initial attempt to open HID Control L2CAP PSM 0x0011 and HID Interrupt L2CAP PSM 0x0013.
5. Starts one pending inbound BRB_L2CA_ACL_TRANSFER read on the Interrupt channel and re-submits it after successful completion.
6. Validates 12-byte HIDP DATA/INPUT packets with Report ID 0x07.
7. Removes only the leading HIDP packet header (0xA1) and submits the remaining 11-byte report (ID byte plus ten payload bytes) to VHF through HID_XFER_PACKET / VhfReadReportSubmit.
8. Keeps the VHF gamepad alive across ordinary remote L2CAP disconnects.
9. Exposes an administrator-only device interface and a one-shot Connect IOCTL for explicit user-triggered reconnection.
10. Stops and drains the Interrupt reader before deleting VHF or closing channels during PnP cleanup.

The VHF child uses VID 0x1949 and PID 0x0402. Its descriptor contains only the Game Pad top-level collection (Report ID 0x07), not the Consumer Control or Mouse collections from the full physical descriptor.

## Connection policy

There is no periodic retry timer and no background polling.

- PnP device start performs one normal initial connection attempt.
- A user can make another single attempt by clicking Connect in tools/XpMx210BL-Connect.ps1.
- Each attempt refreshes remote device information, cleans up any stale reader/channel state, opens both L2CAP channels, and starts a fresh reader.
- The Control IOCTL queue is sequential and configured for PASSIVE_LEVEL callbacks. A WDF wait lock serializes connection operations with PnP cleanup.
- The device interface is restricted to LocalSystem and elevated Administrators.

We have not found a documented extension point for a custom HID profile driver to add an arbitrary Connect button to Windows Bluetooth Settings or Quick Settings. The project therefore uses a small companion Connect window.

## Known limitations

- The manual-connect source changes have not yet been built or hardware-tested on the target laptop.
- The companion Connect window is not integrated into Windows Bluetooth Settings or Quick Settings.
- The driver does not implement HIDP Control-channel SET_PROTOCOL, GET_REPORT, or SET_REPORT transactions.
- Haptic/rumble output is out of scope; this controller's tested hardware has no vibration motors.
- Sleep/hibernate and full Bluetooth radio power-transition recovery still need separate testing.

## Build and package

Open XpMx210BL.vcxproj in Visual Studio 2022 with the WDK integration installed. Select Debug | x64 and rebuild.

The project links VhfKm.lib. The INF registers the in-box VHF service as a lower filter in [Install.NT.HW] via [Install.NT.HW.AddReg]. Verify the generated INF and catalog signature before installation.

## Test

1. Build Debug | x64.
2. Regenerate the package using the WDK/Inf2Cat target; this machine uses Inf2Cat local time.
3. Verify the generated catalog with SignTool /pa.
4. Start DebugView as administrator with Capture Events and Capture Kernel enabled.
5. Install the newly built package using pnputil /add-driver <package-inf> /install.
6. Confirm the Bluetooth service node is Started and bound to the newly published oemNN.inf.
7. Confirm VHF gamepad child startup and verify input in joy.cpl.
8. Turn on the controller in HOME+X mode and click Connect in tools/XpMx210BL-Connect.ps1.
9. Verify both channels open and Interrupt reports resume.

## Logging

The driver emits compact HID Interrupt packet lines plus messages for VhfCreate, VhfStart, VhfReadReportSubmit, VhfDelete, connection-state flags, and manual connection attempts. Keep DebugView captures short while exercising controls.

## Rollback

Record the actual published oemNN.inf name returned by PnPUtil. To remove only the prototype package:

~~~powershell
pnputil /delete-driver oemNN.inf /uninstall
pnputil /scan-devices
~~~

Do not use /force. Keep the previous known-working VHF package available until the manual-connect revision passes testing.
