# @build/soong/scripts/microfactory.bash

case $(uname) in
    Linux)
        export GOROOT="${TOP}/prebuilts/go/linux-x86/"  #这里导入这个变量
        ;;
    Darwin)
        export GOROOT="${TOP}/prebuilts/go/darwin-x86/"
        ;;
    *) echo "unknown OS:" $(uname) >&2 && exit 1;;
esac

# Find the output directory
function getoutdir
{
    local out_dir="${OUT_DIR-}"
    if [ -z "${out_dir}" ]; then
        if [ "${OUT_DIR_COMMON_BASE-}" ]; then
            out_dir="${OUT_DIR_COMMON_BASE}/$(basename ${TOP})"
        else
            out_dir="out"
        fi
    fi
    if [[ "${out_dir}" != /* ]]; then
        out_dir="${TOP}/${out_dir}"
    fi
    echo "${out_dir}"
}

# Bootstrap microfactory from source if necessary and use it to build the
# requested binary.
#
# Arguments:
#  $1: name of the requested binary
#  $2: package name
function soong_build_go
{
    echo "soong_build_go" "$@"       # soong_build_go soong_ui android/soong/cmd/soong_ui

    BUILDDIR=$(getoutdir) \
      SRCDIR=${TOP} \
      BLUEPRINTDIR=${TOP}/build/blueprint \
      EXTRA_ARGS="-pkg-path android/soong=${TOP}/build/soong" \
      build_go $@       #   执行命令为 build_go soong_ui android/soong/cmd/soong_ui
}

source ${TOP}/build/blueprint/microfactory/microfactory.bash  #导入build_go shell函数，如下：



#   @build/blueprint/microfactory/microfactory.bash  
# Set of utility functions to build and run go code with microfactory
#
# Inputs:
#  ${GOROOT}
#  ${BUILDDIR}
#  ${BLUEPRINTDIR}
#  ${SRCDIR}
 
# Bootstrap microfactory from source if necessary and use it to build the
# requested binary.
#
# Arguments:
#  $1: name of the requested binary
#  $2: package name
#  ${EXTRA_ARGS}: extra arguments to pass to microfactory (-pkg-path, etc)

#   执行命令为 build_go soong_ui android/soong/cmd/soong_ui
function build_go
{
    # Increment when microfactory changes enough that it cannot rebuild itself.
    # For example, if we use a new command line argument that doesn't work on older versions.
    local mf_version=3
 
    local mf_src="${BLUEPRINTDIR}/microfactory"     #   @build/blueprint/microfactory
    local mf_bin="${BUILDDIR}/microfactory_$(uname)"    #   @out/microfactory_Linux
    local mf_version_file="${BUILDDIR}/.microfactory_$(uname)_version" # @out/.microfactory_Linux_version
    local built_bin="${BUILDDIR}/$1"            #   @out/soong_ui
    local from_src=1
    
     #   @out/microfactory_Linux        # @out/.microfactory_Linux_version    
    if [ -f "${mf_bin}" ] && [ -f "${mf_version_file}" ]; then
    ┊   if [ "${mf_version}" -eq "$(cat "${mf_version_file}")" ]; then  #判断out/.microfactory_Linux_version 文件存储值 是否为3
    ┊   ┊   from_src=0
    ┊   fi  
    fi  
 
    local mf_cmd
    if [ $from_src -eq 1 ]; then  #如果为1，则执行
    ┊   # `go run` requires a single main package, so create one
        #  @ out/.microfactory_Linux_intermediates/src
    ┊   local gen_src_dir="${BUILDDIR}/.microfactory_$(uname)_intermediates/src" 
    ┊   mkdir -p "${gen_src_dir}"   #创建目录   out/.microfactory_Linux_intermediates/src

        #  这里命令是将build/blueprint/microfactory/microfactory.go文件中的package microfactory 更改为 package main，
        #   然后写入out/.microfactory_Linux_intermediates/src/microfactory.go中
        #  sed "s/^package microfactory/package main/"  "build/blueprint/microfactory/microfactory.go" > out/.microfactory_Linux_intermediates/src/microfactory.go
    ┊   sed "s/^package microfactory/package main/" "${mf_src}/microfactory.go" >"${gen_src_dir}/microfactory.go"

        # prebuilts/go/linux-x86/bin/go run out/.microfactory_Linux_intermediates/src/microfactory.go
    ┊   mf_cmd="${GOROOT}/bin/go run ${gen_src_dir}/microfactory.go"
    else
    ┊   mf_cmd="${mf_bin}"
    fi  
 
    rm -f "${BUILDDIR}/.$1.trace"
    # GOROOT must be absolute because `go run` changes the local directory
    GOROOT=$(cd $GOROOT; pwd) ${mf_cmd} -b "${mf_bin}" \
    ┊   ┊   -pkg-path "github.com/google/blueprint=${BLUEPRINTDIR}" \
    ┊   ┊   -trimpath "${SRCDIR}" \
    ┊   ┊   ${EXTRA_ARGS} \
    ┊   ┊   -o "${built_bin}" $2
 
    if [ $? -eq 0 ] && [ $from_src -eq 1 ]; then
    ┊   echo "${mf_version}" >"${mf_version_file}"
    fi  
} 
