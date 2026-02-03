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

import argparse
from StepData import *
import json

def parse_data_from_file(file_path):
    """ Parse the raw device log to separate dicts for the initiator and reflector. """
    with open(file_path, 'r') as file:
        data = file.read()

    # Split the data into initiator and reflector
    sections = data.split("Reflector:")

    # Parse the keys/values from the log
    def extract_values(section):
        values = {}
        lines = section.strip().split("\n")
        for line in lines:
            if ":" in line:
                key, value = line.split(":", 1)
                values[key.strip()] = value.strip()
        return values

    initiator_data = extract_values(sections[0].split("Initiator:")[1])
    reflector_data = extract_values(sections[1])

    # Return the dicts
    return initiator_data, reflector_data


def write_step_data_to_file(step_data, filename, mode):
    """ Save the parsed step data into json-file. """
    json_step_data = json.dumps(step_data, indent=4)
    with open(filename, mode) as json_file:
        json_file.write(json_step_data + '\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Parse the raw procedure data from a log file, and save parsed data to json-file.")

    # Add arguments
    parser.add_argument(
        "input_file",
        type=str,
        help="Path to the log file containing raw step data."
    )
    parser.add_argument(
        "output_file",
        type=str,
        help="Path to the output json-file."
    )
    args = parser.parse_args()

    # Start by parsing the input file. Make sure the input file has a similar format as example_log_*.txt
    print("Parsing the input file...")
    initiator_data, reflector_data = parse_data_from_file(args.input_file)

    # Create StepData objects for the initiator and reflector based on the parsed file
    initiator_step_data = StepData(
        role = "initiator",
        subevent_tstamp=initiator_data['subevent_timestamps_ms'],
        freq_comp=initiator_data['Frequency compensation'],
        ref_plevel=initiator_data['Reference power level'],
        num_antenna_paths=initiator_data['Number of antenna paths'],
        step_data_len=initiator_data['Step data count'],
        raw_step_data=initiator_data['Step data']
    )

    reflector_step_data = StepData(
        role = "reflector",
        subevent_tstamp=reflector_data['subevent_timestamps_ms'],
        freq_comp=reflector_data['Frequency compensation'],
        ref_plevel=reflector_data['Reference power level'],
        num_antenna_paths=reflector_data['Number of antenna paths'],
        step_data_len=reflector_data['Step data count'],
        raw_step_data=reflector_data['Step data']
    )

    # Parse the raw step data
    print("Parsing the raw step data to json...")
    initiator_step_data.parse_step_data()
    reflector_step_data.parse_step_data()

    # Write the step data to a json-file
    cs_procedure = {
        "cs_procedure": [initiator_step_data.parsed_step_data, reflector_step_data.parsed_step_data]
    }

    write_step_data_to_file(cs_procedure, args.output_file, 'w')
    print(f"Parsed step data written to {args.output_file}.")
    print("Done!")
