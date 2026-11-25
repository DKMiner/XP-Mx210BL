# XP-Mx210BL protocol fixtures

## HOME+X HID report descriptor

- Binary source of truth: `xp-mx210bl-home-x-report-descriptor.bin`
- Human-readable dump: `xp-mx210bl-home-x-report-descriptor.hex`
- Length: **208 bytes**
- SDP attribute: `0x0206` (HIDDescriptorList)
- HID descriptor type: `0x22` (Report Descriptor)
- SHA-256: `23d374412356a5abef40af844f9ec9fec67b282092f97f43f4df843809df1f58`

### Provenance

Extracted from the Android Bluetooth HCI snoop capture `btsnoop_hci.log.last` in the investigation source files. The SDP exchange was isolated to the classic Bluetooth connection for the controller address `the XP-Mx210BL controller`, then the SDP ServiceAttributeResponse fragments were reassembled. The report descriptor was taken from HIDDescriptorList attribute `0x0206`, descriptor type `0x22`. The file is the exact 208-byte descriptor transmitted by the controller; it was not manually reconstructed.

### Application collections and report IDs

The descriptor defines three top-level collections:

| Collection | Report ID |
|---|---:|
| Consumer Control | `0x02` |
| Mouse | `0x06` |
| Game Pad | `0x07` |

The Game Pad collection declares four 8-bit axes (Generic Desktop usages X, Y, Z, and Rz), a 4-bit hat switch, a 16-bit button field, and 8-bit Accelerator and Brake values. The descriptor also contains padding/reserved bits as specified by the original report.

The Interrupt-channel captures from Windows contain 12-byte HIDP input packets with prefix `A1 07`; the remaining 10 bytes are the Report ID 0x07 payload. A neutral sample observed in the capture is:

```text
A1 07 80 80 80 80 88 00 00 00 00 00
```

## Fixture policy

Treat the binary descriptor as authoritative. Changes should be justified by a new capture from the physical controller, not by hand-editing a guessed descriptor. Preserve source captures separately; do not check private pairing keys or unrelated device traces into the repository.

## VHF Game Pad-only descriptor

- Binary VHF fixture: `xp-mx210bl-home-x-gamepad-report-descriptor.bin`
- Hex dump: `xp-mx210bl-home-x-gamepad-report-descriptor.hex`
- Length: **97 bytes**
- SHA-256: `4e5d0e6f151961e6dc0d965b9bc924db2340b6614b5a846d033986a307306d1f`

This descriptor is the exact byte-for-byte slice beginning at the Game Pad top-level Application Collection (Usage Page Generic Desktop, Usage Game Pad, Report ID `0x07`) and ending at its matching top-level `END_COLLECTION` in the original 208-byte descriptor. No report fields were rewritten. It is used for the VHF child so Windows enumerates only the gamepad collection, not the physical descriptor's Consumer Control and Mouse collections.

## HIDP input packet and VHF mapping

A captured gamepad input packet is 12 bytes:

- Byte 0: `0xA1`, HIDP DATA/INPUT header
- Byte 1: `0x07`, HID Report ID
- Bytes 2–11: the 10-byte Report ID 0x07 payload

Payload offsets, relative to byte 2 of the HIDP packet:

| Payload offset | Width | Descriptor meaning |
|---:|---:|---|
| 0 | 8 bits | X axis |
| 1 | 8 bits | Y axis |
| 2 | 8 bits | Z axis |
| 3 | 8 bits | Rz axis |
| 4 | 4 bits + 4 constant bits | Hat switch and padding |
| 5–6 | 16 bits | Button field (descriptor Report Count 16) |
| 7 | 8 bits | Accelerator |
| 8 | 8 bits | Brake |
| 9 | 8 constant bits | Padding |

The raw Bluetooth HIDP packet header `0xA1` is not part of the HID report delivered to VHF. For VHF, use Report ID `0x07`, a report buffer beginning at the ID byte (the `0x07` byte followed by the 10-byte payload), and reportBufferLen 11. This matches the VHF `HID_XFER_PACKET` pattern used in Microsoft's HIDInjector sample. VHF's default report buffering is used (no EvtVhfReadyForNextReadReport callback), so the receive buffer can be reused after VhfReadReportSubmit returns.

References:
- Microsoft VHF source-driver guide: https://learn.microsoft.com/windows-hardware/drivers/hid/virtual-hid-framework--vhf-
- Microsoft HID_XFER_PACKET reference: https://learn.microsoft.com/windows-hardware/drivers/ddi/vhf/ns-vhf-_hid_xfer_packet
- Microsoft VhfReadReportSubmit reference: https://learn.microsoft.com/windows-hardware/drivers/ddi/vhf/nf-vhf-vhfreadreportsubmit
- Microsoft HIDInjector sample: https://github.com/microsoft/Windows-IoT-Samples/tree/master/samples/HIDInjector
