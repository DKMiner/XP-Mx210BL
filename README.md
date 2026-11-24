# XP-Mx210BL Bluetooth HID driver

A Windows KMDF Bluetooth profile-driver experiment for the XP-7018L / XP-Mx210BL controller.

The goal is native Bluetooth gamepad input on Windows using the laptop's built-in Bluetooth radio. No Android relay, keyboard emulation, or permanently attached USB Bluetooth dongle.

## Current status

The repository was initially empty. The first development work is being done on the `work/l2cap-probe` branch.

The project is currently at the prototype stage. Nothing in this repository should be installed on a daily-use Windows system yet.

## Target architecture

1. Bind a device-specific Bluetooth profile driver to the controller's HID service.
2. Use the Windows Bluetooth profile-driver interface and L2CAP BRBs to test the HID Control and Interrupt PSMs.
3. Receive HIDP reports and validate them against the descriptor captured from Android.
4. Publish a virtual HID gamepad through Microsoft's Virtual HID Framework (VHF).

The first implementation is deliberately a transport probe. It checks whether Windows' existing Bluetooth stack can open the controller's L2CAP channels without relying on the failing in-box `HidBth` profile driver.

## Known hardware identity

- Bluetooth device name: `XP-Mx210BL`
- Bluetooth address seen in Windows: `00:90:E1:5C:17:C2`
- Remote HID service UUID: `00001124-0000-1000-8000-00805F9B34FB`
- SDP PnP identity: VID `1949`, PID `0402`
- HID Control PSM: `0x0011`
- HID Interrupt PSM: `0x0013`

See [docs/INVESTIGATION.md](docs/INVESTIGATION.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the evidence and implementation plan.

## Development requirements

- Windows 10/11 x64
- Visual Studio 2022 with the C++ desktop workload
- Windows Driver Kit (WDK) matching the target Windows/SDK environment
- A separate test machine or a recoverable Windows installation is strongly recommended for kernel-driver experiments

Do not install an unsigned or untested kernel driver on a machine you rely on. The initial probe is not a production driver.

## References

- [Microsoft: Bluetooth driver stack](https://learn.microsoft.com/windows-hardware/drivers/bluetooth/using-the-bluetooth-driver-stack)
- [Microsoft: Installing a Bluetooth device / profile-driver INF matching](https://learn.microsoft.com/windows-hardware/drivers/bluetooth/installing-a-bluetooth-device)
- [Microsoft: Bluetooth L2CAP Echo sample](https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/bthecho)
- [Microsoft: Virtual HID Framework](https://learn.microsoft.com/windows-hardware/drivers/hid/virtual-hid-framework--vhf-)

## License

Project code will be licensed under the MIT License. Third-party code, if incorporated, will retain its original license and attribution.
