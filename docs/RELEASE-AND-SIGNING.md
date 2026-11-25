# Driver signing research and release gates

Last researched: 2026-11-25

## Current decision: keep the prototype paused

The working HOME + X prototype is hardware-tested on one Windows 11 Pro 22H2 machine. It is retained on `work/l2cap-probe` as a known-tested source snapshot. No production-signed package exists, and public distribution is not currently planned.

Microsoft's Hardware Developer Program enrollment instructions require organization information, an Entra ID global administrator, a legal contact authorized to accept the program agreements for that organization, and an associated EV code-signing certificate. The current documented process does not describe a simple individual hobbyist route. Without an eligible organization and an authorized enrollment, this project cannot use the documented Hardware Dev Center submission process at present.

Do not invent company information, use another party's identity or certificate, or assume that a locally self-signed certificate is a substitute for Microsoft's kernel signature. No supported per-machine exception for this new kernel-mode driver has been identified.

## What the current local signature means

The tested build was locally test-signed and loaded while Windows Test Mode was enabled and Secure Boot was disabled. This is suitable only for controlled development.

Turning Test Mode off and restoring normal signature enforcement does **not** require uninstalling the driver package. The package may remain in the Driver Store, but Windows is expected to refuse to load the test-signed driver. The controller's Bluetooth HID service may consequently fail to start until a supported signed package or a different implementation is available. This is not a reason to leave Test Mode enabled on a daily-use Windows installation.

## Microsoft signing routes researched

### Attestation signing

Attestation signing is a Microsoft Hardware Dev Center submission, not a local self-sign. Microsoft's current documentation describes it as supporting Windows Desktop kernel-mode drivers on Windows 10 Desktop and later without an HLK test campaign. It is not WHCP certification and does not satisfy the newer Windows Driver Policy where that policy requires WHCP-certified drivers. It still requires Hardware Developer Program access and its enrollment prerequisites.

For the recorded Windows 11 Pro 22H2 build 22621.4317, attestation signing looked like a possible target to investigate if an eligible Hardware Dev Center account were available. No submission or target-machine test was performed.

### WHCP certification

WHCP requires the applicable HLK campaign and submission through Hardware Dev Center. It is the supported route for certification and wider support on Windows versions that enforce the new Windows Driver Policy. Windows Update distribution is separate; a signed package could be hosted on GitHub Releases if all eligibility and validation requirements were met.

### Custom Kernel Signers

Microsoft's Custom Kernel Signers / App Control route is not a per-machine switch for this existing 22H2 installation. The researched prerequisites include a supported Windows 11 version/edition, a clean installation with the feature enabled during initial setup, and careful UEFI Secure Boot key management. It is not a practical option for this machine as currently installed.

## Secure Boot, Test Mode, and restoring ordinary Windows

- The test-signed prototype is not expected to load once Test Mode is disabled and normal signature enforcement is restored.
- The driver package does not have to be uninstalled just to turn Test Mode off or re-enable Secure Boot.
- If the Bluetooth HID service node no longer starts after restoring normal boot security, that is the expected signing limitation; it does not mean Windows itself is broken.
- Do not leave Test Mode enabled solely to keep this prototype working if the priority is normal Windows security and Vanguard.
- Before enabling Secure Boot, verify in `msinfo32` that BIOS Mode is UEFI. Do not switch an existing Windows installation from Legacy/CSM to UEFI as an improvised step.

## Memory Integrity / HVCI

Memory Integrity (also called HVCI) is a VBS feature that runs kernel-mode code-integrity decisions in a hypervisor-protected environment. It requires supported hardware virtualization and compatible kernel drivers.

Microsoft's driver guidance says HVCI-compatible drivers should opt in to NX by default, use NX memory allocations such as `NonPagedPoolNx`, avoid writable-and-executable sections, avoid modifying executable system memory, and avoid dynamic code in kernel mode. The source-level use of NX pool in this project is not proof of compatibility.

The project has **not** been formally validated with Memory Integrity enabled. Microsoft recommends Driver Verifier's Code Integrity checks, functional testing with Memory Integrity enabled, and the HLK Hypervisor Code Integrity Readiness Test. Those are future validation tasks, not steps to run casually on the daily-use installation.

## Remaining release gates

- Confirm an eligible Hardware Developer Program enrollment route before spending money or preparing a submission.
- Confirm the exact applicable attestation or WHCP path for this third-party controller-specific Bluetooth profile driver.
- Produce and verify a reproducible release package.
- Test on a recoverable machine with Secure Boot on and Test Mode off.
- Validate Memory Integrity / HVCI and all driver lifecycle paths.
- Test sleep/resume, hibernate, fast startup, radio off/on, repeated reconnect, install/upgrade, rollback, and uninstall.
- Test the games and Windows versions the release would actually claim to support.
- Publish only a Microsoft-signed package with clear compatibility and rollback instructions.

## References

Microsoft:
- [Register for the Microsoft Windows Hardware Developer Program](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/register-for-the-hardware-program)
- [Driver signing options and best practices](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings)
- [Windows Driver Policy](https://support.microsoft.com/en-us/windows/hardware/drivers/the-windows-driver-policy)
- [Driver compatibility with memory integrity and VBS](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/driver-compatibility-with-device-guard)
- [Implement memory-integrity-compatible code](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/implement-hvci-compatible-code)

Riot:
- [Restore Windows Security to Normal State](https://support.riotgames.com/en-us/riot/security/restore-windows-security-to-normal-state)
- [Vanguard Security Requirements](https://support.riotgames.com/en-us/riot/performance/vanguard-security-requirements)
- [Enable Memory Integrity (HVCI/VBS)](https://support.riotgames.com/en-us/riot/support-tools/enable-memory-integrity/)
- [Error VAN: RESTRICTION](https://support.riotgames.com/en-us/riot/penalties/error-van-restriction)
