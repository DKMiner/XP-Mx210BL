# Build and test workflow

This file will be expanded as the KMDF project and installer are implemented.

## Development tools

- Windows 10/11 x64 development host
- Visual Studio 2022 with Desktop development with C++
- Windows Driver Kit (WDK) and matching Windows SDK
- Microsoft Windows Driver Samples repository for API examples

## Milestones

### P0 — Repository and design

- [x] Capture controller identity and protocol observations.
- [x] Identify Microsoft Bluetooth L2CAP client-profile sample.
- [x] Select a controller-specific `BTHENUM` match.
- [ ] Add KMDF driver project and INF.
- [ ] Build x64 Debug configuration.

### P1 — L2CAP connection probe

- [ ] Query Bluetooth profile-driver interface.
- [ ] Retrieve remote BD_ADDR from the BTHENUM PDO.
- [ ] Open Control PSM `0x0011`.
- [ ] Open Interrupt PSM `0x0013`.
- [ ] Log completion status and negotiated MTU for each.
- [ ] Close both channels reliably on removal/disconnect.
- [ ] Confirm driver failure does not leave stale channel handles.

### P2 — HIDP receive path

- [ ] Add inbound `BRB_L2CA_ACL_TRANSFER` on the Interrupt channel.
- [ ] Capture raw reports and compare them with Android's successful trace.
- [ ] Validate report ID, lengths, and disconnect behavior.

### P3 — Virtual gamepad

- [ ] Add the exact captured HID Report Descriptor as a binary fixture.
- [ ] Implement VHF source device descriptor and callbacks.
- [ ] Forward validated gamepad reports to VHF.
- [ ] Verify axes, hat, buttons, and neutral values in Windows game-controller properties.

## Installation policy

No INF installation instructions are provided until the driver is implemented, reviewed, and built. The eventual package must bind only to the exact XP-Mx210BL HID service hardware ID and must include tested rollback steps.

Kernel-driver testing can crash Windows. Prefer a separate test environment. Do not test an unreviewed build on a machine you rely on.
