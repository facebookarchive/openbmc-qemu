#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qapi/error.h"
#include "hw/i2c/i2c.h"
#include "hw/qdev-properties.h"
#include "net/net.h"
#include "net/eth.h"
#include "block/aio.h"

#define START_LEN 3
#define STOP_LEN 2

#define TYPE_I2C_NETDEV2 "i2c-netdev2"
OBJECT_DECLARE_SIMPLE_TYPE(I2CNetdev2, I2C_NETDEV2)

typedef struct I2CNetdev2 I2CNetdev2;
struct I2CNetdev2 {
    I2CSlave parent;

    I2CBus *bus;
    NICConf nic_conf;
    NICState *nic;
    QEMUBH *bh;

    uint8_t rx_buf[3];
    int rx_len;
    bool rx_ack_pending;
};

static bool i2c_netdev2_nic_can_receive(NetClientState *nc)
{
    I2CNetdev2 *s = I2C_NETDEV2(qemu_get_nic_opaque(nc));

    return s->rx_len == 0;
}

// static void print_bytes(const uint8_t *buf, size_t len)
// {
//     int i;
// 
//     printf("[");
//     for (i = 0; i < len; i++) {
//         if (i) {
//             printf(", ");
//         }
//         printf("%02x", buf[i]);
//     }
//     printf("]");
// }

static ssize_t i2c_netdev2_nic_receive(NetClientState *nc, const uint8_t *buf, size_t len);

static void i2c_netdev2_nic_cleanup(NetClientState *nc)
{
    I2CNetdev2 *s = I2C_NETDEV2(qemu_get_nic_opaque(nc));

    s->nic = NULL;
}

static NetClientInfo net_client_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NetClientState),
    .can_receive = i2c_netdev2_nic_can_receive,
    .receive = i2c_netdev2_nic_receive,
    .cleanup = i2c_netdev2_nic_cleanup,
};

static ssize_t i2c_netdev2_nic_receive(NetClientState *nc, const uint8_t *buf, size_t len)
{
    // printf("%s: ", __func__);
    // print_bytes(buf, len);
    // printf("\n");

    I2CNetdev2 *s = I2C_NETDEV2(qemu_get_nic_opaque(nc));

    assert(len <= sizeof(s->rx_buf));
    memcpy(s->rx_buf, buf, len);
    s->rx_len = len;

    switch (len) {
    case START_LEN:
        i2c_bus_master(s->bus, s->bh);
        break;
    case 1:
        // This is probably an ack/nack packet.
        if (s->bus->bh != s->bh) {
            s->rx_len = 0;
            break;
        }
        qemu_bh_schedule(s->bh);
        break;
    case STOP_LEN:
        qemu_bh_schedule(s->bh);
        break;
    default:
        printf("%s: unexpected packet len: %ld\n", __func__, len);
        abort();
    }

    return len;
}

static void i2c_netdev2_slave_mode_rx(void *opaque)
{
    I2CNetdev2 *s = opaque;
    NetClientState *netdev = qemu_get_queue(s->nic);
    uint8_t rx_addr;
    uint8_t ack = 1;

    // printf("%s: rx_len=%d\n", __func__, s->rx_len);

    if (s->rx_ack_pending) {
        s->rx_ack_pending = false;
        s->rx_len = 0;
        return;
    }

    switch (s->rx_len) {
    case START_LEN:
        rx_addr = s->rx_buf[0];
        assert((rx_addr & 1) == 0);
        rx_addr >>= 1;
        if (i2c_start_send(s->bus, rx_addr) != 0) {
            printf("%s: i2c_start_send to 0x%02x failed\n", __func__, rx_addr);
            ack = 0;
            i2c_bus_release(s->bus);
        }
        qemu_send_packet(netdev, &ack, sizeof(ack));
        break;
    case 1:
        i2c_send_async(s->bus, s->rx_buf[0]);
        qemu_send_packet(netdev, &ack, sizeof(ack));
        break;
    case STOP_LEN:
        i2c_end_transfer(s->bus);
        i2c_bus_release(s->bus);
        return;
    default:
        printf("%s: unexpected rx_len %d\n", __func__, s->rx_len);
        abort();
    }

    s->rx_ack_pending = true;
}

static void i2c_netdev2_realize(DeviceState *dev, Error **errp)
{
    I2CNetdev2 *s = I2C_NETDEV2(dev);

    s->bus = I2C_BUS(qdev_get_parent_bus(dev));
    s->nic = qemu_new_nic(&net_client_info, &s->nic_conf, TYPE_I2C_NETDEV2, dev->id, s);
    s->bh = qemu_bh_new(i2c_netdev2_slave_mode_rx, s);
    s->rx_len = 0;
}

static int i2c_netdev2_handle_event(I2CSlave *i2c, enum i2c_event event)
{
    I2CNetdev2 *s = I2C_NETDEV2(i2c);
    NetClientState *netdev = qemu_get_queue(s->nic);
    uint8_t tx_addr = i2c->address << 1;
    uint8_t start_msg[START_LEN];
    uint8_t stop_msg[STOP_LEN];

    // printf("%s: %d\n", __func__, event);

    switch (event) {
    case I2C_START_RECV:
        printf("%s: RECV UNIMPLEMENTED\n", __func__);
        abort();
    case I2C_START_SEND:
        // printf("%s: SENDING START MESSAGE PACKET\n", __func__);
        memset(start_msg, 0, sizeof(start_msg));
        start_msg[0] = tx_addr;
        qemu_send_packet(netdev, start_msg, sizeof(start_msg));
        break;
    case I2C_FINISH:
        memset(stop_msg, 0, sizeof(stop_msg));
        qemu_send_packet(netdev, stop_msg, sizeof(stop_msg));
        break;
    case I2C_NACK:
        printf("%s: NACK UNIMPLEMENTED\n", __func__);
        abort();
    }

    return 0;
}

static uint8_t i2c_netdev2_handle_recv(I2CSlave *i2c)
{
    printf("%s: unimplemented\n", __func__);
    abort();
}

static int i2c_netdev2_handle_send(I2CSlave *i2c, uint8_t byte)
{
    I2CNetdev2 *s = I2C_NETDEV2(i2c);
    NetClientState *netdev = qemu_get_queue(s->nic);

    qemu_send_packet(netdev, &byte, sizeof(byte));

    return 0;
}

static Property i2c_netdev2_props[] = {
    DEFINE_NIC_PROPERTIES(I2CNetdev2, nic_conf),
    DEFINE_PROP_END_OF_LIST(),
};

static void i2c_netdev2_class_init(ObjectClass *cls, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(cls);
    I2CSlaveClass *sc = I2C_SLAVE_CLASS(cls);

    dc->realize = i2c_netdev2_realize;
    device_class_set_props(dc, i2c_netdev2_props);

    sc->event = i2c_netdev2_handle_event;
    sc->recv = i2c_netdev2_handle_recv;
    sc->send = i2c_netdev2_handle_send;
}

static const TypeInfo i2c_netdev2 = {
    .name = TYPE_I2C_NETDEV2,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(I2CNetdev2),
    .class_init = i2c_netdev2_class_init,
};

static void register_types(void)
{
    type_register_static(&i2c_netdev2);
}

type_init(register_types);
