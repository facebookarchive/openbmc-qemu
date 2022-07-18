# Facebook OpenBMC QEMU README

## Building QEMU on CentOS 8

```bash
dnf install -y ninja-build glib2-devel pixman-devel
git clone https://github.com/facebook/openbmc-qemu
cd openbmc-qemu
./configure --target-list=arm-softmmu
make -j $(nproc)
```

## List Facebook machines supported in QEMU

```bash
./build/qemu-system-arm -machine help | grep Facebook
```

## Get a Facebook OpenBMC image

Build an image from https://github.com/facebook/openbmc, or get one from the [Github Releases](https://github.com/facebook/openbmc/releases).

```bash
wget https://github.com/facebook/openbmc/releases/download/openbmc-e2294ff5d31d/fby35.mtd
```

## Boot a Facebook OpenBMC image

```bash
./build/qemu-system-arm -machine fby35-bmc -drive file=fby35.mtd,format=raw,if=mtd -nographic
```

Username `root`, password `0penBmc`.

## Boot a Facebook OpenBMC kernel directly (Much faster!)

Note: If you don't get any console output, experiment with different console
values in the `-append` argument. Some of the `.dts`'s are wrong and rely on a
different value from U-Boot.

```bash
# Get the kernel FIT image from the build directory, usually fit-<machine>.itb
dumpimage -i fit-fby35.itb -T flat_dt -p 0 kernel
dumpimage -i fit-fby35.itb -T flat_dt -p 1 ramdisk
dumpimage -i fit-fby35.itb -T flat_dt -p 2 dtb
./build/qemu-system-arm -machine fby35-bmc -kernel kernel -ramdisk ramdisk -dtb dtb -append "console=ttyS0 root=/dev/ram rw" -nographic
```

## Get a Facebook OpenBIC image

Build an image from https://github.com/facebook/openbic, or get one from [my fork](https://github.com/peterdelevoryas/OpenBIC/releases).

```bash
wget https://github.com/peterdelevoryas/OpenBIC/releases/download/oby35-cl-2022.13.01/Y35BCL.elf
```

## Boot a Facebook OpenBIC image

```bash
./build/qemu-system-arm -machine oby35-cl -kernel Y35BCL.elf -nographic
```

## Boot a Nuvoton NPCM845R OpenBMC image

```bash
wget https://github.com/peterdelevoryas/openbmc/releases/download/nv1/npcm845.mtd

./configure --target-list=aarch64-softmmu && make
./build/qemu-system-aarch64 -machine npcm845-evb -nographic -drive file=npcm845.mtd,if=mtd,format=raw,snapshot=on
```

## Using a custom Nuvoton NPCM845R TIP firmware

```bash
git clone https://github.com/peterdelevoryas/vbootrom
cd vbootrom/npcm8xx

# on macOS
brew install aarch64-elf-binutils aarch64-elf-gcc
make CROSS_COMPILE=aarch64-elf-

# on Fedora
sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
make

qemu-system-aarch64 -bios npcm8xx_bootrom.bin ...
```

## Port-forwarding ssh to port 2222

```bash
./build/qemu-system-arm -machine fby35-bmc -drive file=fby35.mtd,format=raw,if=mtd -nographic \
    -netdev user,id=nic,mfr-id=0x8119,oob-eth-addr=de:ad:be:ef:ca:fe,hostfwd=::2222-:22 \
    -net nic,model=ftgmac100,netdev=nic
sshpass -p 0penBmc ssh root@localhost -p 2222
```

## Forward all network traffic to a TAP interface

First, setup an ethernet bridge and a TAP interface:

