#!/bin/bash

arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 -u . && arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
