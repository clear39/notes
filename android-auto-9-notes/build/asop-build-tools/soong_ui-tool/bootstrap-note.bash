#!/bin/bash

# This script serves two purposes.  First, it can bootstrap the standalone
# Blueprint to generate the minibp binary.  To do this simply run the script
# with no arguments from the desired build directory.
#
# It can also be invoked from another script to bootstrap a custom Blueprint-
# based build system.  To do this, the invoking script must first set some or
# all of the following environment variables, which are documented below where
# their default values are set:
#
#   BOOTSTRAP
#   WRAPPER
#   SRCDIR
#   BLUEPRINTDIR
#   BUILDDIR
#   NINJA_BUILDDIR
#   GOROOT
#
# The invoking script should then run this script, passing along all of its
# command line arguments.

set -e  #这句语句告诉bash如果任何语句的执行结果不是true则应该退出

EXTRA_ARGS=""

# BOOTSTRAP should be set to the path of the bootstrap script.  It can be
# either an absolute path or one relative to the build directory (which of
# these is used should probably match what's used for SRCDIR).

# "./build/blueprint/bootstrap.bash"
if [ -z "$BOOTSTRAP" ]; then # -z 字符串值长度为0 （the length of STRING is zero）这里不为 0
    BOOTSTRAP="${BASH_SOURCE[0]}"

    # WRAPPER should only be set if you want a ninja wrapper script to be
    # installed into the builddir. It is set to blueprint's blueprint.bash
    # only if BOOTSTRAP and WRAPPER are unset.
    [ -z "$WRAPPER" ] && WRAPPER="`dirname "${BOOTSTRAP}"`/blueprint.bash"
fi

# SRCDIR should be set to the path of the root source directory.  It can be
# either an absolute path or a path relative to the build directory.  Whether
# its an absolute or relative path determines whether the build directory can
# be moved relative to or along with the source directory without re-running
# the bootstrap script.
# "."
[ -z "$SRCDIR" ] && SRCDIR=`dirname "${BOOTSTRAP}"`

# BLUEPRINTDIR should be set to the path to the blueprint source. It generally
# should start with SRCDIR.
#"./build/blueprint"
[ -z "$BLUEPRINTDIR" ] && BLUEPRINTDIR="${SRCDIR}"

# BUILDDIR should be set to the path to store build results. By default, this
# is the current directory, but it may be set to an absolute or relative path.
#  "out/soong" 
[ -z "$BUILDDIR" ] && BUILDDIR=.

# NINJA_BUILDDIR should be set to the path to store the .ninja_log/.ninja_deps
# files. By default this is the same as $BUILDDIR.

# "out"
[ -z "$NINJA_BUILDDIR" ] && NINJA_BUILDDIR="${BUILDDIR}"

# TOPNAME should be set to the name of the top-level Blueprints file

# "Android.bp"
[ -z "$TOPNAME" ] && TOPNAME="Blueprints"

# These variables should be set by auto-detecting or knowing a priori the host
# Go toolchain properties.

# "./prebuilts/go/linux-x86"
[ -z "$GOROOT" ] && GOROOT=`go env GOROOT`

usage() {
    echo "Usage of ${BOOTSTRAP}:"
    echo "  -h: print a help message and exit"
    echo "  -b <builddir>: set the build directory"
    echo "  -t: run tests"
}

# Parse the command line flags.
while getopts ":b:ht" opt; do
    case $opt in
        b) BUILDDIR="$OPTARG";;
        t) RUN_TESTS=true;;
        h)
            usage
            exit 1
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            usage
            exit 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            exit 1
            ;;
    esac
done

# If RUN_TESTS is set, behave like -t was passed in as an option.
[ ! -z "$RUN_TESTS" ] && EXTRA_ARGS="${EXTRA_ARGS} -t"

# Allow the caller to pass in a list of module files
# "out/.module_paths/Android.bp.list"
if [ -z "${BLUEPRINT_LIST_FILE}" ]; then
  BLUEPRINT_LIST_FILE="${BUILDDIR}/.bootstrap/bplist"
fi

EXTRA_ARGS="${EXTRA_ARGS} -l ${BLUEPRINT_LIST_FILE}"  # "-t -l out/.module_paths/Android.bp.list"

mkdir -p $BUILDDIR/.minibootstrap    # 创建 "out/soong/.minibootstrap" 目录


# out/soong/.minibootstrap/build.ninja 文件内容如下：
# bootstrapBuildDir = out/soong
# topFile = ./Android.bp
# extraArgs =  -t -l out/.module_paths/Android.bp.list
# builddir = out
# include ./build/blueprint/bootstrap/build.ninja

echo "bootstrapBuildDir = $BUILDDIR" > $BUILDDIR/.minibootstrap/build.ninja
echo "topFile = $SRCDIR/$TOPNAME" >> $BUILDDIR/.minibootstrap/build.ninja
echo "extraArgs = $EXTRA_ARGS" >> $BUILDDIR/.minibootstrap/build.ninja
echo "builddir = $NINJA_BUILDDIR" >> $BUILDDIR/.minibootstrap/build.ninja
echo "include $BLUEPRINTDIR/bootstrap/build.ninja" >> $BUILDDIR/.minibootstrap/build.ninja



# out/soong/.blueprint.bootstrap 文件内容如下：
# BLUEPRINT_BOOTSTRAP_VERSION=2
# SRCDIR="."
# BLUEPRINTDIR="./build/blueprint"
# NINJA_BUILDDIR="out"
# GOROOT="./prebuilts/go/linux-x86"
# TOPNAME="Android.bp"

echo "BLUEPRINT_BOOTSTRAP_VERSION=2" > $BUILDDIR/.blueprint.bootstrap
echo "SRCDIR=\"${SRCDIR}\"" >> $BUILDDIR/.blueprint.bootstrap
echo "BLUEPRINTDIR=\"${BLUEPRINTDIR}\"" >> $BUILDDIR/.blueprint.bootstrap
echo "NINJA_BUILDDIR=\"${NINJA_BUILDDIR}\"" >> $BUILDDIR/.blueprint.bootstrap
echo "GOROOT=\"${GOROOT}\"" >> $BUILDDIR/.blueprint.bootstrap
echo "TOPNAME=\"${TOPNAME}\"" >> $BUILDDIR/.blueprint.bootstrap

echo "=====$WRAPPER======" >> $BUILDDIR/.blueprint.bootstrap

touch "${BUILDDIR}/.out-dir"  # 创建  ”out/soong/.out-dir“ 文件

# "$WRAPPER" 长度为 0
if [ ! -z "$WRAPPER" ]; then
    cp $WRAPPER $BUILDDIR/
fi