```bash
sudo ip link add dev bmc-br0 type bridge
sudo ip link set dev bmc-br0 up
sudo ip tuntap add tap0 mode tap
sudo ip link set tap0 up
sudo brctl addif bmc-br0 tap0
./build/qemu-system-arm -machine fby35-bmc -drive file=fby35.mtd,format=raw,if=mtd -nographic \
    -netdev tap,id=tap0,script=no,ifname=tap0 \
    -net nic,model=ftgmac100,netdev=tap0
# ... after boot, find link-local IPv6 address in the guest OS
ip address show dev eth0 scope link
# 2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
#     link/ether 8e:0c:2d:76:b8:6b brd ff:ff:ff:ff:ff:ff
#    inet6 fe80::8c0c:2dff:fe76:b86b/64 scope link
#        valid_lft forever preferred_lft forever

# Then you can ping or ssh to from the host to the guest
ping6 fe80::8c0c:2dff:fe76:b86b%bmc-br0
sshpass -p 0penBmc ssh root@fe80::8c0c:2dff:fe76:b86b%bmc-br0
```

## MACVTAP Configuration

```bash
export MACVTAP0_ADDR="1a:46:0b:ca:bc:7b"
sudo ip link add link eth0 name macvtap0 type macvtap
sudo ip link set macvtap0 address $MACVTAP0_ADDR up

# For some reason, I get permission errors opening /dev/tap3 if I don't
# switch to root.
su -
export MACVTAP0_IFINDEX="$(cat /sys/class/net/macvtap0/ifindex)"
export MACVTAP0_DEV="/dev/tap$MACVTAP0_IFINDEX"
./build/qemu-system-arm -machine yosemitev2-bmc \
  -drive file=fby2.mtd,format=raw,if=mtd \
  -drive file=fby2.mtd,format=raw,if=mtd \
  -serial stdio -display none \
  -netdev tap,fd=3,id=macvtap0 \
  -net nic,netdev=macvtap0,macaddr=$MACVTAP0_ADDR,model=ftgmac100 \
  3<>$MACVTAP0_DEV
```

## Debugging OpenBMC with GDB

