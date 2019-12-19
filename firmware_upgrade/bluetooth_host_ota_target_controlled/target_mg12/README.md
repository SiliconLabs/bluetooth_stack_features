# NCP OTA Target Controlled Project #

## Summary ##

This directory is meant for the TARGET. There are two programs that need to be uploaded to the target: the application and the bootloader. The necessary .sls projects are provided to use the example as is.

## Hardware Required ##

- One BRD4162A radio board
- One BRD4001A WSTK board

## How to Port to Another Part ##

### Bootloader ###

Any bootloader with at least one storage slot will work.

### Application ###

If you want the target controlled app on a different radio chip, do the following:

1. Place all src code into newly created project.
   - In main.c, only initialization code and event handler was added. Everything else stayed the same as NCP Empty Target example.
   - In ncp_usart.h, add extern UARTDRV_Handle_t handle;
2. Get the bootloader_interface.c and bootloader_interface_storage.c from the bootloader api folder and add to the project. Make sure to include the path of the header files in the project as well.
`C:\SiliconLabs\SimplicityStudio\v4\developer\sdks\gecko_sdk_suite\v2.7\platform\bootloader\api`
3. Configure btl_config.h to match your application.
4. Import gatt.xml
