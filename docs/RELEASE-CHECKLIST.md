# Future validation checklist

Current status: **working prototype; public signing paused**. The checklist below records work that would be needed before any public or normal-mode release. It is not a claim that those tests have been completed.

## Already tested on the single target laptop

- [x] Bluetooth HID Control and Interrupt L2CAP channels open.
- [x] Actual controller input reaches the driver.
- [x] VHF gamepad appears as a native Windows gamepad.
- [x] Input verified in joy.cpl, Terraria, and Dead Cells.
- [x] Companion one-shot Connect action works in HOME + X mode.
- [x] Controller power-off removes the VHF child and Windows/Steam report disconnection.

## Engineering and compatibility work still open

- [ ] Build a reproducible Release/staging package with the intended WDK and verify the INF/CAT/SYS.
- [ ] Review KMDF lifetime, cancellation, disconnect, PnP, and power paths.
- [ ] Test reboot, Bluetooth radio off/on, sleep, hibernate, fast startup, and repeated reconnection.
- [ ] Test install, upgrade, failed-install recovery, and uninstall on a recoverable system.
- [ ] Test Secure Boot enabled with Test Mode disabled.
- [ ] Validate Memory Integrity / HVCI with Driver Verifier Code Integrity checks and the HLK readiness test.
- [ ] Test on Windows versions and Bluetooth adapters beyond the recorded target.
- [ ] Test Vanguard/League separately after Windows security settings are restored.

## Signing blocker

- [ ] Confirm an eligible Hardware Developer Program enrollment route and legal authority to accept its agreements.
- [ ] Confirm Microsoft accepts this third-party controller-specific Bluetooth profile driver under an eligible submission category.
- [ ] Obtain the required EV certificate and Hardware Dev Center enrollment only if eligibility is established.
- [ ] Determine attestation vs WHCP requirements for the intended Windows versions.
- [ ] Receive and verify a Microsoft-signed package.

## Public release

- [ ] Prepare end-user installation, uninstall, rollback, and recovery instructions.
- [ ] Prepare a versioned archive with the Microsoft-signed INF, SYS, CAT, Connect utility, and SHA-256 checksum.
- [ ] Test the exact archive from a clean download on a second machine.
- [ ] Publish a GitHub Release only after all signing and validation gates pass.
