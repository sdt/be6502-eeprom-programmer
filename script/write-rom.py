#!/usr/bin/env python3

import argparse
import os
import re
import serial
import sys

from dataclasses import dataclass

# We raise this exception if we get a RESET response from the arduino. That
# means the arduino was resetting, and has just come back up. When we detect
# that condition, we call `stty -F $PORT -hupcl` to fix it for next time, then
# start the BEGIN sequence afresh.
class ResetException(Exception):
    pass

verbose = False

def printv(string):
    if verbose:
        print(string)

def printq(string):
    if not verbose:
        print(string, end='', flush=True)

@dataclass
class DataResponse:
    address: int
    size: int

def check_page(expected, match):
    got = DataResponse(int(match[2], 16), int(match[3]))
    if expected.size != got.size:
        raise RuntimeError(f'Expected {expected.size} bytes, got {got.size}')
    if expected.address != got.address:
        raise RuntimeError(f'Expected address 0x{expected.address:x} , got 0x{got.address:x}')

def get_response(port):
    while True:
        response = str(port.readline(), 'ascii').rstrip()
        if len(response) == 0:
            printv(f'<-- []')
            continue
        printv(f'<-- {response}')
        if "MSG:" in response:
            print(response)
            continue
        if "ACK:" in response:
            return response
        if "NAK:" in response:
            raise RuntimeError(response)
        if response == 'RESET':
            raise ResetException
        raise RuntimeError(f'Unexpected response: {response}')

def expect(port, regex, description):
    response = get_response(port)
    match = re.match(regex, response)
    if match is None:
        raise RuntimeError(f'Unexpected {description} response: {response}')
    return match

def expect_ack(port, ack):
    response = get_response(port)
    if response != 'ACK:' + ack:
        raise RuntimeError(f'Expected ACK:{ack}, got: {response}')
    return

def send(port, string):
    printv(f'--> {string}')
    port.write(string.encode('ascii'))
    port.write('\n'.encode('ascii'))

def file_err(line_num, message):
    raise RuntimeError(f'Line {line_num} {message}')

@dataclass
class Record:
    address: int
    size: int
    line: bytes

def parse_line(line_num, line):
    if line[0:2] != 'S1':
        file_err(line_num, f'doesn\'t look like an SREC S1 record')
    if len(line) < 10:
        file_err(line_num, f'is too short ({len(line)}) chars')
    byte_count = int(line[2:4], 16)
    if len(line) != (byte_count + 2) * 2:
        file_err(line_num, f'length doesn\'t match byte count len={len(line)} bc={byte_count}')
    start_address = int(line[4:8], 16)
    if start_address >= 32 * 1024:
        file_err(line_num, f'address 0x{address:x} is outside 32k')
    size = byte_count - 3
    if size > 64:
        file_err(line_num, f'page size {size} is over 64 bytes')
    end_address = start_address + size - 1
    start_page = start_address >> 6
    end_page = end_address >> 6
    if start_page != end_page:
        file_err(line_num, f'crosses page boundary')
    return Record(start_address, size, line)

@dataclass
class ROM:
    size: int
    pages: int
    records: [Record]

def parse_file(f):
    size = 0
    pages = 0
    records = []
    for (line_num, line) in enumerate(f):
        record = parse_line(line_num, line.rstrip())
        size  += record.size
        pages += 1
        records.append(record)
    return ROM(size, pages, records)

def send_file(f, port, verify):
    verb = "Verifying" if verify else "Writing"
    prefix = "V" if verify else "W"
    print(f'{verb} {f.size} bytes in {f.pages} pages')
    updated = 0
    for record in f.records:
        data = record.line
        printv(f'Sending page: address=0x{record.address:x} size={record.size}')
        printq('>\b')
        send(port, prefix + data)
        match = expect(port, r'^ACK:([WV]):([0-9A-Z]+):(\d+)$', 'page response')
        printq('<\b')
        check_page(record, match)
        if match[1] == "V":
            if verify:
                printq('v')
            else:
                printq('.')
        else:
            printq('W')
            updated = updated + 1
    printq('\n')
    return updated

parser = argparse.ArgumentParser(description='Write and verify eeprom')


parser.add_argument('--erase',
    default=False, action="store_true", help='Erase chip')
parser.add_argument('--port',
    default='/dev/ttyUSB0', help='Serial port device')
parser.add_argument('--speed',
    default=115200, type=int, help='Port speed in baud')
parser.add_argument('--verbose',
    default=False, action="store_true", help='Verbose messages')
parser.add_argument('file',
    nargs='?', help='File of S1 records')

args = parser.parse_args()
verbose = args.verbose

try:
    with serial.Serial(args.port, args.speed, timeout=1) as port:

        ready = False
        while not ready:
            try:
                send(port, 'BEGIN')
                expect_ack(port, 'BEGIN')
                ready = True
            except ResetException:
                print("Ignoring RESET, resending BEGIN")
                # Fix the serial port setup and retry
                os.system(f'stty -F {args.port} -hupcl')

        if args.erase:
            send(port, 'ERASE')
            expect_ack(port, 'ERASE')

        if args.file is not None:
            with open(args.file) as f:
                records = parse_file(f)
                # Send all the records in update mode
                updated = send_file(records, port, False)
                # If any got changed, verify them all
                if updated > 0:
                    send_file(records, port, True)
        elif not args.erase:
            print("No file specified, and not erasing. Nothing to do.")

        send(port, 'END')
        expect_ack(port, 'END')
        print("Done")

except Exception as e:
    prefix = "NAK:"
    msg = str(e)
    msg = msg[len(prefix):] if msg.startswith(prefix) else msg
    print(msg)
    sys.exit(1)
