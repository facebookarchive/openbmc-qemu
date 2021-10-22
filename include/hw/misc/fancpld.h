#ifndef QEMU_FANCPLD_H
#define QEMU_FANCPLD_H

#include "hw/i2c/i2c.h"
#include "qom/object.h"

#define TYPE_FANCPLD "fancpld"
OBJECT_DECLARE_SIMPLE_TYPE(FanCpldState, FANCPLD)

struct FanCpldState {
    I2CSlave i2c;
};

#endif
