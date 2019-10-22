#!/bin/bash

dir=/work/notes/tools/adb_tools

sudo ln -sf ${dir}/pull_server.sh /usr/bin/pull_server
sudo ln -sf ${dir}/ssh_server.sh /usr/bin/ssh_server
sudo ln -sf ${dir}/push_server.sh /usr/bin/push_server


sudo ln -sf ${dir}/adb_disable-verity.sh /usr/bin/adb_disable-verity
sudo ln -sf ${dir}/adb_remount.sh /usr/bin/adb_remount
sudo ln -sf ${dir}/push_server.sh /usr/bin/push_server
