#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "hw/i2c/i2c.h"
#include "hw/qdev-properties.h"
#include "net/net.h"
#include "net/eth.h"
#include "block/aio.h"

#define TYPE_I2C_NETDEV "i2c-netdev"
OBJECT_DECLARE_SIMPLE_TYPE(I2CNetdev, I2C_NETDEV)

typedef enum AsyncState AsyncState;
enum AsyncState {
    ASYNC_NONE,
    ASYNC_WAITING,
    ASYNC_DONE,
};

typedef struct I2CNetdev I2CNetdev;
struct I2CNetdev {
    I2CSlave parent;

    I2CBus *bus;
    NICConf nic_conf;
    NICState *nic;
    QEMUBH *bh;
    AsyncState bh_bus_master;
    AsyncState bh_transmit;

    uint8_t guest_os_tx_buf[256];
    uint8_t guest_os_rx_buf[256];
    int guest_os_tx_pos;
    int guest_os_rx_pos;
    int guest_os_rx_len;
};

static int i2c_event(I2CSlave *i2c, enum i2c_event event)
{
    I2CNetdev *s = I2C_NETDEV(i2c);
    NetClientState *netdev;

    switch (event) {
    case I2C_START_RECV:
        printf("SLAVE RECEIVE UNIMPLEMENTED\n");
        abort();
    case I2C_START_SEND:
        s->guest_os_tx_pos = 0;
        // Slave address, with R/W bit, is expected to be the first byte.
        // R/W bit 1 indicates "read", 0 indicates "write".
        s->guest_os_tx_buf[s->guest_os_tx_pos++] = i2c->address << 1 | 0;
        break;
    case I2C_FINISH:
        assert(s->guest_os_tx_pos != 0);
        netdev = qemu_get_queue(s->nic);
        qemu_send_packet(netdev, s->guest_os_tx_buf, s->guest_os_tx_pos);
        break;
    case I2C_NACK:
        printf("RECEIVE NACK UNIMPLEMENTED\n");
        abort();
    }

    return 0;
}

static uint8_t i2c_receive(I2CSlave *i2c)
{
    printf("I2C NETDEV SLAVE RECEIVE UNIMPLEMENTED\n");
    abort();
}

static int i2c_transmit(I2CSlave *i2c, uint8_t byte)
{
    I2CNetdev *s = I2C_NETDEV(i2c);

    assert(s->guest_os_tx_pos < sizeof(s->guest_os_tx_buf));
    s->guest_os_tx_buf[s->guest_os_tx_pos++] = byte;

    return 0;
}

static bool i2c_netdev_can_receive_packet(NetClientState *nc)
{
    I2CNetdev *s = I2C_NETDEV(qemu_get_nic_opaque(nc));

    // If the rx buffer is empty, then we can accept a new packet and schedule a
    // new receive.
    return s->guest_os_rx_len == 0;
}

static ssize_t i2c_netdev_receive_packet(NetClientState *nc, const uint8_t *buf, size_t len)
{
    I2CNetdev *s = I2C_NETDEV(qemu_get_nic_opaque(nc));

    s->guest_os_rx_len = len;
    s->guest_os_rx_pos = 0;
    assert(len < sizeof(s->guest_os_rx_buf));
    memcpy(s->guest_os_rx_buf, buf, len);
    qemu_bh_schedule(s->bh);

    return len;
}

static void i2c_netdev_cleanup(NetClientState *nc)
{
    I2CNetdev *s = I2C_NETDEV(qemu_get_nic_opaque(nc));

    s->nic = NULL;
}

static NetClientInfo net_client_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NetClientState),
    .can_receive = i2c_netdev_can_receive_packet,
    .receive = i2c_netdev_receive_packet,
    .cleanup = i2c_netdev_cleanup,
};

static void i2c_netdev_bh(void *opaque)
{
    I2CNetdev *s = opaque;

    switch (s->bh_bus_master) {
    case ASYNC_NONE:
        i2c_bus_master(s->bus, s->bh);
        s->bh_bus_master = ASYNC_WAITING;
        return;
    case ASYNC_WAITING:
        s->bh_bus_master = ASYNC_DONE;
        break;
    case ASYNC_DONE:
        break;
    }

    uint8_t b;
    uint8_t slave_addr;

    switch (s->bh_transmit) {
    case ASYNC_NONE:
    case ASYNC_DONE:
        assert(s->guest_os_rx_pos == 0);
        assert(s->guest_os_rx_len != 0);
        // Other side should be sending us the slave address shifted 1 to the
        // left, with a R/W bit. The R/W bit should always be 0, because we're
        // only supporting writes at this point.
        b = s->guest_os_rx_buf[s->guest_os_rx_pos++];
        assert((b & 1) == 0);
        slave_addr = b >> 1;
        i2c_start_send(s->bus, slave_addr);
        s->bh_transmit = ASYNC_WAITING;
        break;
    case ASYNC_WAITING:
        if (s->guest_os_rx_pos >= s->guest_os_rx_len) {
            s->bh_transmit = ASYNC_DONE;
            s->bh_bus_master = ASYNC_NONE;
            s->guest_os_rx_len = 0;
            s->guest_os_rx_pos = 0;
            i2c_end_transfer(s->bus);
            i2c_bus_release(s->bus);
            break;
        }

        b = s->guest_os_rx_buf[s->guest_os_rx_pos++];
        i2c_send_async(s->bus, b);
        break;
    }
}

static void i2c_netdev_realize(DeviceState *dev, Error **errp)
{
    I2CNetdev *s = I2C_NETDEV(dev);
    s->bus = I2C_BUS(qdev_get_parent_bus(dev));
    s->nic = qemu_new_nic(&net_client_info, &s->nic_conf, TYPE_I2C_NETDEV, dev->id, s);
    s->bh = qemu_bh_new(i2c_netdev_bh, s);
    s->bh_bus_master = ASYNC_NONE;
    s->bh_transmit = ASYNC_NONE;
    memset(s->guest_os_tx_buf, 0, sizeof(s->guest_os_tx_buf));
    memset(s->guest_os_rx_buf, 0, sizeof(s->guest_os_rx_buf));
    s->guest_os_tx_pos = 0;
    s->guest_os_rx_pos = 0;
    s->guest_os_rx_len = 0;
}

static Property i2c_netdev_props[] = {
    DEFINE_NIC_PROPERTIES(I2CNetdev, nic_conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void i2c_netdev_class_init(ObjectClass *cls, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cls);
    dc->realize = i2c_netdev_realize;
    device_class_set_props(dc, i2c_netdev_props);

    I2CSlaveClass *sc = I2C_SLAVE_CLASS(cls);
    sc->event = i2c_event;
    sc->recv = i2c_receive;
    sc->send = i2c_transmit;
}

static const TypeInfo i2c_netdev = {
    .name = TYPE_I2C_NETDEV,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(I2CNetdev),
    .class_init = i2c_netdev_class_init,
};

static void register_types(void)
{
    type_register_static(&i2c_netdev);
}

type_init(register_types);
