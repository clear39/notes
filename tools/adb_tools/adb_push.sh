#!/bin/bash

adb root

adb remount

adb push $1 $2

adb shell sync