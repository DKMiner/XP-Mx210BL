# XP-Mx210BL Bluetooth HID driver

A Windows KMDF Bluetooth profile-driver experiment for the XP-7018L / XP-Mx210BL controller.

The goal is native Bluetooth gamepad input on Windows using the laptop's built-in Bluetooth radio. No Android relay, keyboard emulation, or permanently attached USB Bluetooth dongle.

## Current status

**Stage 1 — Bluetooth transport: hardware-tested.** The controller-specific KMDF profile driver replaces the failing inbox HID Bluetooth profile binding only for the exact XP-Mx210BL HID service UUID + VID/PID. On the target HP laptop, the driver successfully opens HID Control PSM 0x0011 and HID Interrupt PSM 0x0013 through the existing Intel Bluetooth adapter. Both channels negotiated 672-byte MTUs.

**Stage 2 — HIDP receive: hardware-tested.** A continuous inbound L2CAP reader receives actual 12-byte gamepad input packets. In the saved DebugView capture, all 229 received reports began with A1 07 (HIDP DATA/INPUT, Report ID 0x07), and 150 distinct report patterns were observed while the controls were exercised. Axis, hat, and button fields changed in the reports.

**Stage 3 — VHF virtual gamepad: implementation committed, not yet built or hardware-tested.** The current branch adds a VHF source-device lifecycle and forwards validated Report ID 0x07 packets to a Game Pad-only VHF child. It uses the exact 97-byte Game Pad collection extracted from the controller's original 208-byte SDP report descriptor. The new revision must be rebuilt, package-verified, and tested on the target laptop before installation.

The project is a test-signed development prototype, not a production driver. Keep the known-working receive-probe package available for rollback until the VHF revision is verified.

Development branch: [work/l2cap-probe](https://github.com/DKMiner/XP-Mx210BL/tree/work/l2cap-probe).

## Roadmap

1. **Bluetooth transport:** open both fixed HID L2CAP channels through Windows' Bluetooth stack. Hardware-tested.
2. **HIDP receive:** receive and validate the controller's Interrupt-channel gamepad reports. Hardware-tested.
3. **Virtual gamepad:** publish only the Game Pad top-level collection through Microsoft's Virtual HID Framework (VHF). Implementation committed; target-machine build/test pending.
4. **Connection lifecycle:** add automatic recovery after the controller powers off or reconnects, and assess any HIDP Control-channel transactions needed for additional functions such as output reports.

## Known hardware identity

- Bluetooth device name: XP-Mx210BL
- Bluetooth address observed in Windows: 00:90:E1:5C:17:C2
- Remote HID service UUID: 00001124-0000-1000-8000-00805F9B34FB
- SDP PnP identity: VID 1949, PID 0402
- HID Control PSM: 0x0011
- HID Interrupt PSM: 0x0013

See [docs/INVESTIGATION.md](docs/INVESTIGATION.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/RESEARCH.md](docs/RESEARCH.md), [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md), and [protocol/README.md](protocol/README.md).

## Development requirements

- Windows 10/11 x64
- Visual Studio 2022 with the C++ desktop workload
- Windows Driver Kit (WDK) and matching Windows SDK
- A separate test machine or a recoverable Windows installation is strongly recommended for kernel-driver experiments

## References

- [Microsoft: Installing a Bluetooth Device](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/installing-a-bluetooth-device)
- [Microsoft: Bluetooth driver stack](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/using-the-bluetooth-driver-stack)
- [Microsoft: Bluetooth L2CAP Echo sample](https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/bthecho)
- [Microsoft: Creating a L2CAP client connection](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/creating-a-l2cap-client-connection-to-a-remote-device)
- [Microsoft: Virtual HID Framework](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf-)

## License

Project code is licensed under the MIT License. Third-party code, if incorporated, will retain its original license and attribution.