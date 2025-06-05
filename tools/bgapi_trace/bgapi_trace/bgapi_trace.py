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
import time
import datetime
import sys
import fnmatch
import pylink
import importlib.metadata
from bgapi.serdeser import Deserializer, HEADER_LENGTH, MSG_COMMAND
from bgapi.bglib import BGCommand, BGResponse, BGEvent
from bgapi.apiparser import ParsedApi

class TraceFilter(object):
    FILTER_RULE_INCLUDE = '+'
    FILTER_RULE_EXCLUDE = '-'
    FILTER_RULE_MERGE = 'merge'

    def __init__(self):
        self.rules = []

    def add_rule(self, rule):
        # Split the string to drop everything that follows the comment character "#" and strip
        # leading and trailing whitespace
        rule = rule.split("#", 1)[0].strip()

        # Empty lines are ignored
        if rule == "":
            return

        # Split by first whitespace
        parts = rule.split(None, 1)
        if len(parts) < 2:
            raise ValueError(f"Badly formatted rule '{rule}'")
        rule_type = parts[0]
        pattern = parts[1]

        if rule_type == self.FILTER_RULE_INCLUDE or rule_type == self.FILTER_RULE_EXCLUDE:
            # The "leaf" rules are pushed to the array as a tuple
            self.rules.append((rule_type, pattern))
        elif rule_type == self.FILTER_RULE_MERGE:
            # Merge rules specify a filename to read more rules from
            try:
                with open(pattern, 'r') as f:
                    for line in f.read().splitlines():
                        self.add_rule(line)
            except Exception as e:
                print(f"ERROR: Failed to read filter rules from '{pattern}': {e}")
                sys.exit()
        else:
            # Anything else is invalid
            raise ValueError(f"Invalid filter rule type '{rule_type}'")

    def is_included(self, message):
        # Iterate over all rules and check for matches. The first rule that matches decides what to
        # do with the message.
        for rule_type, pattern in self.rules:
            if fnmatch.fnmatchcase(message, pattern):
                return rule_type == self.FILTER_RULE_INCLUDE

        # No rule matched. Default to including the message in the output.
        return True

