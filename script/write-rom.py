#!/usr/bin/env python3

import argparse
import re
import serial

from dataclasses import dataclass

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
    message: str

def parse_line_response(response):
    m = re.match(r'^OK: \[(.*)\] address=0x([0-9A-Z]+) size=(\d+)$', response)
    if m is None:
        raise RuntimeError(f'Unexpected OK response: {response}')
    return DataResponse(int(m[2], 16), int(m[3]), m[1])

def check_response(line, expected):
    got = parse_line_response(line)
    if expected.size != got.size:
        raise RuntimeError(f'Expected {expected.size} bytes, got {got.size}')
    if expected.address != got.address:
        raise RuntimeError(f'Expected address 0x{expected.address:x} , got 0x{got.address:x}')
    return got

def get_response(port):
    while True:
        response = str(port.readline(), 'ascii').rstrip()
        if len(response) == 0:
            response = "<empty>";
        printv(f'<-- {response}')
        if "FAILED:" in response:
            raise RuntimeError(response)
        if "OK:" in response:
            return response

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

def send_file(f, port):
    print(f'Writing {f.size} bytes in {f.pages} pages')
    response = get_response(port)
    for record in f.records:
        data = record.line.encode('ascii')
        printv(f'--> address=0x{record.address:x} size={record.size}')
        printv(f'--> {record.line}')
        printq('>\b')
        port.write(data)
        port.write('\n'.encode('ascii'))
        printq('<\b')
        response = check_response(get_response(port), record)
        if not args.verbose:
            if response.message == "no changes":
                printq('.')
            elif response.message == "updated ok":
                printq('W')
            else:
                printq(f'?[{response.message}]')
    printq('\n')
    print(f'Done')

parser = argparse.ArgumentParser(description='Write and verify eeprom')

parser.add_argument('--port',
    default='/dev/ttyUSB0', help='Serial port device')
parser.add_argument('--speed',
    default=115200, type=int, help='Port speed in baud')
parser.add_argument('--verbose',
    default=False, action="store_true", help='Verbose messages')
parser.add_argument('file',
    nargs=1, help='File of S1 records')

args = parser.parse_args()
verbose = args.verbose

with serial.Serial(args.port, args.speed, timeout=1) as port:
    with open(args.file[0]) as f:
        records = parse_file(f)
        send_file(records, port)
