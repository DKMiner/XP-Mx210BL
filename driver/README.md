# Stage 3 driver: VHF gamepad bridge

This is a test-signed development prototype, not a production release.

## Current implementation

When the XP-Mx210BL Bluetooth HID service PDO starts, the driver:

1. Creates and starts a VHF virtual HID child using the captured Game Pad-only report descriptor.
2. Queries GUID_BTHDDI_PROFILE_DRIVER_INTERFACE.
3. Gets the paired remote device address through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
4. Opens HID Control L2CAP PSM 0x0011 and HID Interrupt L2CAP PSM 0x0013.
5. Starts one pending inbound BRB_L2CA_ACL_TRANSFER read on the Interrupt channel and re-submits it after successful completion.
6. Validates 12-byte HIDP DATA/INPUT packets with Report ID 0x07.
7. Removes only the leading HIDP packet header (0xA1) and submits the remaining 11-byte report (ID byte plus ten payload bytes) to VHF through HID_XFER_PACKET / VhfReadReportSubmit.
8. Uses VHF's default report buffering, so the receive buffer can be reused after VhfReadReportSubmit returns.
9. Stops the pending read before deleting the VHF child and closing any still-valid L2CAP channels.

The VHF child uses VID 0x1949 and PID 0x0402, matching the observed controller SDP/PnP identity. Its descriptor contains only the Game Pad top-level collection (Report ID 0x07), not the Consumer Control or Mouse collections from the full physical descriptor.

## Known limitations

- The Stage 3 VHF source integration has not yet been built or hardware-tested on the target laptop.
- The driver does not implement HIDP Control-channel SET_PROTOCOL, GET_REPORT, or SET_REPORT transactions.
- No HID output reports, rumble, or force feedback are implemented.
- Automatic channel reconnection after a remote disconnect is not implemented. Restarting this Bluetooth HID service device retries opening both channels.
- Sleep/hibernate and D0 power-transition recovery are not implemented.

## Build and package

Open XpMx210BL.vcxproj in Visual Studio 2022 with the WDK integration installed. Select Debug | x64 and rebuild.

The project links VhfKm.lib. The INF adds the in-box VHF service as a lower filter using the same LowerFilters pattern as Microsoft's HIDInjector VHF sample. Verify the generated INF includes that entry and that the catalog signature verifies before installation.

## Test

1. Build Debug | x64.
2. Regenerate the package using the WDK/Inf2Cat target; this machine uses Inf2Cat local time.
3. Verify the generated catalog with SignTool /pa.
4. Start DebugView as administrator with Capture Events and Capture Kernel enabled.
5. Wake the controller in HOME+X mode.
6. Install the new package using pnputil /add-driver <package-inf> /install.
7. Confirm the Bluetooth service node is Started and bound to the newly published oemNN.inf.
8. Look for the VHF gamepad child and the log message VHF gamepad child started.
9. Run joy.cpl and test the axes, hat, and buttons.

The Stage 2 receive-probe package already proved that packets arrive. The Stage 3 package is the first revision that forwards them to VHF; do not expect a usable Windows gamepad until this revision builds, installs, and the VHF child starts.

## Logging

The driver emits one compact DebugView line per received Interrupt packet, plus messages for VhfCreate, VhfStart, VhfReadReportSubmit, and VhfDelete. Keep the capture short if you are pressing buttons continuously.

## Rollback

Record the actual published oemNN.inf name returned by PnPUtil. To remove only the new prototype package, use:

pnputil /delete-driver oemNN.inf /uninstall

Then ask PnP to rescan:

pnputil /scan-devices

Do not use /force. Keep the previously working receive-probe package available until the VHF child has been verified.