#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "hw/misc/fancpld.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/module.h"

static int fancpld_event(I2CSlave *i2c, enum i2c_event event)
{
    return 0;
}

static uint8_t fancpld_recv(I2CSlave *i2c)
{
    return 0xff;
}

static int fancpld_send(I2CSlave *i2c, uint8_t data)
{
    return 0;
}

static void fancpld_init(Object *obj)
{
}

static void fancpld_class_init(ObjectClass *klass, void *data)
{
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);

    k->event = fancpld_event;
    k->recv = fancpld_recv;
    k->send = fancpld_send;
}

static const TypeInfo fancpld_info = {
    .name = TYPE_FANCPLD,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(FanCpldState),
    .instance_init = fancpld_init,
    .class_init = fancpld_class_init,
};

static void fancpld_register_types(void)
{
    type_register_static(&fancpld_info);
}

type_init(fancpld_register_types);
