/***************************************************************************//**
 * @file Program.cs
 * @version 1.0.1
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

using System;
using System.Collections;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Devices.Enumeration.Pnp;
using Windows.Storage.Streams;

namespace ble_console_ota
{
  class Program
  {
    static readonly bool DEBUG = false;
    static readonly string debugDeviceName = "SijinOTA";
    static readonly string debugPathSource = @ "C:\Users\siwoo\Dropbox (Silicon Labs)\OTA\mg12-soc-thermometer-server\application.gbl";

    static readonly int PACKET_DATA_SIZE = 20;

    static BluetoothLEDevice device;
    static DeviceWatcher watcher;
    static string deviceName;

    static bool isDeviceConnected = false;

    // Watcher configurations
    static readonly string[] requestedProperties = { "System.Devices.Aep.DeviceAddress", "System.Devices.Aep.Bluetooth.Le.IsConnectable" };

    // UUIDs for SPP
    static Guid otaServiceUuid = new Guid("1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0");
    static Guid otaControlUuid = new Guid("F7BF3564-FB6D-4E53-88A4-5E37E0326063");
    static Guid otaDataUuid = new Guid("984227F3-34FC-4045-A5D0-2C581F81A153");

    // Gatt handlers
    static GattDeviceService otaServiceHandle;
    static GattCharacteristic otaControlHandle;
    static GattCharacteristic otaDataHandle;

    #region BLE Connection Callbacks
    async static void DeviceAdded(DeviceWatcher sender, DeviceInformation deviceInfo)
    {
      // If the random device matches our device name, connect to it.
      //Console.WriteLine("Device found: " + deviceInfo.Name);
      if (deviceInfo.Name.CompareTo(deviceName) == 0) {
        // Get the bluetooth object and save it. This function will connect to the device
        device = await BluetoothLEDevice.FromIdAsync(deviceInfo.Id);
        Console.WriteLine("Connected to " + device.Name);

        isDeviceConnected = true;
      }
    }

    static void DeviceUpdated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate)
    {
    }

    static void DeviceEnumerationComplete(DeviceWatcher sender, object arg)
    {
      sender.Stop();
    }

    static void DeviceStopped(DeviceWatcher sender, object arg)
    {
      // Device has stopped and disconnected. Start searching for devices again.
      isDeviceConnected = false;
      sender.Start();
    }
    #endregion

    static void QueryForDevices()
    {
      watcher = DeviceInformation.CreateWatcher(
        BluetoothLEDevice.GetDeviceSelectorFromDeviceName(deviceName),
        requestedProperties,
        DeviceInformationKind.AssociationEndpoint);
      watcher.Added += DeviceAdded;
      watcher.Updated += DeviceUpdated;
      watcher.EnumerationCompleted += DeviceEnumerationComplete;
      watcher.Stopped += DeviceStopped;
      watcher.Start();
    }

    static async Task PrintGattdb()
    {
      GattDeviceServicesResult serviceResult = await device.GetGattServicesForUuidAsync(otaServiceUuid, BluetoothCacheMode.Uncached);
      if (serviceResult.Status == GattCommunicationStatus.Success) {
        foreach(var service in serviceResult.Services)
        {
          Console.Out.WriteLine("Service: " + service.Uuid);
          GattCharacteristicsResult charResult = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
          if (charResult.Status == GattCommunicationStatus.Success) {
            foreach(var characteristic in charResult.Characteristics)
            {
              Console.Out.WriteLine("Characteristic: " + characteristic.Uuid);
            }
          }
        }
      }
    }

    static async Task BootBLEDevice()
    {
      GattDeviceServicesResult serviceResult = await device.GetGattServicesForUuidAsync(otaServiceUuid, BluetoothCacheMode.Uncached);
      if (serviceResult.Status == GattCommunicationStatus.Success) {
        foreach(var service in serviceResult.Services)
        {
          if (service.Uuid == otaServiceUuid) {
            GattCharacteristicsResult charResult = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
            if (charResult.Status == GattCommunicationStatus.Success) {
              foreach(var characteristic in charResult.Characteristics)
              {
                if (characteristic.Uuid == otaControlUuid) {
                  await WriteCharacteristic(characteristic, 0, true);
                  isDeviceConnected = false;
                }
              }
            }
          }
        }
      }
    }

    static async Task StoreGattDb()
    {
      GattDeviceServicesResult serviceResult = await device.GetGattServicesForUuidAsync(otaServiceUuid, BluetoothCacheMode.Uncached);
      if (serviceResult.Status == GattCommunicationStatus.Success) {
        foreach(var service in serviceResult.Services)
        {
          if (service.Uuid == otaServiceUuid) {
            otaServiceHandle = service;

            GattCharacteristicsResult charResult = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
            if (charResult.Status == GattCommunicationStatus.Success) {
              foreach(var characteristic in charResult.Characteristics)
              {
                if (characteristic.Uuid == otaControlUuid) {
                  otaControlHandle = characteristic;
                } else if (characteristic.Uuid == otaDataUuid) {
                  otaDataHandle = characteristic;
                }
              }
            }
          }
        }
      }
    }

    static async Task WriteCharacteristic(GattCharacteristic characteristic, byte[] value, bool response)
    {
      GattWriteResult result;
      var rsp = GattWriteOption.WriteWithResponse;
      if (!response) {
        rsp = GattWriteOption.WriteWithoutResponse;
      }

      do {
        var writer = new DataWriter();
        writer.WriteBytes(value);
        result = await characteristic.WriteValueWithResultAsync(writer.DetachBuffer(), rsp);
      } while (result.Status != GattCommunicationStatus.Success);
    }

    static async Task WriteCharacteristic(GattCharacteristic characteristic, byte value, bool response)
    {
      byte[] data = new byte[1];
      data[0] = value;
      await WriteCharacteristic(characteristic, data, response);
    }

    static string WaitForDeviceNameInput()
    {
      if (!DEBUG) {
        Console.Out.Write("Enter device name to connect: ");
        return Console.In.ReadLine();           // blocking
      } else {
        return debugDeviceName;
      }
    }

    static async Task TransferImage()
    {
      try
      {
        using(FileStream fs = File.Open(debugPathSource,
                                        FileMode.Open))
        {
          byte[] img = new byte[fs.Length];
          int imgLength = (int)fs.Length;

          // Get entire img into buffer
          int bytesRead = 0;
          int bytesToRead = imgLength;
          while (bytesRead < imgLength) {
            int n = fs.Read(img, bytesRead, bytesToRead);
            bytesRead += n;
            bytesToRead -= n;
          }

          int offset = 0;
          while (offset < imgLength) {
            var writer = new DataWriter();

            // -------------------------------
            // Partition whole image into PACKET_DATA_SIZE packets because the OTA
            // Data caharacteristic only receives <20 bytes at a time.
            if ((imgLength - offset) >= PACKET_DATA_SIZE) {
              for (int i = 0; i < PACKET_DATA_SIZE; i++) {
                //Console.Out.Write(img[offset]);
                writer.WriteByte(img[offset++]);
              }
            }
            // -------------------------------
            // Case when the remaining bytes count are less than PACKET_DATA_SIZE and there are still
            // valid bytes left in the img.
            else if (((imgLength - offset) < PACKET_DATA_SIZE) && ((imgLength - offset) > 0)) {
              while (offset < imgLength) {
                writer.WriteByte(img[offset++]);
              }
            }
            // -------------------------------
            // All of the data has been sent if program ever reaches here.
            else {
              break;
            }

            // -------------------------------
            // Write to OTA Data characteristic
            var buffer = writer.DetachBuffer();
            GattWriteResult result;
            do {
              result = await otaDataHandle.WriteValueWithResultAsync(buffer, GattWriteOption.WriteWithResponse);
            } while (result.Status != GattCommunicationStatus.Success);

            Thread.Sleep(200);

            Console.WriteLine(offset);
          }
          Console.WriteLine(offset);
        }
      }
      catch(Exception err)
      {
        Console.WriteLine(err.Message);
      }

      Console.WriteLine("Transfer finished.");
    }

    static void Main(string[] args)
    {
      // Get device name from keyboard input
      deviceName = WaitForDeviceNameInput();

      // Start finding devices
      QueryForDevices();

      // Wait until device connects
      while (!isDeviceConnected) ;

      try
      {
        //BootBLEDevice().Wait();

        //Thread.Sleep(1500);

        //while (!isDeviceConnected) ;

        StoreGattDb().Wait();

        WriteCharacteristic(otaControlHandle, 0, true).Wait();

        TransferImage().Wait();

        WriteCharacteristic(otaControlHandle, 3, true).Wait();

        device.Dispose();
      }
      catch(Exception err)
      {
        Console.WriteLine(err.Message);
      }
      Console.WriteLine("Done");

      while (true) ;
    }
  }
}
