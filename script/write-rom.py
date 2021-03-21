#!/usr/bin/env python3

import argparse
import serial

from dataclasses import dataclass

def get_response(port):
    while True:
        response = str(port.readline(), 'ascii').rstrip()
        if len(response) == 0:
            response = "<empty>";
        print(f'<-- {response}')
        if "OK:" in response or "FAILED:" in response:
            return response

def err(line_num, message):
    raise RuntimeError(f'Line {line_num} {message}')

@dataclass
class Record:
    address: int
    size: int
    line: bytes

def parse_line(line_num, line):
    if line[0:2] != 'S1':
        err(line_num, f'doesn\'t look like an SREC S1 record')
    if len(line) < 10:
        err(line_num, f'is too short ({len(line)}) chars')
    byte_count = int(line[2:4], 16)
    if len(line) != (byte_count + 2) * 2:
        err(line_num, f'length doesn\'t match byte count len={len(line)} bc={byte_count}')
    start_address = int(line[4:8], 16)
    if start_address >= 32 * 1024:
        err(line_num, f'address 0x{address:x} is outside 32k')
    size = byte_count - 3
    if size > 64:
        err(line_num, f'page size {size} is over 64 bytes')
    end_address = start_address + size - 1
    start_page = start_address >> 6
    end_page = end_address >> 6
    if start_page != end_page:
        err(line_num, f'crosses page boundary')
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
    response = get_response(port)
    for r in f.records:
        data = r.line.encode('ascii')
        print(f'--> address=0x{r.address:x} size={r.size}')
        print(f'--> {r.line}')
        port.write(data)
        port.write('\n'.encode('ascii'))
        response = get_response(port)

parser = argparse.ArgumentParser(description='Write and verify eeprom')

parser.add_argument('--port',
    default='/dev/ttyUSB0', help='Serial port device')
parser.add_argument('--speed',
    default=115200, type=int, help='Port speed in baud')
parser.add_argument('file',
    nargs=1, help='File of S1 records')

args = parser.parse_args()

with serial.Serial(args.port, args.speed, timeout=1) as port:
    with open(args.file[0]) as f:
        records = parse_file(f)
        send_file(records, port)
