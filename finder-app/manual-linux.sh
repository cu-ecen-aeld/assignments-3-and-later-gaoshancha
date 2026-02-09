#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo - modified by Bryan Robinson for AELD course assignments

set -e
set -u

OUTDIR=/tmp/aeld
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

if ! mkdir -p "${OUTDIR}"; then
    echo "Cannot create output directory: ${OUTDIR}"
    exit 1
fi

echo "Changing into ${OUTDIR}..."
cd "${OUTDIR}"

if [ -d "linux-stable" ]; then
    echo "DEBUG: Directory exists, skipping git repository clone"
else
    echo "DEBUG: Directory does not exist, cloning git repo..."
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION} linux-stable
fi

echo "DEBUG: OUTDIR=${OUTDIR}"
echo "DEBUG: ARCH=${ARCH}"
echo "DEBUG: Full path: ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image"

#if [ -e linux-stable/arch/${ARCH}/boot/Image ]; then
#    echo "Kernel image is already built at is at: linux-stable/arch/${ARCH}/boot/Image"
#    ls -lh linux-stable/arch/${ARCH}/boot/Image*
#else
    cd linux-stable
    echo "No previously built boot image; checking out version ${KERNEL_VERSION} and building..."
    git checkout ${KERNEL_VERSION}

    # Build the kernel image
    make  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make  ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all

    if [ -f arch/arm64/boot/Image ] && [ -s arch/arm64/boot/Image ]; then
       echo "Build successful! Kernel image is at: ./arch/${ARCH}/boot/Image"
       ls -lh ./arch/${ARCH}/boot/Image*
    else
       echo "Build failed! Kernel image not found or is empty."
       exit 1
    fi
    cd ..
#fi

echo "Current working dir: `pwd`"
echo "Adding the Image to ${OUTDIR} ..."
cp ./linux-stable/arch/${ARCH}/boot/Image .
cp ./linux-stable/arch/${ARCH}/boot/Image.gz .

echo "${OUTDIR} contents:"
ls -alh .

if [ -d "rootfs" ]; then
    echo "Deleting the previous rootfs directory and starting over clean"
    rm  -rf ./rootfs
fi

echo "Creating the staging directory for the root filesystem:"
mkdir -p rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var}
mkdir -p rootfs/usr/{bin,lib,sbin}
mkdir -p rootfs/var/log

# works on my system but not on the automated grader docker image
#   --> Not needed, just nice debug output!
#tree -d ./rootfs

if [ -d "busybox" ]; then
    rm -rf ./busybox
fi

git clone git://busybox.net/busybox.git
cd busybox
git checkout ${BUSYBOX_VERSION}
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=../rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ..

echo "Current working dir: `pwd`"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "Shared library"


# Add library dependencies to rootfs build
#cp /toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 rootfs/lib/
#cp /toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6 rootfs/lib64/
#cp /toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 rootfs/lib64/
#cp /toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 rootfs/lib64/
# ... above works on my Ubuntu docker image ... but not in the autograder on my github action runner ... just add the files needed
# ... not ideal but this build script is very complicated
cp ${FINDER_APP_DIR}/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp ${FINDER_APP_DIR}/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp ${FINDER_APP_DIR}/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
cp ${FINDER_APP_DIR}/libc.so.6 ${OUTDIR}/rootfs/lib64/

# Make device nodes
mknod -m 0666 ${OUTDIR}/rootfs/dev/null c 1 3
mknod -m 0600 ${OUTDIR}/rootfs/dev/console c 1 5

# Clean and build the writer utility
cd ${FINDER_APP_DIR}
echo "Current working dir: `pwd`"
make CROSS_COMPILE=${CROSS_COMPILE} clean
make CROSS_COMPILE=${CROSS_COMPILE} all

# Copy the finder related scripts and executables to the /home directory on the target rootfs
echo "DIR: ${OUTDIR}/rootfs/home/"
cp writer ${OUTDIR}/rootfs/home/writer
cp finder.sh ${OUTDIR}/rootfs/home/finder.sh
cp finder-test.sh ${OUTDIR}/rootfs/home/finder-test.sh
mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf/username.txt
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf/assignment.txt
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/autorun-qemu.sh

# Chown the root directory
chown -R root:root ${OUTDIR}/rootfs

# Create initramfs.cpio.gz
echo "Build the initram FS ..."
echo "Current working dir: `pwd`"
cd ${OUTDIR}/rootfs
echo "Current working dir: `pwd`"
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip -f initramfs.cpio
