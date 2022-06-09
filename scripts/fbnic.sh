#!/usr/bin/env bash

FBNIC_QEMU="$1"
FBNIC_ELF="$2"

BMC_QEMU_URL="https://github.com/facebook/openbmc-qemu/releases/download/0.3.0/qemu-system-arm.centos8"
BMC_IMAGE_URL="https://github.com/facebook/openbmc/releases/download/openbmc-e2294ff5d31d/fby35.mtd"

$FBNIC_QEMU -machine durga -nographic -bios none \
    -kernel $FBNIC_ELF \
    -d guest_errors -nic none \
    -netdev socket,id=durga_rbt,listen=localhost:5555 \
    -netdev socket,id=durga_i2c,listen=localhost:5556 &

wget $BMC_IMAGE_URL
wget $BMC_QEMU_URL
chmod +x ./qemu-system-arm.centos8
./qemu-system-arm.centos8 -machine fby35-bmc \
    -drive file=fby35.mtd,format=raw,if=mtd \
    -nographic \
    -netdev socket,id=nic,connect=localhost:5555 \
    -netdev socket,id=i2c,connect=localhost:5556 \
    -net nic,model=ftgmac100,netdev=nic \
    -device i2c-netdev2,bus=aspeed.i2c.bus.0,address=0x32,netdev=i2c
