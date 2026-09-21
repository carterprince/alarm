#!/bin/bash

sudo chmod 666 /dev/ttyACM0
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 -u . && arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
