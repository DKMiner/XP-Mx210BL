# XP-Mx210BL protocol fixtures

## HOME+X HID report descriptor

- Binary source of truth: `xp-mx210bl-home-x-report-descriptor.bin`
- Human-readable dump: `xp-mx210bl-home-x-report-descriptor.hex`
- Length: **208 bytes**
- SDP attribute: `0x0206` (HIDDescriptorList)
- HID descriptor type: `0x22` (Report Descriptor)
- SHA-256: `23d374412356a5abef40af844f9ec9fec67b282092f97f43f4df843809df1f58`

### Provenance

Extracted from the Android Bluetooth HCI snoop capture `btsnoop_hci.log.last` in the investigation source files. The SDP exchange was isolated to the classic Bluetooth connection for the controller address `00:90:E1:5C:17:C2`, then the SDP ServiceAttributeResponse fragments were reassembled. The report descriptor was taken from HIDDescriptorList attribute `0x0206`, descriptor type `0x22`. The file is the exact 208-byte descriptor transmitted by the controller; it was not manually reconstructed.

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
