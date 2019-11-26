#!/bin/bash


mkdir out
make ARCH=arm64 O=./out autolink_android_car_defconfig
make ARCH=arm64 O=./out menuconfig
