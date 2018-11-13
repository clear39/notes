//	@build/soong/soong_ui.bash


# Save the current PWD for use in soong_ui
export ORIGINAL_PWD=${PWD}
export TOP=$(gettop)

echo $TOP	#	/work/workcodes/imx8-android-o

#这里主要导入build_go 和 getoutdir 函数,已经GOROOT(=GOROOT="${TOP}/prebuilts/go/linux-x86/")环境变量
source ${TOP}/build/soong/cmd/microfactory/microfactory.bash

build_go soong_ui android/soong/cmd/soong_ui

cd ${TOP}
# exec "out/soong_ui" --make-mode build/soong/soong_ui.bash --make-mode art
exec "$(getoutdir)/soong_ui" "$@"


//build/soong/cmd/microfactory/microfactory.bash//////////////////////////////
#调用	build_go soong_ui android/soong/cmd/soong_ui
# Arguments:
#  $1: name of the requested binary	   #soong_ui
#  $2: package name								  #android/soong/cmd/soong_ui
function build_go
{
    # Increment when microfactory changes enough that it cannot rebuild itself.
    # For example, if we use a new command line argument that doesn't work on older versions.
    local mf_version=2

    local mf_src="${TOP}/build/soong/cmd/microfactory"

    local out_dir=$(getoutdir)
    local mf_bin="${out_dir}/microfactory_$(uname)"	#out/microfactory_Linux
    local mf_version_file="${out_dir}/.microfactory_$(uname)_version"	#out/.microfactory_Linux_version
    local built_bin="${out_dir}/$1"			#out/soong_ui
    local from_src=1

    if [ -f "${mf_bin}" ] && [ -f "${mf_version_file}" ]; then
    	#如果"$(cat "${mf_version_file}")"  结果为 2
        if [ "${mf_version}" -eq "$(cat "${mf_version_file}")" ]; then
            from_src=0
        fi
    fi

    local mf_cmd
    if [ $from_src -eq 1 ]; then
    	#${TOP}/prebuilts/go/linux-x86/bin/go 		run 		${TOP}/build/soong/cmd/microfactory/microfactory.go
        mf_cmd="${GOROOT}/bin/go run ${mf_src}/microfactory.go"
    else
        mf_cmd="${mf_bin}"				#out/microfactory_Linux
    fi

    #当第一次编译的时候mf_cmd="${GOROOT}/bin/go run ${mf_src}/microfactory.go",后面直接执行 mf_cmd="${mf_bin}"				#out/microfactory_Linux

    rm -f "${out_dir}/.$1.trace"	#out/.soong_ui.trace

    #	${mf_cmd}   -s  ${TOP}/build/soong/cmd/microfactory 		-b 	out/microfactory_Linux 			--pkg-path "android/soong=${TOP}/build/soong"		-trimpath "${TOP}/build/soong"  -o out/soong_ui  android/soong/cmd/soong_ui
    ${mf_cmd} -s "${mf_src}" -b "${mf_bin}" \
            -pkg-path "android/soong=${TOP}/build/soong" -trimpath "${TOP}/build/soong" \
            -o "${built_bin}" $2

    if [ $from_src -eq 1 ]; then
        echo "${mf_version}" >"${mf_version_file}"
    fi
}