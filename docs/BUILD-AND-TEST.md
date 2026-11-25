# XP-Mx210BL driver: build and hardware-test record

The Bluetooth HID transport, VHF gamepad, manual one-shot reconnect, and disconnect lifecycle have been hardware-tested on the developer's HP 250 G6 (Windows 11 Pro 22H2 build 22621.4317, Intel Wireless Bluetooth). Input was verified in `joy.cpl`, Terraria, and Dead Cells. The user also confirmed that Windows Bluetooth Settings and Steam reflect the controller disconnect. This is still a development build, not a production-signed release.

## Current implementation

- Opens the controller's HID Control and Interrupt L2CAP channels.
- Receives Interrupt-channel packets through a pending `BRB_L2CA_ACL_TRANSFER` read and re-submits the read after successful completion.
- Creates a VHF child using the 97-byte Game Pad-only report descriptor after both HID L2CAP channels open.
- Accepts 12-byte HIDP DATA/INPUT packets with prefix `A1 07`.
- Removes the HIDP header byte `A1` and forwards the 11-byte HID report (Report ID `07` plus ten payload bytes) through `HID_XFER_PACKET` / `VhfReadReportSubmit`.
- On either channel's remote disconnect, queues teardown work, stops the reader, closes the remaining channel, deletes the VHF child, and requests a full ACL disconnect using `IOCTL_BTH_DISCONNECT_DEVICE`.
- Supports one initial PnP-start connection attempt and explicit one-shot reconnect requests. No periodic retry or polling loop is used.

## Development build

1. Open `driver/XpMx210BL.vcxproj` in Visual Studio 2022 with a compatible WDK installed.
2. Select **Debug | x64**.
3. Rebuild and inspect compiler/linker output.
4. Inspect the generated INF and catalog. Confirm that the INF still matches only the controller's HID service UUID + VID/PID and that VHF is registered as the intended lower filter.
5. Verify the catalog with SignTool before installing.
6. Install only on a recoverable test machine:

```powershell
pnputil /add-driver "path\to\XpMx210BL.inf" /install
```

Record the published `oemNN.inf` name returned by PnPUtil.

The hardware-tested package was locally test-signed and installed with Windows Test Mode enabled and Secure Boot disabled. This branch does not claim a Release build, normal signature enforcement, Secure Boot-on, or HVCI validation. A local build is not an end-user release.

## Test the Connect helper

1. Leave the laptop's Bluetooth radio enabled.
2. Turn the paired controller on in **HOME + X** mode.
3. Double-click `tools/Launch-XpMx210BL-Connect.cmd`.
4. Accept the UAC prompt.
5. Click **Connect** once.

Each click is one connection attempt. The helper does not poll or retry in the background. It is a companion window, not a control inside Windows Bluetooth Settings.

## Test controller disconnect

1. Connect the controller in HOME + X mode.
2. Turn the controller off.
3. The driver should stop the Interrupt reader, close any remaining L2CAP channel, delete the VHF gamepad child, and request a full Bluetooth ACL disconnect.
4. Confirm the virtual gamepad disappears from `joy.cpl`.
5. Confirm Windows Bluetooth Settings reports **Not connected** and Steam no longer shows the gamepad as connected.
6. Turn the controller on in HOME + X mode and click Connect to create a fresh session.

These disconnect outcomes were confirmed on the target laptop.

## Manual interface details

- Device interface GUID: `{7F44E8AF-4C2B-4C86-8EF5-CF49E2B5DA18}`
- Connect IOCTL: `0x22E000`
- Interface access: LocalSystem and elevated Administrators.
- Companion source: `tools/XpMx210BL-Connect.ps1`
- Double-click launcher: `tools/Launch-XpMx210BL-Connect.cmd`

## Rollback

Record the published package name returned by PnPUtil. To remove only the prototype package:

```powershell
pnputil /delete-driver oemNN.inf /uninstall
pnputil /scan-devices
```

Replace `oemNN.inf` with the recorded package name. Do not use `/force`.

## Not yet validated

- Secure Boot enabled with Windows Test Mode disabled.
- Microsoft production signing, attestation/WHCP submission, and HLK certification.
- Memory Integrity / HVCI on the target laptop.
- Sleep/resume, hibernate, fast startup, Bluetooth radio power transitions, and lengthy reconnect stress tests.
- Install, upgrade, and uninstall on a clean second machine.
- Compatibility with Windows releases other than the recorded 22H2 build.

See [Signing Research and Current Blocker](RELEASE-AND-SIGNING.md) for the paused signing path and future validation gates.
