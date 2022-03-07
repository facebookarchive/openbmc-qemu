// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#ifndef NET_I2C_H
#define NET_I2C_H

#include "hw/i2c/i2c.h"
#include "net/net.h"
#include "qom/object.h"

#define TYPE_NET_I2C "net.i2c"
typedef struct NetI2C NetI2C;
DECLARE_INSTANCE_CHECKER(NetI2C, NET_I2C, TYPE_NET_I2C)

struct NetI2C {
    I2CSlave i2c;

    int32_t mode;

    NICState *nic;
    NICConf conf;
};

#endif
