# BLE Low-power Beacon example #

## Summary ##

This project shows the implementation of a Bluetooth Low-Power beacon. This example also demonstrates the Energy Profiler to measure the current consumption.

## Gecko SDK version ##

v2.7

## Hardware Required ##

- Thunderboard EFR32BG22 or
- EFR32xG22 Bluetooth Starter Kit
- Mobile Phone (optional)

## Setup ##

To test the Low-power beacon application, the Thunderboard needs to be programmed with the bluetooth_soc-BLELPBeacon_bg22.sls project.

*Optional
Download the **Blue Gecko** App for your specific mobile phone OS (iOS, Android)*

## How It Works ##

Of the 40 channels in BLE, three channels (37, 38, and 39) are reserved for broadcasting advertising packets that contain information about the broadcasting node. These three channels are known as primary advertising channels. Beacons work by taking advantage of Bluetooth’s ability to broadcast packets with a small amount of customizable embedded data on these advertising channels.
By default, the Bluetooth stack advertises on all three of the primary advertising channels.

To save power consumption the BLE Low-power example deploys the following techniques:

1. Usage of only one advertising channel
2. Increase advertising interval to 1 sec
3. Transmit output power is set to 0dBm (6dBm default)
4. Reduction of transmit payload to 4 bytes
5. Set up of device to non-connectable to avoid RX mode
6. Disablement of debug features

## .sls Projects Used ##

- bluetooth_soc-BLELPBeacon_bg22.sls

## How to Port to Another Part ##

Open the "Project Properties" and navigate to the "C/C++ Build -> Board/Part/SDK" item. Select the new board or part to target and "Apply" the changes. Note: there may be dependencies that need to be resolved when changing the target architecture.