class TraceReader(object):
    BGABI_DEBUG_DEVICE_TYPE = 6
    MESSAGE_TYPE_COMMAND = 0
    MESSAGE_TYPE_RESPONSE = 1
    MESSAGE_TYPE_EVENT = 2
    MESSAGE_TYPE_DEBUG = 0xFF

    def __init__(self, api_xmls):
        self.apis = [ParsedApi(a) for a in api_xmls]
        self.prev_output_timestamp_us = None
        self.deserializer = Deserializer(self.apis)

        # If the BGAPI debug device API is available, find the IDs and constants for the special
        # events emitted by the BGAPI debug device
        self.trace_metadata_event_ids = None
        self.custom_message_event_ids = None
        self.sync_event_ids = None
        for api in self.apis:
            if api.device_id == self.BGABI_DEBUG_DEVICE_TYPE:
                trace_class = api['trace']
                metadata_event = trace_class.events['message_metadata']
                self.trace_metadata_event_ids = (api.device_id, trace_class.index, metadata_event.index)
                message_type_enum = trace_class.enums['message_type']
                self.MESSAGE_TYPE_COMMAND = message_type_enum['message_type_command'].value
                self.MESSAGE_TYPE_RESPONSE = message_type_enum['message_type_response'].value
                self.MESSAGE_TYPE_EVENT = message_type_enum['message_type_event'].value

                # The custom message and sync events are available only in newer SDKs
                if 'custom_message' in trace_class.events:
                    custom_message_event = trace_class.events['custom_message']
                    self.custom_message_event_ids = (api.device_id, trace_class.index, custom_message_event.index)
                if 'sync' in trace_class.events:
                    sync_event = trace_class.events['sync']
                    self.sync_event_ids = (api.device_id, trace_class.index, sync_event.index)
                break

    def _output_message(self, msg, timestamp_us=None):
        if timestamp_us is not None:
            timestamp = str(datetime.timedelta(microseconds=timestamp_us))
            if self.prev_output_timestamp_us is not None:
                timedelta_us = timestamp_us - self.prev_output_timestamp_us
                if timedelta_us < 60*1000000:
                    timestamp += f" (+{timedelta_us / 1000000:6f})"
                else:
                    timestamp += f" (+{str(datetime.timedelta(microseconds=timedelta_us))})"
            print(f"{timestamp} {msg}")
            self.prev_output_timestamp_us = timestamp_us
        else:
            self.prev_output_timestamp_us = None
            print(f"{msg}")

    def _output_warning(self, msg):
        print(f"WARNING: {msg}")

    def _read_trace(self, trace_filter):
        try:
            command_stack = []
            message_type = None
            last_known_timestamp_us = None
            message_timestamp_us = None
            failed_devices = {}
            debug_event_decode_failed = False
            saw_unknown_debug_event = False
            while True:
                # Read the BGAPI message header
                header_len = HEADER_LENGTH
                header, receive_time_us = self._read_bytes(header_len)
                if not header:
                    break

                (type, device_id, payload_len, class_id, command_id) = self.deserializer.parseHeader(header)
                header_ids = (device_id, class_id, command_id)
                if payload_len > 0:
                    payload = self._read_bytes(payload_len)[0]
                    if not payload:
                        break
                else:
                    payload = bytes()

                # Always parse the events emitted by the BGAPI debug device
                if device_id == self.BGABI_DEBUG_DEVICE_TYPE:
                    try:
                        (apicmdevt, headers, params) = self.deserializer.parse(header, payload, False)
                        msg = BGEvent(apicmdevt, params)
                    except Exception as e:
                        # Output a warning on the first time a decode has failed
                        if not debug_event_decode_failed:
                            debug_event_decode_failed = True
                            saw_unknown_debug_event = True
                            self._output_warning("Failed to decode a debug event. Make sure 'sli_bgapi_debug.xapi' is up to date.")
                            failed_devices[device_id] = device_id

                    if header_ids == self.trace_metadata_event_ids:
                        # BGAPI trace metadata events provide the metadata for the BGAPI message
                        # that follows. If we see the metadata event, just pick the values and
                        # continue to the next message from the stream.
                        message_type = msg.type
                        message_timestamp_us = msg.timestamp_us
                        continue
                    elif header_ids == self.custom_message_event_ids:
                        # Custom messages are formatted directly for cleaner output
                        message_type = self.MESSAGE_TYPE_DEBUG
                        message_timestamp_us = msg.timestamp_us
                        prefix = '< log'
                        msg = msg.message
                    elif header_ids == self.sync_event_ids:
                        # Sync message is formatted directly for cleaner output
                        message_type = self.MESSAGE_TYPE_DEBUG
                        message_timestamp_us = msg.timestamp_us
                        prefix = '< sync'
                        msg = ''
                    else:
                        # Output a warning on the first time we see unknown debug events
                        if not saw_unknown_debug_event:
                            saw_unknown_debug_event = True
                            self._output_warning("Received an unknown debug event. Make sure this tool is up to date.")

                # The target sets timestamp to zero if a timestamp could not be obtained
                if message_timestamp_us == 0:
                    message_timestamp_us = None

                # If a timestamp is not known from metadata, use the timestamp (if any) provided by
                # the stream we're reading from
                if message_timestamp_us is None:
                    # Calculate an estimated delta timestamp if we can. Otherwise just use the
                    # receive timestamp as is.
                    if last_known_timestamp_us is not None and last_known_receive_time_us is not None:
                        delta_receive_time_us = receive_time_us - last_known_receive_time_us
                        message_timestamp_us = last_known_timestamp_us + delta_receive_time_us
                    else:
                        message_timestamp_us = receive_time_us

                # Print a warning if the timestamp has gone backwards
                if message_timestamp_us is not None:
                    if last_known_timestamp_us is not None and message_timestamp_us < last_known_timestamp_us:
                        self._output_warning("Timestamp has gone backwards. The device has probably reset.")
                    last_known_timestamp_us = message_timestamp_us
                    last_known_receive_time_us = receive_time_us

                # Messages that were identified as a known debug message were already formatted
                # above. The standard decoding and formatting is only done for other message types.
                if message_type != self.MESSAGE_TYPE_DEBUG:
                    # If the message type is not known from the metadata event, we need some
                    # heuristics to figure out if command-type messages are actually commands or
                    # responses. Responses use the same IDs as the corresponding command. BGAPI
                    # commands can call other commands in other BGAPI devices but they can't be
                    # recursive to the same command. We assume the first command-type message is a
                    # command and track from there. If we see the same ID we've seen for a command,
                    # it's most likely a response. If we see a different ID, it's most likely a
                    # nested command.
                    if message_type is None:
                        if type == MSG_COMMAND:
                            if len(command_stack) == 0 or command_stack[-1]['ids'] != header_ids:
                                # Looks like the start of a command. We'll push it
                                # to the stack after we've parsed it if a response
                                # is expected.
                                message_type = self.MESSAGE_TYPE_COMMAND
                            else:
                                # Looks like a response
                                message_type = self.MESSAGE_TYPE_RESPONSE
                        else:
                            # Is an event
                            message_type = self.MESSAGE_TYPE_EVENT

                    from_host = message_type == self.MESSAGE_TYPE_COMMAND
                    try:
                        (apicmdevt, headers, params) = self.deserializer.parse(header, payload, from_host)
                    except Exception as e:
                        # Output a warning on the first time we see an error for a device
                        if device_id not in failed_devices:
                            # The BGAPI debug device is a bit special, so have a specific warning
                            # for that
                            if device_id == self.BGABI_DEBUG_DEVICE_TYPE:
                                self._output_warning("Device emitted debug metadata but the XAPI definition is not available.")
                                self._output_warning("Please specify the 'sli_bgapi_debug.xapi' location with '-a' option.")
                            else:
                                self._output_warning(f"Decode for device {device_id}, class {class_id}, command {command_id} failed with error '{e}'.")
                                self._output_warning("Make sure up to data XAPI files for all APIs are supplied with '-a' option.")
                            failed_devices[device_id] = device_id
                        apicmdevt = None

                    # Decode the message first. If we couldn't decode it, use raw output.
                    if apicmdevt is None:
                        msg = f"{header + payload}"
                    else:
                        if message_type == self.MESSAGE_TYPE_COMMAND:
                            msg = str(BGCommand(apicmdevt, params))
                        elif message_type == self.MESSAGE_TYPE_RESPONSE:
                            msg = str(BGResponse(apicmdevt, params))
                        else:
                            msg = str(BGEvent(apicmdevt, params))

                    # Select the output prefix and manage the command stack
                    if message_type == self.MESSAGE_TYPE_COMMAND:
                        prefix = '> command'
                        # A response is expected only if the command returns something. If we
                        # couldn't parse the command, assume it has a response.
                        if apicmdevt is None or not apicmdevt.no_return:
                            started_command = { 'message': msg, 'ids': header_ids }
                            command_stack.append(started_command)
                    elif message_type == self.MESSAGE_TYPE_RESPONSE:
                        prefix = '< response'
                        if len(command_stack) == 0:
                            self._output_warning("Received response when no command was in progress:")
                        else:
                            completed_command = command_stack.pop()
                            if completed_command['ids'] != header_ids:
                                self._output_warning(f"Expected response to {completed_command['message']}, received unexpected response:")
                    else:
                        prefix = '< event'

                # Finally output what we got and prepare for the next message
                if trace_filter.is_included(msg):
                    self._output_message(f"{prefix:11s}{msg}", message_timestamp_us)
                message_type = None
                message_timestamp_us = None

        except KeyboardInterrupt:
            pass

