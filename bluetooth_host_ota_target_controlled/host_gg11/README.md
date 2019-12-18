# NCP OTA Target Controlled Project #

## Summary ##

This directory is meant for the HOST. The host only requires the bootloader. Any application can be programmed because the host does not control its own bootloader in this example. Import the bootloader_storage_internal_single_2048k_gg11.sls to Simplicity to use as is.

## Tested Hardware ##

- One SLSTK3701A EFM32 Giant Gecko GG11 Starter Kit

## How to Port to Another Part ##

To create a new bootloader for the host, perform the following steps:

1. Create new bootloader project for your part. Storage slots are not required.
2. In the plugins tab of the isc file, select the following:
   - Communication / UART XMODEM
   - Communication / XMODEM Parser
   - Drivers / Delay
   - Drivers / UART
   - Utils / GPIO Activation
3. Under Drivers / UART, USART4 pins had to be changed to Port I because the default pins were not accessible on the starter kit. The target application uses the following Port I pins:
   - PI4 - CTS
   - PI5 - RTS
   - PI6 - TX
   - PI7 - RX
4. USART4 configured for UART HW flow control because the target's default UARTDrv has HW flow control configured.
5. Under Utils / GPIO Activation, change pin to PC10. The original pin (PC8) was connected to button 0 which is not accessible.
