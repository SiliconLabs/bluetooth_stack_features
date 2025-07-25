# Copyright 2025 Silicon Laboratories Inc. www.silabs.com
#
# SPDX-License-Identifier: Zlib
#
# The licensor of this software is Silicon Laboratories Inc.
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

import numpy as np

class StepData:
    def __init__(self, role, subevent_tstamp, freq_comp, ref_plevel, num_antenna_paths, step_data_len, raw_step_data):
        self.role = role
        self.subevent_tstamp = int(subevent_tstamp)
        self.num_antenna_paths = int(num_antenna_paths)
        self.freq_comp = int(freq_comp)
        self.ref_plevel = int(ref_plevel)
        self.step_data_len = int(step_data_len)

        raw_bytes = bytearray.fromhex(raw_step_data)
        
        # Check that the data len matches the expected len
        self.raw_step_data_array = np.frombuffer(raw_bytes, dtype=np.uint8)      
        if self.step_data_len != len(self.raw_step_data_array):
            print("The raw step data length does not match the reported length!")
            exit(1)        
        self.parsed_step_data = {
            f"{self.role}_step_data": {},
        }
     
    def parse_step_data(self):
        """ Iterate over the step data, parse it, and save it in a dict. """
        byte_index = 0
        step_index = 0
        
        # Step through the raw procedure data
        while byte_index < self.step_data_len:
            # Start by parsing the mode-agnostic data. This should be the first 3 bytes of every step
            step_mode, byte_index = self.get_bytes(byte_index, 1)
            step_channel, byte_index = self.get_bytes(byte_index, 1)
            step_data_len, byte_index = self.get_bytes(byte_index, 1)
            
            step_data = {
                "Step_Mode": int(step_mode),
                "Step_Channel": int(step_channel),
                "Step_Data_Length": int(step_data_len)
            }
            
            # Parse the role and mode specific data
            mode_specific_data, byte_index = self.get_mode_specific_data(step_mode, byte_index)
            
            # Add the parsed data to the dict
            step_data.update(mode_specific_data)
            self.parsed_step_data[f"{self.role}_step_data"][f"step_{step_index}"] = step_data
            step_index += 1
              
    
    def get_mode_specific_data(self, step_mode, byte_index):
        """ Parse the step data according to the mode-specific data structures. This is described in BLE Core Spec V6.0 | Vol 4, Part E Section 7.7.65.44. """
        mode_specific_data = {}
        
        match step_mode:
            case 0:  # Calibration
                packet_quality, byte_index = self.get_bytes(byte_index, 1)
                packet_rssi, byte_index = self.get_bytes(byte_index, 1)
                packet_antenna, byte_index = self.get_bytes(byte_index, 1)
                
                mode_specific_data["Packet_Quality"] = int(packet_quality)
                mode_specific_data["Packet_RSSI"] = int(np.int8(packet_rssi))
                mode_specific_data["Packet_Antenna"] = int(packet_antenna)
                
                if self.role == "initiator":
                    measured_freq_offset, byte_index = self.get_bytes(byte_index, 2, True)
                    mode_specific_data["Measured_Freq_Offset"] = int(measured_freq_offset)
                    
            case 1:  # RTT
                packet_quality, byte_index = self.get_bytes(byte_index, 1)
                packet_nadm, byte_index = self.get_bytes(byte_index, 1)
                packet_rssi, byte_index = self.get_bytes(byte_index, 1)
                toa_tod, byte_index = self.get_bytes(byte_index, 2, True)
                packet_antenna, byte_index = self.get_bytes(byte_index, 1)
                
                mode_specific_data["Packet_Quality"] = int(packet_quality)
                mode_specific_data["Packet_NADM"] = int(packet_nadm)
                mode_specific_data["Packet_RSSI"] = int(np.int8(packet_rssi))
                mode_specific_data[f"ToA_ToD_{self.role.title()}"] = int(toa_tod)
                mode_specific_data["Packet_Antenna"] = int(packet_antenna)
        
            case 2:  # PBR
                antenna_permutation_index, byte_index = self.get_bytes(byte_index, 1)
                
                # Read PCT and QI for all antenna paths and the tone extension
                tone_pct_all_antennas = []
                tone_qi_all_antennas = []
                for i in range(self.num_antenna_paths + 1):
                    tone_pct, byte_index = self.get_bytes(byte_index, 3, True)  # PCT is a 24 bit value
                    tone_pct_all_antennas.append(int(tone_pct))
                    
                tone_qi_all_antennas, byte_index = self.get_bytes(byte_index, self.num_antenna_paths + 1)
                
                mode_specific_data["Antenna_Permutation_Index"] = int(antenna_permutation_index)
                mode_specific_data["Tone_PCT"] = tone_pct_all_antennas
                mode_specific_data["Tone_Quality_Indicator"] = tone_qi_all_antennas.tolist()
                
            case _:
                pass
        
        return mode_specific_data, byte_index
                        
    def get_bytes(self, index, num_of_bytes, combine=False):
        """ Read the byte from a specific index in the procedure data. Multi-byte values can either be combined into little endian int
            or returned as an array. Return the data and the incremented index. """
        data = self.raw_step_data_array[index:index+num_of_bytes]
        if num_of_bytes > 1:
            if combine == True:  # Otherwise just return the array
                data = combine_bytes(data)
        else:
            data = data[0]
        index += num_of_bytes
        return data, index
    
# ------------------ UTILS ------------------ #
    
def combine_bytes(byte_array):
    """ Combine bytes into a single integer (little-endian). """
    combined = np.uint32(0)
    for i in range(len(byte_array)):
        combined = (combined << 8) | byte_array[-(i+1)]  # Start from the back as the combined byte should be in little-endian
    return combined