# NCP OTA Target Controlled Project #

## Summary ##

The GG11 is the host and the MG12 is the target.
In this project, the target is responsible for setting the host to bootloader mode (using the gpio activation and reset pin) and send the image through USART to the host. The host does not have any code regarding the OTA Control and Data. That is handled inside the target device. All code related to updating the host is in the btl_app folder of this project.

## Gecko SDK Version ##

v2.7

## Hardware Required ##

- One BRD4162A radio board
- One BRD4001A WSTK board
- One SLSTK3701A EFM32 Giant Gecko GG11 Starter Kit

## Connections Required ##

Host uses USART4, target uses USART0, but these connections can be changed in btl_config.h

### USART Connections ##

| Host (GG11) | Target (MG12) |
| --- | --- |
| PI4 - CTS | PA3 - RTS |
| PI5 - RTS | PA2 - CTS |
| PI6 - TX | PA1 - RX |
| PI7 - RX | PA0 - TX |

### Additional Connections ###

| Host (GG11) | Target (MG12) |
| --- | --- |
| PC10 | PC9 |
| RST | PC10 |

## Setup ##

1. Have the USART connections plus the reset and gpio pin connections.
2. The target's bootloader must have a storage slot. Upload the Bootloader Storage Internal Single 1024k example into the target.
3. The host's bootloader uses the UART XMODEM configuration; the following needs to be altered and uploaded for the example to work:
   - USART4 pins had to be changed to Port I because the default pins were not accessible on the starter kit
   - USART4 configured for UART HW flow control because the target's default UARTDrv has HW flow control configured.
   - GPIO Activation Pin changed to PC10. The original pin (PC8) was connected to button 0 and was not accessible.
4. Upload the target's application code. This code has a bootloader handler for handling the OTA service and packaging of the XMODEM packets.
5. Upload the host's application code. This code is just default starter code that has the necessary gatt_db source files and initializes the USART for communication.

### GATT Service and Characteristics Configuration ###

The GATT OTA Control characteristic should have the property Write with Mandatory requirements to work properly with the Blue Gecko App.
The OTA Control Characteristic will either be a 0 (start OTA transfer) or 3 (end OTA Transfer) with its value type being user.
The GATT OTA Data characteristic should have the following properties:

- Write with Mandatory
- Write without response with Mandatory

## How It Works (On the Target) ##

1. Blue Gecko App writes 0 to the OTA Control Characteristic to start transfer.
2. The target will get 20 bytes of data at a time from the OTA Data characteristic and stores image into storage slot 0.
3. Keep getting data until Blue Gecko App writes 3 to the OTA Control Characteristic.
4. Once whole image has been stored (3 received), clear the GPIO activation pin (low).
5. Clear the reset pin (low)
6. Delay
7. Set the reset pin (high)
8. Wait until the host's bootloader sends the menu string i.e. wait until host's bootloader is ready to receive commands.
9. Send '1' character to host. The '1' command starts the image uploading.
10. Wait until the host's bootloader sends the 'C' command. This is part of the XMODEM File Transfer Protocol and it means the last 2 bytes of each packet must have the CRC result.
11. Send XMODEM packet. XMODEM packets only allow 128 bytes of data. If the image is not 128 byte aligned, the end is padded with 0x1A which indicates EOF.
12. Wait for ACK response from host's bootloader.
13. Repeat 9 and 10 until whole image has been sent.
14. Send EOT which indicates the end of the image.
15. Wait until the host's bootloader sends the menu string again.
16. Send '2' character to host. The '2' command makes the host start running the new application.

## .sls Projects Used ##

### Target (MG12) ###

- app_ncp_target_controlled_ota_mg12.sls
- bootloader_storage_internal_1024k_mg12.sls

### Host (GG11) ###

- bootloader_storage_internal_single_2048k_gg11.sls

## Special Notes ##

When porting the btl_app to another project, make sure add: extern UARTDRV_Handle_t handle; to the ncp_usart.h. This is to use the USARTDrv in the bootloader files.

## How to Port to Another Part ##

Go [here](./host_gg11/README.md) for instructions on how to implement the HOST code on another part.

Go [here](./target_mg12/README.md) for instructions on how to implement the TARGET code on another part.
