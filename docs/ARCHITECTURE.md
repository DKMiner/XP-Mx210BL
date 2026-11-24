# Architecture

## Goal

Provide a native Windows gamepad for the XP-7018L / XP-Mx210BL over Bluetooth Classic HID, using the laptop's existing Intel Bluetooth radio.

No Android bridge, no iCade keyboard mapping, no permanently attached USB Bluetooth dongle.

## Device binding

Windows' Bluetooth enumerator creates a PnP node for each remote service. Microsoft documents these hardware-ID patterns:

- `BTHENUM\\{ServiceGUID}_VID&...`
- `BTHENUM\\{ServiceGUID}_VID&..._PID&...`
- compatible ID `BTHENUM\\{ServiceGUID}`

The HID service GUID is:

```text
00001124-0000-1000-8000-00805F9B34FB
```

The controller's observed node contains:

```text
BTHENUM\\{00001124-0000-1000-8000-00805F9B34FB}_VID&00021949_PID&0402
```

The intended INF match is this **specific** service + VID + PID hardware ID. The purpose is to outrank the generic inbox HID Bluetooth profile match for this controller only.

Do not match the generic `BTHENUM\\{00001124...}` compatible ID unless deliberately testing broadly. Do not bind to or replace the Intel Bluetooth radio driver.

## Transport layer

The profile driver attaches to the controller's HID service PDO. It obtains the Bluetooth profile-driver interface (`GUID_BTHDDI_PROFILE_DRIVER_INTERFACE`) and uses the default I/O target to send `IOCTL_INTERNAL_BTH_SUBMIT_BRB` requests to `BthPort.sys`.

Initial connection sequence:

1. Query the profile-driver interface.
2. Retrieve the remote Bluetooth address using `IOCTL_INTERNAL_BTHENUM_GET_DEVINFO`.
3. Open HID Control L2CAP PSM `0x0011` with `BRB_L2CA_OPEN_CHANNEL`.
4. Open HID Interrupt L2CAP PSM `0x0013` with `BRB_L2CA_OPEN_CHANNEL`.
5. Log the NTSTATUS, Bluetooth BRB status, negotiated MTU, and channel handle for each operation.
6. Close channels using `BRB_L2CA_CLOSE_CHANNEL` on orderly device removal and disconnect.

The first milestone does not send HIDP commands or create a VHF device. It only proves that the replacement profile driver can use the existing radio and open both channels.

Use Basic L2CAP mode initially, with conservative MTU negotiation and an incoming queue depth of 10 (Microsoft's documented recommendation). The controller's captured SDP record indicates fixed PSMs; a new SDP query is not necessary for this first probe.

## Input and virtual HID

After both channels open:

1. Receive interrupt-channel packets using `BRB_L2CA_ACL_TRANSFER` with inbound direction.
2. Validate HIDP framing and report IDs.
3. Preserve the original descriptor and report bytes as protocol fixtures.
4. Send valid HID input reports through a VHF-backed HID source device.

VHF is a separate logical HID device stack. The Bluetooth profile driver can be the report source, but it must submit the descriptor and input reports through the documented VHF API rather than pretending that raw L2CAP packets are already Windows HID reports.

The final virtual gamepad should expose the captured gamepad collection, not the mouse or Consumer Control collection unless later desired.

## Why not patch hidbth.sys?

Patching a Microsoft inbox binary would be fragile, difficult to maintain, and would bypass the supported profile-driver model. An INF-selected profile driver is a cleaner experiment: Windows binds the custom driver to the specific BTHENUM service instance, while the Intel radio and Microsoft Bluetooth port stack remain in place.

## Safety and rollback principles

- Development stays on branch `work/l2cap-probe`.
- Do not install a prototype kernel driver on the primary Windows installation before it builds cleanly and has been reviewed.
- Never replace the Intel radio driver.
- The controller-specific INF must contain the exact HID service + VID + PID match.
- Keep an explicit uninstall/rollback procedure in the eventual package.
- Use a separate test environment where possible; a kernel driver bug can crash Windows.
