# XP-Mx210BL Bluetooth HID driver

A Windows KMDF Bluetooth profile-driver experiment for the XP-7018L / XP-Mx210BL controller.

## Current status

**Bluetooth transport and HIDP receive: hardware-tested.** The custom profile driver opens HID Control PSM 0x0011 and HID Interrupt PSM 0x0013 through the laptop's Intel Bluetooth adapter. It receives valid 12-byte gamepad reports.

**VHF virtual gamepad: hardware-tested in games.** The exact 97-byte Game Pad collection from the controller's original SDP descriptor is exposed through VHF. The user verified the controls in joy.cpl, Terraria, and Dead Cells.

**Manual reconnect control: implementation committed, target build/test pending.** The driver now exposes an administrator-only device interface and a one-shot Connect IOCTL. The companion tools/XpMx210BL-Connect.ps1 opens a small window with a Connect button. Each click makes one connection attempt; there is no periodic retry timer or background polling. PnP device start still performs one normal initial connection attempt.

We have not found a documented extension point for adding a custom HID profile driver's Connect button to the native Windows Bluetooth Settings / Quick Settings flyout. The companion window is the explicit control path implemented by this project.

The project remains a test-signed development prototype, not a production driver. Keep the known-working VHF package available for rollback until the manual-connect revision is built and tested.

Development branch: [work/l2cap-probe](https://github.com/DKMiner/XP-Mx210BL/tree/work/l2cap-probe).

## Roadmap

1. Bluetooth transport and HIDP receive: hardware-tested.
2. VHF virtual gamepad: hardware-tested in joy.cpl and games.
3. Explicit reconnect: one connection attempt from a companion Connect window. Source committed; target build/test pending.
4. Connection-state diagnostics: log the Bluetooth stack's BDIF_CONNECTED state.
5. Release preparation: complete lifecycle testing and pursue the appropriate Microsoft driver-signing route.

There is no automatic retry loop. A user clicks Connect when the controller is awake in HOME+X mode.

## Known hardware identity

- Bluetooth device name: XP-Mx210BL
- Bluetooth address observed in Windows: 00:90:E1:5C:17:C2
- Remote HID service UUID: 00001124-0000-1000-8000-00805F9B34FB
- SDP PnP identity: VID 1949, PID 0402
- HID Control PSM: 0x0011
- HID Interrupt PSM: 0x0013
- Manual control interface GUID: {7F44E8AF-4C2B-4C86-8EF5-CF49E2B5DA18}

See docs/INVESTIGATION.md, docs/ARCHITECTURE.md, docs/RESEARCH.md, docs/BUILD-AND-TEST.md, driver/README.md, and protocol/README.md.

## Development requirements

- Windows 10/11 x64
- Visual Studio 2022 with the C++ desktop workload
- Windows Driver Kit (WDK) and matching Windows SDK
- A recoverable Windows installation is strongly recommended for kernel-driver experiments

## References

- [Microsoft: Installing a Bluetooth Device](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/installing-a-bluetooth-device)
- [Microsoft: Bluetooth driver stack](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/using-the-bluetooth-driver-stack)
- [Microsoft: Creating a L2CAP client connection](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/creating-a-l2cap-client-connection-to-a-remote-device)
- [Microsoft: BluetoothSetServiceState](https://learn.microsoft.com/en-us/windows/win32/api/bluetoothapis/nf-bluetoothapis-bluetoothsetservicestate)
- [Microsoft: Virtual HID Framework](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf-)

## License

Project code is licensed under the MIT License. Third-party code, if incorporated, will retain its original license and attribution.