class RttReader(TraceReader):
    def __init__(self, api_xmls, jlink, device, rtt_buffer_index):
        super(RttReader, self).__init__(api_xmls)
        try:
            self.jlink_id = int(jlink)
        except:
            # If the J-Link ID did not parse as an integer, assume it's an IP address. If the port
            # is not provided, use the default port 19020.
            if ':' not in jlink:
                self.jlink_id = jlink + ':19020'
            else:
                self.jlink_id = jlink
        self.device = device
        self.rtt_buffer_index = rtt_buffer_index

    def _read_bytes(self, bytes_to_read):
        buf = bytes()
        receive_time_us = None
        while bytes_to_read > 0:
             read_bytes = bytes(self.jlink.rtt_read(self.rtt_buffer_index, bytes_to_read))
             if len(read_bytes) > 0:
                 buf += read_bytes
                 bytes_to_read -= len(read_bytes)
                 if receive_time_us is None:
                     receive_time_us = int(time.time_ns() / 1000) - self.timestamp_epoch_us
             if bytes_to_read > 0:
                 time.sleep(0.01)
        return (buf, receive_time_us)

    def identify_rtt_index(self, buffer_name):
        # Find the number of up buffers available
        first_retry = True
        while True:
            try:
                num_up_buffers = self.jlink.rtt_get_num_up_buffers()
                break
            except pylink.JLinkRTTException as e:
                # If the RTT control block has not been found, we'll continue to poll every half a
                # second. Other errors are considered fatal.
                if e.code != pylink.JLinkRTTErrors.RTT_ERROR_CONTROL_BLOCK_NOT_FOUND:
                    raise e
                if first_retry:
                    print("Waiting for RTT control block to be found...")
                    first_retry = False
                time.sleep(0.5)

        # Find the named buffer
        for i in range(num_up_buffers):
            buf_descriptor = self.jlink.rtt_get_buf_descriptor(i, up=True)
            if buf_descriptor.name == buffer_name:
                return buf_descriptor.BufferIndex

        print(f"ERROR: Could not find RTT buffer '{buffer_name}'")
        sys.exit()

    def read_trace(self, trace_filter):
        self.jlink = pylink.JLink()
        if isinstance(self.jlink_id, int):
            self.jlink.open(serial_no=self.jlink_id)
        else:
            self.jlink.open(ip_addr=self.jlink_id)
        self.jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
        self.jlink.connect(self.device)
        self.jlink.rtt_start()
        # If a buffer index has not been provided, try to identify it automatically
        if self.rtt_buffer_index is None:
            self.rtt_buffer_index = self.identify_rtt_index('sl_bgapi_trace')
        self.timestamp_epoch_us = int(time.time_ns() / 1000)
        print(f"Start reading BGAPI Trace from RTT buffer index {self.rtt_buffer_index}")
        self._read_trace(trace_filter)

