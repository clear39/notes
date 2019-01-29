#   @build/envsetup.sh

function make()
{
    # $@ 表示所有参数
    _wrap_build $(get_make_command "$@") "$@" 
}



function get_make_command()
{
    # If we're in the top of an Android tree, use soong_ui.bash instead of make
    if [ -f build/soong/soong_ui.bash ]; then   #由于android p build/soong/soong_ui.bash文件存在
        # Always use the real make if -C is passed in
        echo "get_make_command" "$@" 
        for arg in "$@"; do
            if [[ $arg == -C* ]]; then   #当make 带有-C参数（后面值为对应的目录）时，则执行通用make工具
                echo command make
                return
            fi
        done
        echo build/soong/soong_ui.bash --make-mode
    else
        echo command make
    fi
}


function _wrap_build()
{
    local start_time=$(date +"%s") #获取时间戳
    echo "$@"  #当执行 make -j4  则打印 build/soong/soong_ui.bash --make-mode -j4
    "$@"        #运行命令   build/soong/soong_ui.bash --make-mode [参数...]
    local ret=$?    # $? 最后运行的命令的结束代码（返回值）
    local end_time=$(date +"%s")
    local tdiff=$(($end_time-$start_time))
    local hours=$(($tdiff / 3600 ))
    local mins=$((($tdiff % 3600) / 60))
    local secs=$(($tdiff % 60))
    local ncolors=$(tput colors 2>/dev/null)

    if [ -n "$ncolors" ] && [ $ncolors -ge 8 ]; then
        color_failed=$'\E'"[0;31m"
        color_success=$'\E'"[0;32m"
        color_reset=$'\E'"[00m"
    else
        color_failed=""
        color_success=""
        color_reset=""
    fi

    echo    #换行


    if [ $ret -eq 0 ] ; then
        echo -n "${color_success}#### build completed successfully "
    else
        echo -n "${color_failed}#### failed to build some targets "
    fi

    if [ $hours -gt 0 ] ; then
        printf "(%02g:%02g:%02g (hh:mm:ss))" $hours $mins $secs
    elif [ $mins -gt 0 ] ; then
        printf "(%02g:%02g (mm:ss))" $mins $secs
    elif [ $secs -gt 0 ] ; then
        printf "(%s seconds)" $secs
    fi

    echo " ####${color_reset}"

    echo

    return $ret
}




