# !/bin/bash
# Standard Android Build Environment

sudo ln -s /work/bin/repo /usr/local/bin/repo

sudo ln -s /work/bin/uuu /usr/local/bin/uuu

sudo apt-get -y install linux-doc libcorelinux-doc

sudo apt-get -y install manpages-posix  manpages-posix-dev

sudo apt-get -y install libc-dev glibc-doc

sudo apt-get -y install libstdc++6-4.7-dev  libstdc++6-4.7-doc

sudo apt-get -y install stl-manual

sudo apt-get -y install tree

sudo apt-get -y install vim

sudo apt-get -y install subversion

sudo apt-get -y install meld

sudo apt-get -y install minicom

cp ~/.bashrc ~/.bashrc-bk
echo "ANDROID_SDK=/work/tools/android-sdk" >> ~/.bashrc
echo "export PATH=$$PATH:$$ANDROID_SDK/tools:$$ANDROID_SDK/platform-tools" >> ~/.bashrc 
echo "export PATH=$$PATH:/usr/local/go/bin" >> ~/.bashrc
source ~/.bashrc



apt-get update
apt-get install -y git-core gnupg flex bison gperf build-essential
apt-get install -y zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386c libc6-dev 
apt-get install -y lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccachec
apt-get install -y libgl1-mesa-dev libxml2-utils xsltproc unzip


# Freescale Android-4.4.3 Required Environment
apt-get install -y uuid uuid-dev liblz-dev liblzo2-2 liblzo2-dev lzop
add-apt-repository -y ppa:git-core/ppa
apt-get update
apt-get install -y --reinstall -y git git-core curl
apt-get install -y u-boot-tools


sudo apt-get -y install git-core gnupg flex bison gperf build-essential 
sudo apt-get -y install zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 
sudo apt-get -y install lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache 
sudo apt-get -y install libgl1-mesa-dev libxml2-utils xsltproc unzip



sudo apt-get -y install libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-dev g++-multilib
sudo apt-get -y install -y git flex bison gperf build-essential libncurses5-dev:i386
sudo apt-get -y install tofrodos python-markdown libxml2-utils xsltproc zlib1g-dev:i386
sudo apt-get -y install dpkg-dev libsdl1.2-dev libesd0-dev
sudo apt-get -y install git-core gnupg flex bison gperf build-essential  
sudo apt-get -y install zip curl zlib1g-dev gcc-multilib g++-multilib
sudo apt-get -y install libc6-dev-i386
sudo apt-get -y install lib32ncurses5-dev x11proto-core-dev libx11-dev
sudo apt-get -y install libgl1-mesa-dev libxml2-utils xsltproc unzip m4
sudo apt-get -y install lib32z-dev ccache

# kernel "make menuconfig" command Required Packages
apt-get install -y libncurses5 libncurses5-dev mingw32









