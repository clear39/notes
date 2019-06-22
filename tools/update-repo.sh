#!/bin/bash



if [ $# -gt 0 ]; then
	echo $@
else
	echo "没有参数,请输入您的用户名！"
	exit
fi

file1=aosp-p9.0.0_2.0.0-ga.xml
file2=imx-p9.0.0_2.1.0-auto-ga.xml


command git  checkout ${file1}  ${file2}
command git pull 
command sed -i "5s/jenkins/$1/g" ${file1}
command sed -i "6,28s/jenkins/$1/g" ${file2}

command git diff  ${file1}

command git diff  ${file2}
