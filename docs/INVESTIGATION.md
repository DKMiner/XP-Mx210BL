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
- Bluetooth address: `the XP-Mx210BL controller`
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

Observed gamepad input reports start with HIDP DATA/INPUT bytes `A1 07`. The gamepad report contains four 8-bit axes (X, Y, Z, Rz), a 4-bit hat field, a 16-bit button field (the descriptor declares usages 1 through 15), 8-bit Accelerator and Brake fields, and padding.

Example neutral report observed:

```text
A1 07 80 80 80 80 88 00 00 00 00 00
```

The exact 208-byte descriptor was extracted from the Android HCI snoop capture's SDP HIDDescriptorList attribute 0x0206 (descriptor type 0x22) and is preserved in `protocol/xp-mx210bl-home-x-report-descriptor.bin` with a human-readable hex dump. Its SHA-256 is `23d374412356a5abef40af844f9ec9fec67b282092f97f43f4df843809df1f58`.

The 97-byte Game Pad-only collection slice used by the VHF child is preserved separately as `protocol/xp-mx210bl-home-x-gamepad-report-descriptor.bin` and a hex dump. Its SHA-256 is `4e5d0e6f151961e6dc0d965b9bc924db2340b6614b5a846d033986a307306d1f`. It is an exact slice of the original descriptor, not a rewritten or guessed report definition.

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

## Custom profile-driver test (2026-11-25)

The controller-specific profile driver package was successfully test-signed, staged, ranked above the generic inbox HID match, and installed on the exact remote HID service instance.

Observed PnP state after installation:

- Device description: XP-Mx210BL Bluetooth HID Profile (Probe)
- Setup class: Bluetooth
- PnP status: Started / OK
- Published driver package: oem111.inf
- The prior hidbth.inf binding is no longer selected for this service instance.

The user observed that the controller's connection LEDs became steady and remained steady after restarting the custom Bluetooth HID service device. This indicates that the custom profile node starts and that the controller recognizes a live Bluetooth connection state; it does not by itself prove HIDP initialization or usable gamepad input.

### Stage 1 BRB results

The first DebugView kernel capture recorded:

- One initial HID Control open failure: NTSTATUS 0xC00000C2, Bluetooth BRB status 0, Bluetooth status 0x1F.
- A later successful HID Control open on PSM 0x0011 with inbound/outbound MTU 672.
- A successful HID Interrupt open on PSM 0x0013 with inbound/outbound MTU 672.
- A successful orderly close of both channels.
- A subsequent successful reopen of both channels.

A later capture included multiple cycles of remote disconnect indications, device-node restarts, and successful opens of both PSMs again. It also showed close attempts returning 0xC000009D after remote disconnects had already invalidated the channels.

These logs establish that the Windows Bluetooth stack can open both HID L2CAP channels through the custom profile driver. They do not establish that HIDP control transactions or input-report reception work yet.

### Stage 2 receive probe — hardware-tested

The Stage 2 reader was built and installed on the target laptop. Its DebugView capture recorded 229 complete 12-byte Interrupt-channel packets. Every packet began with `A1 07` (HIDP DATA/INPUT, Report ID 0x07), and 150 distinct packet patterns appeared while buttons and sticks were exercised. The reports included the previously observed neutral sample:

```text
A1 07 80 80 80 80 88 00 00 00 00 00
```

The changing values in the axis, hat, and button portions match the HOME+X Game Pad report collection from the Android descriptor. This confirms that actual gamepad input reaches the custom Windows profile driver over the laptop's built-in Intel Bluetooth adapter.

The receive logger was subsequently changed to print each packet as one compact DebugView line rather than one line per byte.

### Stage 3 VHF bridge and session lifecycle — hardware-tested

The VHF bridge was built and installed on the target laptop. The exact 97-byte Game Pad-only descriptor is exposed as a native VHF gamepad (VID 0x1949, PID 0x0402). Input was confirmed in joy.cpl, Terraria, and Dead Cells.

The driver accepts the controller's 12-byte HIDP input packet (prefix A1 07), strips the HIDP header byte, and submits the 11-byte report (Report ID 0x07 plus ten payload bytes) to VHF.

The companion Connect utility was tested on the target laptop. It performs one user-requested connection attempt per click; it does not poll or retry in the background. HOME + X is the supported controller mode. HOME-only mode is not part of the supported path.

When the controller is switched off, the disconnect worker stops and drains the Interrupt reader, closes the surviving L2CAP channel, deletes the VHF child, and requests a full Bluetooth ACL disconnect. The user confirmed that Windows Bluetooth Settings changes to Not connected, Steam reports the controller disconnected, and the virtual gamepad disappears from joy.cpl. A later Connect click recreates the session.

## Project status and remaining validation (2026-11-25)

The working prototype is retained on work/l2cap-probe as a known-tested development snapshot. Its local test signature is not a production signature. Microsoft Hardware Developer Program enrollment currently requires organization-level information and authorization, an Entra ID global administrator, and an associated EV certificate; no documented individual machine-only signing route has been identified. The project is paused at this signing gate.

The following have not been validated: operation with Secure Boot enabled and Test Mode disabled; Memory Integrity / HVCI; WHCP/HLK certification; sleep, hibernate, fast startup, and Bluetooth power-transition recovery; and compatibility beyond the single recorded Windows 11 Pro 22H2 target machine.
