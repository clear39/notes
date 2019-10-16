#!/bin/bash
adb kill-server
adb root
adb disable-verity
adb reboot
