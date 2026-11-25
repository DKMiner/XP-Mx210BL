# XP-Mx210BL Bluetooth HID profile driver

This KMDF profile driver binds to the XP-Mx210BL / XP-7018L Bluetooth Classic HID service instance only. It opens the HID L2CAP channels, receives HIDP gamepad reports, and exposes the Game Pad collection through VHF.

## Hardware-tested behavior

Tested on an HP 250 G6 running Windows 11 Pro 22H2 build 22621.4317 with Intel Wireless Bluetooth:

- HID Control PSM `0x0011` and Interrupt PSM `0x0013` open successfully.
- The Interrupt channel delivers 12-byte HIDP input packets beginning with `A1 07`.
- The 97-byte Game Pad-only descriptor is exposed through VHF as VID `0x1949`, PID `0x0402`.
- Input works in `joy.cpl`, Terraria, and Dead Cells.
- The companion Connect utility performs an explicit one-shot reconnect in HOME + X mode.
- When the controller disconnects, the driver stops the reader, closes the remaining L2CAP channel, deletes the VHF child, and requests a full Bluetooth ACL disconnect. Windows Settings and Steam reflected the disconnected state during testing.

Only **HOME + X** mode is supported by this prototype.

## Connection and disconnect policy

There is no periodic retry timer and no background polling.

- PnP device start makes one initial connection attempt.
- Each click in the companion Connect utility submits one additional attempt.
- A usable session requires both HID L2CAP channels and an active Interrupt reader.
- Loss of either channel tears down the complete session, removes the VHF child, and requests a full ACL disconnect.
- A subsequent user Connect request creates a new session and VHF child.
- The private control interface is restricted to LocalSystem and elevated Administrators.

The utility is in `tools/`. Double-click `Launch-XpMx210BL-Connect.cmd` to open it. It uses Windows PowerShell and WinForms; it is not a packaged standalone executable and is not integrated into Windows Bluetooth Settings or Quick Settings.

## Known limitations and unvalidated areas

- No Microsoft production signature, WHCP certification, or HLK results.
- Secure Boot-on operation and normal signature enforcement have not been tested with this prototype.
- Memory Integrity / HVCI compatibility has not been formally validated.
- Sleep, hibernate, fast startup, radio power transitions, repeated stress cycles, and clean install/upgrade/uninstall need separate validation.
- HIDP Control-channel SET_PROTOCOL, GET_REPORT, and SET_REPORT transactions are not implemented.
- No HID output/rumble path is implemented.

## Development build

Requirements: Visual Studio 2022, C++ desktop workload, WDK, and a compatible Windows SDK.

1. Open `XpMx210BL.vcxproj`.
2. Select **Debug | x64** and rebuild.
3. Inspect compiler and linker output.
4. Verify the generated INF and catalog before installing.
5. Install only on a recoverable test installation and record the published `oemNN.inf` name for rollback.

The target-machine build was test-signed and installed with Windows Test Mode enabled. Do not use that package as a public download. See [Build and Test](../docs/BUILD-AND-TEST.md) and [Signing Research](../docs/RELEASE-AND-SIGNING.md).

## Package identity

- Setup class: Bluetooth
- HID service UUID: `00001124-0000-1000-8000-00805F9B34FB`
- Matching remote identity: VID `1949`, PID `0402`
- Connect interface GUID: `{7F44E8AF-4C2B-4C86-8EF5-CF49E2B5DA18}`
- Connect IOCTL: `0x22E000`

The INF must continue to match the exact service UUID + VID + PID hardware ID. Do not widen the match to the generic Bluetooth HID compatible ID and do not bind to or replace the Intel Bluetooth radio driver.

## Rollback

Record the actual published `oemNN.inf` name returned by PnPUtil. To remove only the prototype package:

```powershell
pnputil /delete-driver oemNN.inf /uninstall
pnputil /scan-devices
```

Replace `oemNN.inf` with the actual package name. Do not use `/force`.
