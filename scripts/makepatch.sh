#!/bin/bash
# Generates patch for use with automated library patcher in FT-Controller-FW

git diff master..usb-ds4 -- \
    ':(exclude)scripts/*' \
    ':(exclude).github/*' \
    ':(exclude).gitignore' \
    ':(exclude)README.md' \
    ':(exclude)keywords.txt' > 00-teensy-cores.patch
