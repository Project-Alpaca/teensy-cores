#!/bin/bash
# Generates patch for use with automated library patcher in FT-Controller-FW

git diff master..usb-ds4 -- \
    ':(exclude,top)scripts/*' \
    ':(exclude,top).github/*' \
    ':(exclude,top).gitignore' \
    ':(exclude,top)README.md' \
    ':(exclude,top)keywords.txt' > 00-teensy-cores.patch