```bash
# Run QEMU with "-s -S". It will wait for you to connect with GDB at
# "localhost:1234". (Do this in a separate terminal window or TMUX pane)
qemu-system-arm -machine yosemitev2-bmc \
  -drive file=fby2.mtd,format=raw,if=mtd \
  -drive file=fby2.mtd,format=raw,if=mtd \
  -nographic \
  -nic user,hostfwd=::2222-:22 \
  -s -S

# This bitbake recipe builds gdb with arm instruction support, which
# is not enabled by default in most x86 gdb distributions. Sometimes
# the recipe doesn't work though, so use cleanall to ensure it works.
bitbake gdb-cross-arm -c do_cleanall && bitbake gdb-cross-arm
GDB=$(find . -name \*gdb -path \*image/data\* -executable -type f)

# To debug the U-Boot SPL (usually the first code that runs).
# Replace "default" with "recovery" if necessary.
SPL=$(find . -name u-boot-spl -executable -type f -path \*default\*)
$GDB $SPL -ex "target remote localhost:1234"

# To debug U-Boot (post-SPL), start by setting a breakpoint in the
# SPL, then use "add-symbol-file" to add the U-Boot ELF at the
# appropriate location in memory. For example:
find . -name u-boot -executable -type f -path \*default\*
# ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot
# Replace "default" with "recovery" if booting a signed or locked image.
$GDB $SPL -ex "target remote localhost:1234"
# GNU gdb (GDB) 8.0
# Copyright (C) 2017 Free Software Foundation, Inc.
# License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
# This is free software: you are free to change and redistribute it.
# There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
# and "show warranty" for details.
# This GDB was configured as "--host=x86_64-linux --target=arm-fb-linux-gnueabi".
# Type "show configuration" for configuration details.
# For bug reporting instructions, please see:
# <http://www.gnu.org/software/gdb/bugs/>.
# Find the GDB manual and other documentation resources online at:
# <http://www.gnu.org/software/gdb/documentation/>.
# For help, type "help".
# Type "apropos word" to search for commands related to "word"...
# Reading symbols from ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/spl/u-boot-spl...done.
# Remote debugging using localhost:1234
# ast_ahbc_boot_remap () at ../arch/arm/mach-aspeed/ast-ahbc.c:79
# 79      {
# (gdb) b vboot_jump
# Breakpoint 1 at 0xf6c: file ../board/aspeed/ast-g5/ast-g5-spl.c, line 121.
# (gdb) c
# Continuing.
#
# Breakpoint 1, vboot_jump (to=to@entry=0x28084000, vbs=vbs@entry=0x1e7213b8) at ../board/aspeed/ast-g5/ast-g5-spl.c:121
# 121     {
# (gdb) p/x to
# $1 = 0x28084000
# (gdb) add-symbol-file ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot $1
# add symbol table from file "./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot" at
#         .text_addr = 0x28084000
# (y or n) y
# Reading symbols from ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot...done.
# (gdb) b board_init_f
# Breakpoint 2 at 0x0: board_init_f. (3 locations)
# (gdb) c
# Continuing.
#
# Breakpoint 2, board_init_f (boot_flags=0) at ../common/board_f.c:1057
# 1057            gd->have_console = 0;
# (gdb) p/x $pc
# $2 = 0x2808f2a8
# (gdb)
#
# To automate this process, you can use a gdb script:
cat goto_uboot_board_init_f
# target remote localhost:1234
# b vboot_jump
# c
# p/x to
# add-symbol-file ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot $1
# b board_init_f
# c
$GDB $SPL -ex "source goto_uboot_board_init_f"

# There's actually another relocation step within U-Boot proper, when
# U-Boot relocates from SRAM to DRAM. This requires another "add-symbol-file" call:
cat goto_uboot_board_init_r
# source goto_uboot_board_init_f
# b relocate_done
# c
# p/x ((gd_t*)$r9)->relocaddr
# add-symbol-file ./tmp/work/armv6-fb-linux-gnueabi/u-boot/v2016.07-r0/u-boot-v2016.07/default/u-boot $2
# b board_init_r
# c
$GDB $SPL -ex "source goto_uboot_board_init_r"
# This will go all the way to post-relocation U-Boot proper.

# To debug the kernel, you need to build with some debug options enabled,
# and you need to allow-list the vmlinux-gdb.py script in your ~/.gdbinit script.
git diff
# diff --git a/meta-facebook/meta-fby2/meta-fby2-kernel/recipes-kernel/linux/files/defconfig b/meta-facebook/meta-fby2/meta-fby2-kernel/recipes-kernel/linux/files/defconfig
# index 8fc53f3d54..fb961b6f70 100644
# --- a/meta-facebook/meta-fby2/meta-fby2-kernel/recipes-kernel/linux/files/defconfig
# +++ b/meta-facebook/meta-fby2/meta-fby2-kernel/recipes-kernel/linux/files/defconfig
# @@ -3450,3 +3450,10 @@ CONFIG_DEBUG_LL_INCLUDE="mach/debug-macro.S"
#  CONFIG_UNCOMPRESS_INCLUDE="debug/uncompress.h"
#  # CONFIG_PID_IN_CONTEXTIDR is not set
#  # CONFIG_CORESIGHT is not set
# +
# +CONFIG_DEBUG_INFO=y
# +CONFIG_DEBUG_INFO_DWARF4=y
# +CONFIG_GDB_SCRIPTS=y
# +CONFIG_FRAME_POINTER=y
# +CONFIG_DEBUG_KERNEL=y
cat ~/.gdbinit
# add-auto-load-safe-path /
VMLINUX=$(find . -name vmlinux -executable -type f | tail -n 1)
$GDB $VMLINUX -ex "target remote localhost:1234"
# You can basically break on anything in a kernel driver, no relocation stuff to deal with.
# Or, you can ctrl-C to interrupt, to debug a hang.
# (gdb) b ast_adc_probe
# (gdb) c
```

## Debugging OpenBIC with gdb

Debugging OpenBIC with gdb is much easier:

```
./build/qemu-system-arm -machine oby35-cl -kernel Y35BCL.elf -nographic -s -S
gdb Y35BCL.elf -ex "target remote localhost:1234"
```

## Using the QEMU Monitor

Enter ctrl-a; c to bring up the QOM prompt while using `-nographic`:

```bash
QEMU 7.0.50 monitor - type 'help' for more information
(qemu) help
```

In particular, you can use `qom-list`, `qom-get`, and `qom-set` to query the QOM tree.
