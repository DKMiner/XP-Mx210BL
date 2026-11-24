# Investigation record

This document records observations from the XP-Mx210BL Windows Bluetooth investigation. It deliberately separates captured facts from hypotheses.

## Target computer

- Manufacturer/model: HP 250 G6 Notebook PC
- Reported OS: Windows 11 Pro
- Build observed: 22621.4317
- Bluetooth adapter: Intel Wireless Bluetooth, USB hardware ID `USB\\VID_8087&PID_0AA7`
- Adapter driver before testing: Intel 20.100.7.1 (2020-07-10)
- The user installed Intel's final 20.100.10.11 branch package and restarted; the controller still reports a Bluetooth HID driver error.

## Controller identity

- Friendly name: `XP-Mx210BL`
- Bluetooth address: `00:90:E1:5C:17:C2`
- HID service UUID: `00001124-0000-1000-8000-00805F9B34FB`
- PnP service instance observed:
  `BTHENUM\\{00001124-0000-1000-8000-00805F9B34FB}_VID&00021949_PID&0402\\...`
- PnP/SDP vendor/product identity: VID `1949`, PID `0402`
- SDP PnP Device ID service (UUID `0x1200`) starts successfully in Windows.
- The remote HID service node is assigned `hidbth.inf`, service `HidBth`.

## Working controller modes

- USB connection: controller works as a gamepad.
- Bluetooth HOME+X (Android/gamepad mode): Android successfully opens HID Control PSM `0x0011` and HID Interrupt PSM `0x0013`, and receives gamepad reports.
- Bluetooth HOME+Y: Android/iOS-style keyboard + consumer-control descriptor; no gamepad collection.
- Bluetooth HOME+B: iCade-style keyboard output; not suitable for analog gamepad input.
- Windows HOME+X: the remote HID service is discovered, but the in-box Bluetooth HID driver fails to start.
- Windows HOME+B: separate keyboard and consumer-control collections start, confirming the controller can expose an alternate Bluetooth HID profile to Windows.

## Captured HID protocol facts

The successful Android HOME+X SDP HID Report Descriptor is 208 bytes. It describes three application collections:

- Consumer Control, report ID `0x02`
- Mouse, report ID `0x06`
- Game Pad, report ID `0x07`

Observed gamepad input reports start with HIDP DATA/INPUT bytes `A1 07`. The gamepad report contains four 8-bit axes (X, Y, Z, Rz), a 4-bit hat field, 15 buttons, and 8-bit accelerator/brake fields.

Example neutral report observed:

```text
A1 07 80 80 80 80 88 00 00 00 00 00
```

The exact 208-byte descriptor should be added to `protocol/xp-mx210bl-home-x-report-descriptor.bin` (and a human-readable hex dump alongside it) before implementing the VHF HID descriptor. It is not reconstructed or fabricated in this repository.

## Windows failure evidence

### PnP start failure

Kernel-PnP Event 411 for the HID service node reports:

- Driver: `hidbth.inf`
- Service: `HidBth`
- PnP Problem: `0x0`
- Problem Status: `0xC00000E5` (`STATUS_INTERNAL_ERROR`)

The Bluetooth Device Identification Service (UUID `0x1200`) starts successfully at the same pairing attempts.

### HidBth System events

HidBth Event ID 2 records say:

> Bluetooth HID device either went out of range or became unresponsive.

Raw event records include device object names such as `\\Device\\_HID0000000d`, an empty second data field, and a repeated binary payload. These events don't identify a root cause or carry the remote address.

### ETW / HCI trace

The Windows ETW report contains BTHPORT raw HCI events. SDP traffic shows the HID service record and its fixed L2CAP PSMs, including `0x0011` and `0x0013`. The separate HidBth trace provider GUID recorded two events, but its event data is not meaningfully rendered by the available event metadata.

The previous trace appears to show service discovery completing before the HID transport fails. That is a useful lead, not yet proof of the exact internal failure point. The custom driver's explicit BRB completion statuses will be more decisive.

## What has been ruled out / not established

- Removing AFGC and HidHide did not fix HOME+X. HidHide service was absent afterward.
- ViGEmBus was removed; vJoy remains installed and functional.
- The Microsoft in-box HID Bluetooth driver is still the failing function driver.
- Updating the Intel Bluetooth driver to its final 20.100.10.11 branch did not fix the issue.
- No evidence establishes that the controller itself is defective: USB and Android Bluetooth gamepad mode work.
- We have not yet tested a second Bluetooth adapter. It remains an optional A/B diagnostic, not a required product dependency.

## Primary hypothesis

Windows successfully enumerates the remote HID SDP service but the in-box `HidBth` profile driver fails during initialization or connection setup. The prototype will replace the profile driver **only for this exact remote HID service instance** and report the status returned by the Bluetooth stack for each L2CAP BRB.

This is a hypothesis to test. It is not yet proven that the Intel adapter is innocent or that the in-box driver alone is defective.
