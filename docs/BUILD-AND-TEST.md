# Stage 2 driver: L2CAP receive probe

This is a development prototype, not a production release.

## Current implementation

When the XP-Mx210BL's Bluetooth HID service PDO starts, the driver:

1. Queries GUID_BTHDDI_PROFILE_DRIVER_INTERFACE.
2. Retrieves the remote address through IOCTL_INTERNAL_BTHENUM_GET_DEVINFO.
3. Opens HID Control PSM 0x0011 and HID Interrupt PSM 0x0013.
4. Starts a single pending inbound BRB_L2CA_ACL_TRANSFER request on the Interrupt channel.
5. Logs the first 16 bytes of each received transfer and re-submits the pending read after successful completion.
6. Tracks channel handles independently, invalidates a handle on its remote-disconnect callback, requests the stack to close that disconnected channel, and cancels the pending read.
7. Stops/drains the read before attempting orderly channel close during PnP cleanup.

Basic L2CAP mode and an incoming queue depth of 10 are used. The observed channel MTU is 672 bytes. The reader uses a 672-byte nonpaged buffer and intentionally leaves its inbound BRB pending until data arrives or it is canceled.

## Known limitations

- No HIDP Control-channel transactions are implemented yet.
- Incoming data is logged, not translated into Windows HID input.
- No HID descriptor parser or VHF virtual gamepad exists yet.
- Automatic reconnect after a remote disconnect is not implemented. Restarting this Bluetooth HID service device retries opening both channels.
- Sleep/hibernate and D0 power-transition recovery are not implemented.

## Build

Open driver/XpMx210BL.vcxproj in Visual Studio 2022 with the WDK integration installed. Select Debug | x64 and build.

The Stage 1 probe built and ran on the target laptop. The Stage 2 receive-reader revision is a new code change and must be built and tested on that machine before installation. Do not assume a successful Stage 1 build proves this revision is buildable.

## First receive test

1. Build Debug | x64.
2. Regenerate the catalog using the project's package target; use local time for Inf2Cat as required by the target machine's package setup.
3. Verify the new catalog signature with SignTool /pa.
4. Confirm Windows is still in the intended development test-signing configuration.
5. Start DebugView as administrator with Capture Events and Capture Kernel enabled.
6. Wake the paired controller in HOME+X mode.
7. Install the newly built package with pnputil /add-driver <package-inf> /install.
8. Confirm the device is Started and its Driver Name is the newly published oemNN.inf.
9. Press several buttons and move the sticks briefly. Capture the [XP-Mx210BL] HID Interrupt RX messages.

The reader logs only the first 16 bytes of each transfer. Compare those bytes with the previously captured Android HIDP reports. Do not assume that an open L2CAP channel alone completes the HID protocol.

## Reconnect test

Turn the controller off and on in HOME+X mode while watching DebugView. The current revision should log remote disconnect indications and cancel the pending read. It does not yet autonomously reopen channels. Restart only the controller's Bluetooth HID service device to retry the probe; do not remove the pairing or cycle the Intel Bluetooth adapter unless separately diagnosing the adapter.

## Rollback

Record the published package name returned by pnputil /add-driver. To remove only this prototype package, use its actual published name:

pnputil /delete-driver oemNN.inf /uninstall

Then ask PnP to rescan:

pnputil /scan-devices

Do not use /force, and do not remove the Intel Bluetooth radio or generic Bluetooth bus packages.