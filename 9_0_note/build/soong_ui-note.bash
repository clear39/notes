# @build/soong/soong_ui.bash

#执行命令为 build/soong/soong_ui.bash --make-mode -j4

# Save the current PWD for use in soong_ui
export ORIGINAL_PWD=${PWD}  #导入变量 ORIGINAL_PWD 为当前路径，我这里/work/workcodes/aosp-p9.x-auto-alpha/build/soong/
export TOP=$(gettop)    
source ${TOP}/build/soong/scripts/microfactory.bash

soong_build_go soong_ui android/soong/cmd/soong_ui

cd ${TOP}

# exce "out/soong_ui" “--make-mode -j4“
exec "$(getoutdir)/soong_ui" "$@"






#   查找更目录
function gettop
{
    local TOPFILE=build/soong/root.bp   # 定义局部变量；  build/soong/root.bp该文件存在，该文件没有任何作用，只是用来查找根目录
    echo "gettop" "${TOP-}"
    if [ -z "${TOP-}" -a -f "${TOP-}/${TOPFILE}" ] ; then
        # The following circumlocution ensures we remove symlinks from TOP.
        (cd $TOP; PWD= /bin/pwd)
    else
        if [ -f $TOPFILE ] ; then
            # The following circumlocution (repeated below as well) ensures
            # that we record the true directory name and not one that is
            # faked up with symlink names.
            PWD= /bin/pwd
        else
            local HERE=$PWD
            T=
            while [ \( ! \( -f $TOPFILE \) \) -a \( $PWD != "/" \) ]; do
                \cd ..
                T=`PWD= /bin/pwd -P`
            done
            \cd $HERE
            if [ -f "$T/$TOPFILE" ]; then
                echo $T
            fi
        fi
    fi
}