# XP-Mx210BL

A Windows Bluetooth Classic HID profile-driver prototype for the XP-Mx210BL / XP-7018L controller. It receives the controller's Bluetooth HID reports and exposes the Game Pad collection to Windows through Microsoft's Virtual HID Framework (VHF).

> **Project status — 2026-11-25:** The Bluetooth transport, VHF gamepad, one-shot reconnect utility, and controller-disconnect teardown have been tested on the developer's HP 250 G6. This is a working, test-signed development prototype, not a production-signed driver. Work on public signing is paused because the Microsoft Hardware Developer Program requires organization-level enrollment prerequisites; no supported individual, machine-only signing route has been identified. The source is being kept as a known-tested snapshot on `work/l2cap-probe`.

## What has been tested

- Bluetooth Classic HID in **HOME + X mode**.
- HID Control and Interrupt L2CAP channels, PSM `0x0011` and `0x0013`.
- Incoming 12-byte HIDP gamepad packets with Report ID `0x07`.
- The 97-byte Game Pad-only report descriptor exposed through VHF as VID `0x1949`, PID `0x0402`.
- Native gamepad input in `joy.cpl`, Terraria, and Dead Cells.
- The companion Connect utility makes one explicit connection attempt per click; no periodic retry or polling loop is used.
- When the controller is switched off, the driver tears down the session, removes the VHF child, and requests a full Bluetooth ACL disconnect. Windows Bluetooth Settings and Steam showed the device as disconnected during testing.

## Compatibility and scope

| Item | Tested scope |
|---|---|
| Controller | XP-Mx210BL / XP-7018L |
| Controller mode | HOME + X |
| Bluetooth service | Classic HID, UUID `00001124-0000-1000-8000-00805F9B34FB` |
| Remote VID / PID | `1949` / `0402` |
| Architecture | x64 |
| Windows | Windows 11 Pro 22H2, build 22621.4317 |
| Host | HP 250 G6, Intel Wireless Bluetooth (USB `VID_8087&PID_0AA7`) |

This is a report of one tested machine, not a compatibility promise for other Windows versions, editions, Bluetooth adapters, or computers. HOME-only and other controller modes are not supported by this implementation.

## Signing and Windows security status

The tested package was locally test-signed and loaded while Windows Test Mode was enabled and Secure Boot was disabled. That is a development-only setup. After Test Mode is disabled and normal kernel signature enforcement is restored, Windows is expected to refuse to load this test-signed driver. The package can remain in the Driver Store; uninstalling it is not required just to restore normal Windows boot settings. The controller's Bluetooth HID function will not use this prototype until a supported signed package or a different implementation is available.

Memory Integrity / HVCI, Secure Boot-on operation, normal-signature-enforcement operation, HLK validation, and WHCP certification have not been validated for this driver. Do not distribute the test-signed package.

## Build and tools

- Visual Studio 2022 with the C++ desktop workload.
- Windows Driver Kit (WDK) and a compatible Windows SDK.
- A recoverable Windows test installation for kernel-driver experiments.

The companion Connect tool is in `tools/`. Double-click `tools/Launch-XpMx210BL-Connect.cmd`; Windows displays a UAC prompt because the control interface is administrator-only. It launches the PowerShell/WinForms helper, not a standalone executable.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Build and hardware test notes](docs/BUILD-AND-TEST.md)
- [Investigation record and target-machine results](docs/INVESTIGATION.md)
- [Protocol fixtures](protocol/README.md)
- [Microsoft signing research and current blocker](docs/RELEASE-AND-SIGNING.md)
- [Future validation checklist](docs/RELEASE-CHECKLIST.md)

## Safety

This is third-party kernel-mode code. A faulty driver can crash Windows or prevent this controller's Bluetooth HID service from starting. Keep a recovery path and do not install development builds on a computer you cannot recover.

## License

MIT. See [LICENSE](LICENSE).
