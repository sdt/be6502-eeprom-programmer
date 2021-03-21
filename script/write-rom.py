#!/usr/bin/env python3

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
        print(f'--> {line.rstrip()}')
        port.write(data)
        response = get_response(port)

with serial.Serial('/dev/ttyUSB0', 9600, timeout=1) as port:
    with open('test.srec') as file:
        send_file(file, port)

