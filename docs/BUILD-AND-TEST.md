# XP-Mx210BL driver: build and test

This is a test-signed development prototype, not a production release. The VHF gamepad path has been hardware-tested in joy.cpl, Terraria, and Dead Cells. The manual reconnect control added in the current branch still requires a target-machine build and test.

## Current implementation

The source driver:

- Opens the controller's HID Control and Interrupt L2CAP channels.
- Receives Interrupt-channel packets with a pending BRB_L2CA_ACL_TRANSFER request and re-submits the read after each completion.
- Creates a VHF child using the 97-byte Game Pad-only descriptor.
- Accepts only 12-byte HIDP DATA/INPUT packets with prefix A1 07.
- Removes the HIDP header byte A1 and forwards the 11-byte HID report (Report ID 07 plus the ten-byte payload) with HID_XFER_PACKET / VhfReadReportSubmit.
- Uses VHF's default input-report buffering.
- Stops and drains the Bluetooth reader before deleting the VHF child during PnP cleanup.

The current branch adds an administrator-only control interface and one-shot Connect IOCTL. The companion PowerShell/C# helper in tools/XpMx210BL-Connect.ps1 displays a Connect button. Each click submits one request to the driver. No periodic retry timer or background polling is implemented.

## Build

1. Open driver/XpMx210BL.vcxproj in Visual Studio 2022.
2. Select **Debug | x64**.
3. Rebuild the project.
4. Inspect build output for compiler/linker warnings and errors.

The project links VhfKm.lib. The INF registers vhf as a lower filter in [Install.NT.HW] via [Install.NT.HW.AddReg]:

~~~ini
[Install.NT.HW]
AddReg=Install.NT.HW.AddReg

[Install.NT.HW.AddReg]
HKR,,"LowerFilters",0x00010000,"vhf"
~~~

Do not move this value into [Install.NT.Services].

## Package verification

Use the generated package folder:

~~~powershell
$pkg = 'D:\Projects\XP-Mx210BL\driver\x64\Debug\XpMx210BL'

Select-String "$pkg\XpMx210BL.inf" -Pattern '^\s*DriverVer\s*=', '^\[Install\.NT\.HW\]', '^AddReg=Install\.NT\.HW\.AddReg', '^\[Install\.NT\.HW\.AddReg\]', 'LowerFilters.*vhf'
~~~

Verify the package catalog using SignTool (locate the x64 signtool.exe under Windows Kits\10\bin as in the earlier build instructions):

~~~powershell
& $signtool verify /pa /v /c "$pkg\xpmx210bl.cat" "$pkg\XpMx210BL.inf"
~~~

Expected: SignTool reports successful verification with no errors. Do not install a package that failed build or catalog verification.

## Install

Keep the existing known-working gamepad package available for rollback. Install the new package only after the checks above pass:

~~~powershell
pnputil /add-driver "$pkg\XpMx210BL.inf" /install
~~~

Record the actual published oemNN.inf name.

## Test initial connection

1. Start DebugView as administrator with Capture Events and Capture Kernel enabled.
2. Wake the paired controller in HOME+X mode.
3. Confirm the Bluetooth service node is Started and bound to the new package.
4. Look for:
   ~~~text
   [XP-Mx210BL] VHF gamepad child started: VID=0x1949 PID=0x0402 reportID=0x07 descriptorLength=97
   [XP-Mx210BL] Remote BD_ADDR=0x... flags=0x... connected=...
   [XP-Mx210BL] HID Control opened: PSM=0x0011 ...
   [XP-Mx210BL] HID Interrupt opened: PSM=0x0013 ...
   [XP-Mx210BL] Continuous HID Interrupt reader started
   ~~~
5. Run joy.cpl and verify the gamepad.

## Test manual reconnect

The driver performs one connection attempt on PnP start. After a controller-side disconnect, it does not poll or retry automatically. To request another connection:

1. Leave laptop Bluetooth enabled.
2. Turn the controller on in HOME+X mode.
3. From the repository root, launch the companion window:
   ~~~powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\XpMx210BL-Connect.ps1
   ~~~
   It requests elevation automatically if the current PowerShell session is not elevated.
4. Click **Connect** once.
5. A successful request should display a success message and DebugView should show:
   ~~~text
   [XP-Mx210BL] Connect attempt requested (user request); no background retry is scheduled
   [XP-Mx210BL] HID Control opened: PSM=0x0011 ...
   [XP-Mx210BL] HID Interrupt opened: PSM=0x0013 ...
   [XP-Mx210BL] Continuous HID Interrupt reader started
   [XP-Mx210BL] HID session connected (user request); both channels and reader are active
   ~~~

If the attempt fails, the status shown by the app and the associated DebugView lines should be captured. Turn the controller on before clicking Connect. The IOCTL performs one attempt; it does not keep trying while the controller is off.

Also test that turning the controller off leaves the VHF gamepad child present until Windows actually stops/removes the parent Bluetooth service device. Turning laptop Bluetooth off/on is a separate PnP restart test.

## Manual interface details

- Device interface GUID: {7F44E8AF-4C2B-4C86-8EF5-CF49E2B5DA18}
- Connect IOCTL: 0x22E000
- The interface is restricted to LocalSystem and elevated Administrators.
- The PowerShell helper enumerates the device interface and submits the IOCTL; it performs no polling.
- This is a companion Connect window, not an extension to Windows Bluetooth Settings / Quick Settings.

## Rollback

Record the published package name returned by PnPUtil. To remove only the new prototype package:

~~~powershell
pnputil /delete-driver oemNN.inf /uninstall
pnputil /scan-devices
~~~

Replace oemNN.inf with the actual new package name. Do not use /force. Keep the previously working VHF package available until the manual-connect revision has passed testing.
