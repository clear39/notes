#!/bin/bash



if [ $# -gt 0 ]; then
	echo $@
else
	echo "没有参数,请输入您的用户名！"
	exit
fi

command git  checkout autolink-aosp-o8.1.0_1.2.0_8qxp-beta2.xml autolink-imx-o8.1.0_1.2.0_8qxp-beta2.xml
command git pull 
command sed -i "5s/jenkins/$1/g" autolink-aosp-o8.1.0_1.2.0_8qxp-beta2.xml
command sed -i "6,28s/jenkins/$1/g" autolink-imx-o8.1.0_1.2.0_8qxp-beta2.xml

command git diff  autolink-aosp-o8.1.0_1.2.0_8qxp-beta2.xml

command git diff  autolink-imx-o8.1.0_1.2.0_8qxp-beta2.xml
