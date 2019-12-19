# OTA from Windows over BLE #

## Summary ##

This example shows how to perform an OTA update over BLE from a Windows PC. This project uses Microsoft's C# Visual Studio IDE and the Windows Bluetooth API.

## Hardware Required ##

- One radio board which can run the OTA firmware update example
- One BRD4001A WSTK board
- A Windows PC

## Setup ##

Review this [knowledge base article](https://www.silabs.com/community/wireless/bluetooth/knowledge-base.entry.html/2018/11/06/implementing_otafir-Lqwk), follow the steps to build the attached project and bootloader, and flash them to the radio board. This allows the device to receive OTA update files. Instead of using the Blue Gecko mobile application or BLED112 dongle as the OTA client, launch the executable provided with this project. Type in the name of the device and the absolute path to the .gbl file to be transferred to the device. The program should begin transferring the .gbl to the device, and once the transfer is complete the device should update its firmware.

## How It Works ##

On the PC:

1. User types name of the device to connect to
2. Windows start watching for the device name
3. Windows connects to the BLE device
4. Windows writes zero to Control Characteristic so the device can go into bootloader mode
5. Windows transfers the .gbl file 20 bytes at a time to Data Characteristic
6. Windows writes three to Control Characteristic

The program writing three to the control characteristic signals to the device that the transfer is complete. The device then reboots into the Gecko Bootloader and writes the stored .gbl file into application space, upgrading the device's firmware.
