# Stage 3 driver: VHF gamepad bridge

This is a test-signed development prototype, not a production release.

## Current implementation

The source driver opens the controller's HID Control and Interrupt L2CAP channels, receives Interrupt-channel packets using a pending BRB_L2CA_ACL_TRANSFER request, and re-submits the read after each completion.

The Stage 3 revision also:

- Creates and starts a VHF child using a 97-byte descriptor containing only the physical controller's Game Pad top-level collection.
- Accepts only 12-byte HIDP DATA/INPUT packets with prefix A1 07.
- Removes the HIDP header byte A1 and sends the remaining 11-byte report (Report ID 07 plus the ten-byte payload) using HID_XFER_PACKET and VhfReadReportSubmit.
- Uses VHF's default input-report buffering.
- Stops and drains the Bluetooth reader before deleting the VHF child during PnP cleanup.
- Logs received packets as one compact DebugView line each rather than one line per byte.

The VHF child uses the controller's observed VID 0x1949 / PID 0x0402. The full 208-byte source descriptor and 97-byte Game Pad-only descriptor are preserved in protocol/.

## Known limitations

- The Stage 3 VHF source integration is committed but has not yet been built or hardware-tested on the target laptop.
- No HIDP Control-channel transactions are implemented.
- No output reports, rumble, or force feedback are supported.
- Automatic reconnect after a remote disconnect is not implemented.
- Sleep/hibernate and D0 power-transition recovery are not implemented.

## Build

1. Open driver/XpMx210BL.vcxproj in Visual Studio 2022.
2. Select Debug | x64.
3. Rebuild the project.
4. Inspect the build output for all compiler/linker warnings and errors.

The project must link VhfKm.lib. The INF's [Install.NT.Services] section must contain the VHF lower-filter registration:

HKR,,"LowerFilters",0x00010000,"vhf"

This is the lower-filter pattern used by Microsoft's HIDInjector VHF sample. Do not install the package if this line is missing from the generated INF.

## Package verification

Use the generated package folder:

$pkg = 'D:\Projects\XP-Mx210BL\driver\x64\Debug\XpMx210BL'

Check the time-stamped DriverVer line (StampInf uses local time on this machine):

Select-String "$pkg\XpMx210BL.inf" -Pattern '^\s*DriverVer\s*='

Check that the VHF lower filter survived INF generation:

Select-String "$pkg\XpMx210BL.inf" -Pattern 'LowerFilters.*vhf'

Verify the package catalog:

& $signtool verify /pa /v /c "$pkg\xpmx210bl.cat" "$pkg\XpMx210BL.inf"

Expected: SignTool reports Successfully verified with no errors.

## First VHF hardware test

1. Start DebugView as administrator with Capture Events and Capture Kernel enabled.
2. Wake the paired controller in HOME+X mode.
3. Install the newly built package:

pnputil /add-driver "$pkg\XpMx210BL.inf" /install

4. Confirm that the Bluetooth service node is Started and bound to the newly published oemNN.inf package.
5. Look for these DebugView messages:

[XP-Mx210BL] VHF gamepad child started: VID=0x1949 PID=0x0402 reportID=0x07 descriptorLength=97
[XP-Mx210BL] HID Interrupt RX: length=12 remaining=0 data=A1 07 ...
 
6. Open the Windows Game Controllers panel by running joy.cpl.
7. Select the newly enumerated gamepad and test the axes, hat switch, and buttons.

VhfCreate returns after validating its configuration and reporting the virtual child to PnP; full child initialization may complete afterward. If the VHF child does not appear, capture PnP state and DebugView messages for VhfCreate/VhfStart rather than assuming the Bluetooth channels failed.

## Reconnect test

The current revision does not reopen channels automatically after the controller powers off. To retry the receive path, wake the controller in HOME+X mode and restart only its Bluetooth HID service node. A later revision will add automatic reconnect and sleep/power recovery.

## Rollback

Record the actual published package name returned by pnputil. Remove only that prototype package:

pnputil /delete-driver oemNN.inf /uninstall

Then rescan devices:

pnputil /scan-devices

Do not use /force. Keep the previously working Stage 2 receive package available until the VHF child is verified.