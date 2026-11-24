# Microsoft references

The implementation is based on the documented Windows Bluetooth profile-driver model and the Microsoft Windows Driver Samples repository.

## Bluetooth profile binding

Microsoft describes client-side profile drivers as the model for remote devices that advertise services and accept a connection initiated by the PC. The Bluetooth stack generates `BTHENUM` hardware IDs from the service GUID, and an INF chooses the profile driver based on those IDs.

- [Installing a Bluetooth Device](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/installing-a-bluetooth-device)
- [How to Use the Bluetooth Driver Stack](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/using-the-bluetooth-driver-stack)

The documented HID service UUID is `00001124-0000-1000-8000-00805F9B34FB`. The stack may include remote VID/PID values in the generated hardware ID, which permits a controller-specific INF match.

## L2CAP profile driver

Microsoft documents retrieving the Bluetooth profile-driver interface and submitting BRBs. For client connections, the profile driver sends `BRB_L2CA_OPEN_CHANNEL`, then closes the channel with `BRB_L2CA_CLOSE_CHANNEL`.

- [Querying for Bluetooth Interfaces](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/querying-for-bluetooth-interfaces)
- [Creating a L2CAP Client Connection to a Remote Device](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/creating-a-l2cap-client-connection-to-a-remote-device)
- [Accessing SDP Service Information](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/accessing-sdp-service-information)
- [Supporting Bluetooth Event Notifications](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/supporting-bluetooth-event-notifications)

Microsoft's sample implementation:

- [Bluetooth Echo L2CAP Profile Driver](https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/bthecho)

The sample includes a client profile driver that queries `GUID_BTHDDI_PROFILE_DRIVER_INTERFACE`, gets the remote Bluetooth address through `IOCTL_INTERNAL_BTHENUM_GET_DEVINFO`, and opens L2CAP connections using `BRB_L2CA_OPEN_CHANNEL`. It is the implementation reference for this project.

## Virtual HID

- [Write a HID Source Driver by Using Virtual HID Framework (VHF)](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/virtual-hid-framework--vhf-)
- [HID Driver Samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/human-interface-devices--hid--driver-samples)

VHF is the intended way to expose the controller's validated input reports as a virtual HID device on modern Windows. It avoids writing a bespoke HID transport minidriver for the virtual gamepad.

## Compatibility note

The Microsoft Bluetooth Echo sample and VHF docs are the starting points, not proof that the XP-Mx210BL will work with the prototype. The prototype will log actual BRB completion statuses on the user's Intel adapter before adding HIDP and VHF behavior.