class FileReader(TraceReader):
    def __init__(self, api_xmls, filename):
        super(FileReader, self).__init__(api_xmls)
        self.filename = filename

    def _read_bytes(self, bytes_to_read):
        buf = self.file.read(bytes_to_read)
        receive_time_us = None
        return (buf, receive_time_us)

    def read_trace(self, trace_filter):
        with open(self.filename, mode="rb") as file:
            self.file = file
            self._read_trace(trace_filter)

class FilterArgAction(argparse.Action):
    def __init__(self, trace_filter, rule_type, *args, **kwargs):
        """
        argparse custom action for filter-related arguments
        """
        self.trace_filter = trace_filter
        self.rule_type = rule_type
        super(FilterArgAction, self).__init__(*args, **kwargs)

    def __call__(self, parser, namespace, values, option_string):
        # We only accept one string at a time
        if not isinstance(values, str):
            parser.error(f"{name} expects a single string.")
        self.trace_filter.add_rule(f'{self.rule_type} {values}')

def main():
    parser = argparse.ArgumentParser(description="""
    Parse BGAPI traces produced by the component 'bluetooth_utility_bgapi_trace'. To parse traces in
    realtime, use the '-j' or '--jlink' option to specify the J-Link device to use and the '-d' or
    '--device' to specify the target device. Different SDK versions may use a different RTT buffer
    index for the events. By default the tool tries to identify the buffer index based on the buffer
    name, but a specific buffer index can be forced with the '-r' or '--rtt-buffer-index' option. To
    parse traces that have been recorded to a file, use '-f' or '--file' to read from the file. This
    option could also be used to parse for example a serial port capture of BGAPI NCP communication.
    Always use '-a' or '--api' option to point the tool to all APIs that you expect to be present on
    the device, including the debug API 'sli_bgapi_debug.xapi' used by the trace mechanism for
    metadata. You can find the XAPI files in the GSDK release in 'protocol/bluetooth/api'. Use the
    optional filtering rules '-i', '-e', and '--filter' to filter the decoded trace. See README.md
    for detailed description of the filtering.
    """)
    trace_filter = TraceFilter()
    parser.add_argument('-j', '--jlink', dest='jlink', help='When reading from J-Link, the serial number or IP address of the J-Link to connect to')
    parser.add_argument('-d', '--device', dest='device', help='When reading from J-Link, the name of the target device, for example EFR32BG24AxxxF1024')
    parser.add_argument('-r', '--rtt-buffer-index', dest='rtt_buffer_index', type=int, default=None, metavar='INDEX', help='Force a specific RTT buffer index when reading from J-Link')
    parser.add_argument('-f', '--rtt-file', dest='file', metavar='FILE', help='When reading from a previously recorded file, the name of the file to read from')
    parser.add_argument('-a', '--api', dest='api_xmls', action='append', default=[], metavar='XAPI', help='Path to a XAPI file defining a device API. Use this option multiple times to parse multiple APIs.')
    parser.add_argument('-i', '--include', type=str, metavar='PATTERN', action=FilterArgAction, trace_filter=trace_filter, rule_type=TraceFilter.FILTER_RULE_INCLUDE, help='Include messages that match the specified pattern')
    parser.add_argument('-e', '--exclude', type=str, metavar='PATTERN', action=FilterArgAction, trace_filter=trace_filter, rule_type=TraceFilter.FILTER_RULE_EXCLUDE, help='Exclude messages that match the specified pattern')
    parser.add_argument('--filter', type=str, metavar='FILTERFILE', action=FilterArgAction, trace_filter=trace_filter, rule_type=TraceFilter.FILTER_RULE_MERGE, help='Read filtering rules from a file')
    parser.add_argument('-v', '--version', action='version', version=f"bgapi_trace {importlib.metadata.version('bgapi_trace')}")
    args = parser.parse_args()

    if args.jlink:
        if not args.device:
            print("ERROR: 'device' must be specified when reading from RTT")
            sys.exit(1)
        reader = RttReader(args.api_xmls, args.jlink, args.device, args.rtt_buffer_index)
    elif args.file:
        reader = FileReader(args.api_xmls, args.file)
    else:
        print("ERROR: Either 'jlink' or 'file' must be specified")
        sys.exit(1)
    reader.read_trace(trace_filter)

if __name__ == '__main__':
    main()
