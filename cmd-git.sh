#!/bin/bash



function push(){
    git add .
    git commit -m "$1"
    git push origin master
}

function pull(){
    git pull origin master
}



# 单个字母前带冒号表示参数的值不是必须的 前面有冒号表示参数必须有选项的值
while getopts 'd::ft:' OPT; do
    case ${OPT} in
        d)
            DEL_DAYS="$OPTARG";;
        f)
            DIR_FROM="$OPTARG";;
        t)
            DIR_TO="$OPTARG";;
        ?)
            echo "Usage: `basename $0` [options] filename"
     esac
done
