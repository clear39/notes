#!/bin/bash


src=$1
dst=$1


if [ $# -ge 2 ] then
    dst=$2
fi

scp lixuqing1@192.168.3.7:/home/lixuqing1/aosp-p9.x-auto-ga/${src} ${dst}