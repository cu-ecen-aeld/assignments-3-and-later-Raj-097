#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

if [ -d "/home/raj_u22/assignment-1-Raj-097/tmp/aeld/" ]; then
	echo "Local machine"
	OUTDIR=/home/raj_u22/assignment-1-Raj-097/tmp/aeld
else
	echo "Github runner"
	OUTDIR=/__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/tmp/aeld
fi

KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"

if [ -d "/__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/arm-gnu-toolchain" ]; then
    echo "Directory exists in Github runner environment"
else
    echo "Directory does not exist in github runner environment or in local machine"
    exit 1
fi



if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
else 
	echo "Already cloned git linux stable, skipping the step"
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
	cd linux-stable
	echo "Checking out version ${KERNEL_VERSION}"
	git checkout ${KERNEL_VERSION}

   # TODO: Add your kernel build steps here
    # Deep clean
	make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    # Default kernel configuration
	make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    # Build the Kernel Image
	make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    # Build the Device Tree File
	make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs    
else
	echo "Already built Kernel image , skipping the step"

fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]; then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
	sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
	git clone git://busybox.net/busybox.git
	cd busybox
	git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
	make distclean
	make defconfig
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

else
	cd busybox
	echo "Already cloned Busybox git, skipping the step" 
fi

# TODO: Make and install busybox
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

if [ -d "/home/raj_u22/assignment-1-Raj-097/arm-gnu-toolchain/" ]; then
	echo "Local Machine"
	cp /home/raj_u22/assignment-1-Raj-097/arm-gnu-toolchain/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
	cp /home/raj_u22/assignment-1-Raj-097/arm-gnu-toolchain/libm.so.6 /home/raj_u22/assignment-1-Raj-097/arm-gnu-toolchain/libresolv.so.2 /home/raj_u22/assignment-1-Raj-097/arm-gnu-toolchain/libc.so.6 ${OUTDIR}/rootfs/lib64/
  
else
	echo "GitHub runner"
	cp /__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/arm-gnu-toolchain/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
	cp /__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/arm-gnu-toolchain/libm.so.6 /tmp/arm-gnu-toolchain/libresolv.so.2 /__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/arm-gnu-toolchain/libc.so.6 ${OUTDIR}/rootfs/lib64/
 
fi


# TODO: Make device nodes
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/tty c 5 1 

# TODO: Clean and build the writer utility
if [ -d "/home/raj_u22/assignment-1-Raj-097/finder-app" ]; then
	echo "Local Machine"
	cd /home/raj_u22/assignment-1-Raj-097/finder-app
	make clean
	make CROSS_COMPILE=${CROSS_COMPILE} all
	
else
	echo "GitHub Runner"
	cd /__w/assignments-3-and-later-Raj-097/assignments-3-and-later-Raj-097/finder-app
	make clean
	make CROSS_COMPILE=${CROSS_COMPILE} all
fi


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer finder.sh finder-test.sh conf/username.txt conf/assignment.txt autorun-qemu.sh ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio


# TODO: Create initramfs.cpio.gz
gzip -f ${OUTDIR}/initramfs.cpio
