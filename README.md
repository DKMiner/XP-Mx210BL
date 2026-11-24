# XP-Mx210BL Bluetooth HID driver

A Windows KMDF Bluetooth profile-driver experiment for the XP-7018L / XP-Mx210BL controller.

The goal is native Bluetooth gamepad input on Windows using the laptop's built-in Bluetooth radio. No Android relay, keyboard emulation, or permanently attached USB Bluetooth dongle.

## Current status

The first development milestone is **Stage 1: L2CAP probe**. A controller-specific KMDF profile-driver prototype and INF are in the `driver/` directory.

- Binds to the XP-Mx210BL HID service using its service UUID + VID/PID hardware ID.
- Queries the Windows Bluetooth profile-driver interface.
- Retrieves the remote Bluetooth address.
- Attempts the HID Control (`0x0011`) and HID Interrupt (`0x0013`) L2CAP channels.
- Logs NTSTATUS, Bluetooth BRB status, negotiated MTU, and channel handles.
- Uses a 12-second timeout for BRB operations.

**It has not yet been built with the WDK or tested on hardware. Do not install it yet.** The INF is intentionally controller-specific, but installation and rollback instructions will only be added after a clean build and static review.

Development branch: [work/l2cap-probe](https://github.com/DKMiner/XP-Mx210BL/tree/work/l2cap-probe).

## Roadmap

1. **L2CAP probe:** prove that the replacement profile driver can open both channels through the existing Windows Bluetooth stack.
2. **HIDP receive:** receive Interrupt-channel reports and verify report framing and IDs.
3. **Virtual gamepad:** publish the validated gamepad collection through Microsoft's Virtual HID Framework (VHF).

## Known hardware identity

- Bluetooth device name: `XP-Mx210BL`
- Bluetooth address observed in Windows: `00:90:E1:5C:17:C2`
- Remote HID service UUID: `00001124-0000-1000-8000-00805F9B34FB`
- SDP PnP identity: VID `1949`, PID `0402`
- HID Control PSM: `0x0011`
- HID Interrupt PSM: `0x0013`

See [docs/INVESTIGATION.md](docs/INVESTIGATION.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/RESEARCH.md](docs/RESEARCH.md), and [docs/BUILD-AND-TEST.md](docs/BUILD-AND-TEST.md).

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
