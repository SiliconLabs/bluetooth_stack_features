
# Adding Watchdog in NCP Mode

## Description

This example describes how to implement the Watchdog on a Bluetooth NCP firmware. The Watchdog is an EFR32 peripheral which allows to detect and reset/reboot an EFR32/BGM device when it hangs and becomes unresponsive (e.g., software stalemate).

> For more information about NCP mode in SDK v3.x, see [AN1259: Using Silicon Labs v3.x Bluetooth Stack in Network Co-Processor (NCP) mode](https://www.silabs.com/documents/public/application-notes/an1259-bt-ncp-mode-sdk-v3x.pdf). For more details about the Watchdog, see the relevant EFR32 Reference Manual.

The sample code in this project was implemented from an **NCP - Empty** project, on **Silabs Bluetooth SDK version 3.1.x**. The basic idea is to **feed the Watchdog timer** each time the **soft timer** ticks. If the soft timer stops ticking (it means that the stack stop working), the Watchdog will expire and reset the NCP.

The Watchdog has to be initialized once after reset with `WDOG_Init(&init)` and it has to be fed from time to time before the Watchdog timer expires using `WDOG_Feed()`.

The soft timer is initialized by calling the function:

```C
sl_status_t sl_bt_system_set_soft_timer(uint32_t time,
                                        uint8_t handle,
                                        uint8_t single_shot)
```

Each time the soft timer triggers, it will activate the event `sl_bt_evt_system_soft_timer_id`. 

By default in NCP mode, all of the events are passed to the host, but they can also be handled by the NCP by writing the handling code in the function:

```C
bool sl_ncp_local_evt_process(sl_bt_msg_t *evt)
```

If you don't want to pass the events to the host after the NCP handles them, change the return value of the function `sl_ncp_local_evt_process()` to `false`. In this example, the return value is kept as `true`, it mean that after the events are handle by the NCP, they are still passed to the host.

To determine whether you recovered from a crash with a Watchdog reset or whether the device was reset normally, use the function `RMU_ResetCauseGet()`.



## Gecko SDK version ##

- GSDK v4.2

## Hardware Required ##

- One WSTK board: BR4001A
- One Bluetooth radio board, e.g: BRD4162A

## Setup

1. Create a new **NCP - Empty** project.
2. Copy the attached **app.c** file into the project folder (replacing the existing one).
3. Config **Software components**, add: **Watchdog** driver and **RMU** driver (RMU is used to get the cause of the last reset).

    - Install **WDOG** component  
    ![add wdog](images/add_wdog.png)  

    - Install **RMU** component  
    ![add wdog](images/add_rmu.png)  

4. **Save and close** then the tool will auto-generate to code.

5. Build and flash the project to your devices.

## Usage

You can use **Bluetooth NCP Commander** to test the example described in this article. In the Simplicity Studio Tools bar, select **Tools** then find the **Bluetooth NCP Commander**.  

![BG Tool](images/launch_bg_tool_1.png)

Select your device and press `Connect`.  

![BG Tool](images/launch_bg_tool_3.png)

You will observe soft timer events occurring every 1 second. In the command prompt, you can stop the soft timer by sending the command `sl_bt_system_set_soft_timer(0,0,0)`, which will cause the Watchdog to reset the device after the 2 seconds timeout.

![BG Tool](images/result.png)
