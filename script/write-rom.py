#!/usr/bin/env python3

import argparse
import serial

def get_response(port):
    while True:
        response = str(port.readline(), 'ascii').rstrip()
        if len(response) == 0:
            response = "<empty>";
        print(f'<-- {response}')
        if "OK:" in response or "FAILED:" in response:
            return response


def send_file(file, port):
    response = get_response(port)
    for (i, line) in enumerate(file):
        data = line.encode('ascii')
        address=int(line[4:8], 16)
        size=int(line[2:4], 16)-3
        print(f'--> address=0x{address:x} size={size}');
        print(f'--> {line.rstrip()}')
        port.write(data)
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
    with open(args.file[0]) as file:
        send_file(file, port)

