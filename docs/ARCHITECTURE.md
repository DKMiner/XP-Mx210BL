# Architecture

## Goal

Provide a native Windows gamepad for the XP-7018L / XP-Mx210BL over Bluetooth Classic HID, using the laptop's existing Intel Bluetooth radio.

No Android bridge, no iCade keyboard mapping, no permanently attached USB Bluetooth dongle.

## Device binding

Windows' Bluetooth enumerator creates a PnP node for each remote service. Microsoft documents these hardware-ID patterns:

- BTHENUM\{ServiceGUID}_VID&...
- BTHENUM\{ServiceGUID}_VID&..._PID&...
- compatible ID BTHENUM\{ServiceGUID}

The HID service GUID is:

00001124-0000-1000-8000-00805F9B34FB

The controller's observed node contains:

BTHENUM\{00001124-0000-1000-8000-00805F9B34FB}_VID&00021949_PID&0402

The intended INF match is this specific service + VID + PID hardware ID. The purpose is to outrank the generic inbox HID Bluetooth profile match for this controller only.

Do not match the generic BTHENUM HID compatible ID unless deliberately testing broadly. Do not bind to or replace the Intel Bluetooth radio driver.

## Transport layer

The profile driver attaches to the controller's HID service PDO. It obtains the Bluetooth profile-driver interface (GUID_BTHDDI_PROFILE_DRIVER_INTERFACE) and uses the default I/O target to send IOCTL_INTERNAL_BTH_SUBMIT_BRB requests to BthPort.sys.

Connection sequence:

1. Query the profile-driver interface.
2. Retrieve the remote Bluetooth address using IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
3. Open HID Control L2CAP PSM 0x0011 with BRB_L2CA_OPEN_CHANNEL.
4. Open HID Interrupt L2CAP PSM 0x0013 with BRB_L2CA_OPEN_CHANNEL.
5. Start a pending inbound BRB_L2CA_ACL_TRANSFER request on the Interrupt channel. After each successful read, the completion routine queues a DPC that re-submits the read.
6. Log the transferred length, remaining buffer size, and first 16 bytes of each received Interrupt-channel transfer.

The profile callback is channel-specific. When a remote disconnect is indicated, it invalidates only the matching channel handle, sets the CloseNow response so the Bluetooth stack can finish tearing down that channel, and cancels the continuous Interrupt read when appropriate. PnP cleanup stops and drains the read before closing still-valid channels.

The driver currently does not autonomously reopen channels after a remote disconnect while the service node remains started. Restarting this specific PnP device re-runs the open sequence. Automatic reconnect and power-transition recovery are separate work items.

The controller's captured SDP record indicates fixed PSMs; a new SDP query is not necessary for this first probe. Basic L2CAP mode is used initially, with the observed 672-byte MTU and an incoming queue depth of 10.

## HIDP and virtual HID

The current receive stage only records raw Interrupt-channel bytes. It does not implement the HIDP state machine.

Next steps after validating the received reports:

1. Capture complete input report payloads and compare them with the Android btsnoop trace.
2. Implement the required HIDP control-channel transactions and report handling.
3. Validate report IDs, lengths, and the captured 208-byte report descriptor.
4. Preserve the original descriptor and report bytes as protocol fixtures.
5. Submit validated gamepad input reports through a VHF-backed HID source device.

VHF is a separate logical HID device stack. The Bluetooth profile driver can be the report source, but it must submit the descriptor and input reports through the documented VHF API rather than pretending raw L2CAP packets are already Windows HID reports.

The final virtual gamepad should expose the captured gamepad collection, not the mouse or Consumer Control collection unless later desired.

## Why not patch hidbth.sys?

Patching a Microsoft inbox binary would be fragile, difficult to maintain, and would bypass the supported profile-driver model. An INF-selected profile driver is a cleaner experiment: Windows binds the custom driver to the specific BTHENUM service instance, while the Intel radio and Microsoft Bluetooth port stack remain in place.

## Safety and rollback principles

- Development stays on branch work/l2cap-probe.
- Do not install a new prototype revision until it builds cleanly and the generated INF/CAT/SYS package has been checked.
- Never replace the Intel radio driver.
- The controller-specific INF must contain the exact HID service + VID + PID match.
- Keep an explicit uninstall/rollback procedure for each published oemNN.inf package.
- Use a separate test environment where possible; a kernel driver bug can crash Windows.

## Setup class and virtual HID separation

The INF intentionally uses the Windows Bluetooth setup class, following Microsoft's Bluetooth profile-driver installation model. The current inbox HidBth package classifies its device node as HIDClass because it is itself a HID transport driver; this prototype instead owns the Bluetooth profile service and does not expose raw HID transport callbacks on that PDO. The gamepad that applications consume will be a separate virtual HID child created through VHF, where the HID class stack is appropriate.

The targeted match remains the remote HID service UUID plus VID 1949 / PID 0402.