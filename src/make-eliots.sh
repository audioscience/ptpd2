
# Location of target linux headers
ARM_KBUILD_OUTPUT=$HOME/asi/external/linux-omap3
# Root of NFS filesystem for network boot
ARM_NFS_ROOT=$HOME/asi/firmware/ezsdk_target_fs_5_03_01_15
# ARM c compiler
ARM_CC=arm-none-linux-gnueabi-gcc

make KBUILD_OUTPUT=$ARM_KBUILD_OUTPUT CC=$ARM_CC $@
cp ptpd2 $ARM_NFS_ROOT/home/root/
