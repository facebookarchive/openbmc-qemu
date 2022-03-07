// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/net_i2c.h"
#include "migration/vmstate.h"

#define DEBUG_NET_I2C 1

#ifdef DEBUG_NET_I2C
#define DPRINTF(fmt, ...) \
do { printf("net_i2c(%02x): " fmt , dev->i2c.address, ## __VA_ARGS__); } while (0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "net_i2c: error: " fmt , ## __VA_ARGS__); exit(1);} while (0)
#else
#define DPRINTF(fmt, ...) do {} while(0)
#define BADF(fmt, ...) \
do { fprintf(stderr, "net_i2c: error: " fmt , ## __VA_ARGS__);} while (0)
#endif

enum {
    NET_I2C_IDLE,
    NET_I2C_WRITE_DATA,
    NET_I2C_READ_DATA,
    NET_I2C_DONE,
    NET_I2C_CONFUSED = -1
};

static int net_i2c_event(I2CSlave *s, enum i2c_event event)
{
    NetI2C *dev = NET_I2C(s);

    switch (event) {
    case I2C_START_SEND:
        switch (dev->mode) {
        case NET_I2C_IDLE:
            DPRINTF("Incoming data\n");
            dev->mode = NET_I2C_WRITE_DATA;
            break;

        default:
            BADF("Unexpected send start condition in state %d\n", dev->mode);
            dev->mode = NET_I2C_CONFUSED;
            break;
        }
        break;

    case I2C_START_RECV:
        switch (dev->mode) {
        case NET_I2C_IDLE:
            DPRINTF("Read mode\n");
            dev->mode = NET_I2C_READ_DATA;
            break;

        case NET_I2C_WRITE_DATA:
            DPRINTF("Read mode\n");
            dev->mode = NET_I2C_READ_DATA;
            break;

        default:
            BADF("Unexpected recv start condition in state %d\n", dev->mode);
            dev->mode = NET_I2C_CONFUSED;
            break;
        }
        break;

    case I2C_FINISH:
        switch (dev->mode) {
        case NET_I2C_READ_DATA:
            BADF("Unexpected stop during receive\n");
            break;

        default:
            // Nothing to do.
            break;
        }
        dev->mode = NET_I2C_IDLE;
        break;

    case I2C_NACK:
        switch (dev->mode) {
        case NET_I2C_DONE:
            // Nothing to do.
            break;

        case NET_I2C_READ_DATA:
            dev->mode = NET_I2C_DONE;
            break;

        default:
            BADF("Unexpected NACK in state %d\n", dev->mode);
            dev->mode = NET_I2C_CONFUSED;
            break;
        }
    }

    return 0;
}

static uint8_t net_i2c_recv(I2CSlave *s)
{
    NetI2C *dev = NET_I2C(s);
    NetClientState *nc = qemu_get_queue(dev->nic);
    uint8_t ret = 0xff;
    ssize_t size;

    switch (dev->mode) {
    case NET_I2C_READ_DATA:
        size = qemu_receive_packet(nc, &ret, 1);
        assert(size == 1);
        DPRINTF("Read data %02x\n", ret);
        break;

    default:
        BADF("Unexpected read in state %d\n", dev->mode);
        dev->mode = NET_I2C_CONFUSED;
        break;
    }

    return ret;
}

static int net_i2c_send(I2CSlave *s, uint8_t data)
{
    NetI2C *dev = NET_I2C(s);
    NetClientState *nc = qemu_get_queue(dev->nic);
    ssize_t size;

    switch (dev->mode) {
    case NET_I2C_WRITE_DATA:
        DPRINTF("Write data %02x\n", data);
        size = qemu_send_packet(nc, &data, 1);
        assert(size == 1);
        break;

    default:
        BADF("Unexpected write in state %d\n", dev->mode);
        break;
    }

    return 0;
}

static NetClientInfo net_i2c_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
};

static Property net_i2c_properties[] = {
    DEFINE_NIC_PROPERTIES(NetI2C, conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void net_i2c_realize(DeviceState *dev, Error **errp)
{
    NetI2C *s = NET_I2C(dev);
    NetClientState *nc = qemu_find_netdev("net_i2c");
    if (nc == NULL) {
        printf("Please specify -netdev xx,id=net_i2c to enable net_i2c.\n");
    } else {
        // Find unused entry and fill it.
        for(uint32_t nic = 0; nic < MAX_NICS; nic++) {
            NICInfo *entry = nd_table + nic;
            if (!entry->used) {
                entry->netdev = nc;
                entry->used = true;
                qdev_set_nic_properties(dev, entry);
                break;
            }
        }
    }

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->nic = qemu_new_nic(&net_i2c_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static void net_i2c_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_props(dc, net_i2c_properties);
    dc->realize = net_i2c_realize;

    I2CSlaveClass *sc = I2C_SLAVE_CLASS(klass);
    sc->event = net_i2c_event;
    sc->recv = net_i2c_recv;
    sc->send = net_i2c_send;
}

const VMStateDescription vmstate_net_i2c = {
    .name = TYPE_NET_I2C,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static const TypeInfo net_i2c_type_info = {
    .name = TYPE_NET_I2C,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(NetI2C),
    .class_init = net_i2c_class_init,
};

static void net_i2c_register_types(void)
{
    type_register_static(&net_i2c_type_info);
}

type_init(net_i2c_register_types)
