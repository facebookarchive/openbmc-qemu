/*
 * QTest testcase for the Aspeed I2C Controller.
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "libqtest-single.h"

#define ASPEED_I2C_BASE 0x1E78A000
#define ASPEED_I2C_BUS0_BASE (ASPEED_I2C_BASE + 0x80)
#define I2C_CTRL_GLOBAL 0x0C
#define   I2C_CTRL_NEW_REG_MODE BIT(2)
#define I2CD_FUN_CTRL_REG 0x00
#define   I2CD_SLAVE_EN (0x1 << 1)
#define   I2CD_MASTER_EN (0x1)
#define I2CD_INTR_CTRL_REG 0x0c
#define I2CD_INTR_STS_REG 0x10
#define   I2CD_INTR_SLAVE_ADDR_RX_MATCH    (0x1 << 7)  /* use RX_DONE */
#define   I2CD_INTR_NORMAL_STOP            (0x1 << 4)
#define   I2CD_INTR_RX_DONE                (0x1 << 2)
#define I2CD_CMD_REG            0x14       /* I2CD Command/Status */
#define   I2CD_RX_DMA_ENABLE               (0x1 << 9)
#define   I2CD_TX_DMA_ENABLE               (0x1 << 8)
#define   I2CD_RX_BUFF_ENABLE              (0x1 << 7)
#define   I2CD_TX_BUFF_ENABLE              (0x1 << 6)
#define   I2CD_M_STOP_CMD                  (0x1 << 5)
#define   I2CD_M_S_RX_CMD_LAST             (0x1 << 4)
#define   I2CD_M_RX_CMD                    (0x1 << 3)
#define   I2CD_S_TX_CMD                    (0x1 << 2)
#define   I2CD_M_TX_CMD                    (0x1 << 1)
#define   I2CD_M_START_CMD                 (0x1)
#define I2CD_DEV_ADDR_REG       0x18       /* Slave Device Address */
#define I2CD_BYTE_BUF_REG       0x20       /* Transmit/Receive Byte Buffer */
#define   I2CD_BYTE_BUF_TX_SHIFT           0
#define   I2CD_BYTE_BUF_TX_MASK            0xff
#define   I2CD_BYTE_BUF_RX_SHIFT           8
#define   I2CD_BYTE_BUF_RX_MASK            0xff

#define DATA_LEN 1
#define ACK_LEN 2
#define START_LEN 3
#define STOP_LEN 4

static void aspeed_i2c_master_mode_tx(const uint8_t *buf, int len)
{
    int i;

    writel(ASPEED_I2C_BUS0_BASE + I2CD_BYTE_BUF_REG, buf[0]);
    writel(ASPEED_I2C_BUS0_BASE + I2CD_CMD_REG, I2CD_M_START_CMD);

    for (i = 1; i < len; i++) {
        writel(ASPEED_I2C_BUS0_BASE + I2CD_BYTE_BUF_REG, buf[i]);
        writel(ASPEED_I2C_BUS0_BASE + I2CD_CMD_REG, I2CD_M_TX_CMD);
    }

    writel(ASPEED_I2C_BUS0_BASE + I2CD_CMD_REG, I2CD_M_STOP_CMD);
}

static void aspeed_i2c_slave_mode_enable(uint8_t addr)
{
    writel(ASPEED_I2C_BUS0_BASE + I2CD_DEV_ADDR_REG, addr);
    writel(ASPEED_I2C_BUS0_BASE + I2CD_FUN_CTRL_REG, I2CD_SLAVE_EN);
}

static int aspeed_i2c_slave_mode_rx(uint8_t *buf, int len)
{
    uint32_t sts;
    uint32_t byte_buf;
    int i = 0;

    while (i < len) {
        sts = readl(ASPEED_I2C_BUS0_BASE + I2CD_INTR_STS_REG);
        if (sts & I2CD_INTR_NORMAL_STOP) {
            break;
        }
        if (sts & I2CD_INTR_RX_DONE) {
            byte_buf = readl(ASPEED_I2C_BUS0_BASE + I2CD_BYTE_BUF_REG);
            buf[i++] = (byte_buf >> I2CD_BYTE_BUF_RX_SHIFT) & I2CD_BYTE_BUF_RX_MASK;
        }
        writel(ASPEED_I2C_BUS0_BASE + I2CD_INTR_STS_REG, sts);
    }

    return i;
}

static uint8_t aspeed_i2c_slave_mode_rx_byte(void)
{
    uint8_t buf = 0xff;

    g_assert_cmphex(aspeed_i2c_slave_mode_rx(&buf, sizeof(buf)), ==, 1);
    return buf;
}

static int udp_socket;

static void test_write_in_old_byte_mode(void)
{
    uint8_t pkt[] = {0x64, 0xde, 0xad, 0xbe, 0xef};
    uint8_t buf[10];
    ssize_t n;
    int i;
    struct sockaddr_in rx_addr;
    uint8_t ack[ACK_LEN] = {1, 0};

    rx_addr.sin_family = AF_INET;
    rx_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    rx_addr.sin_port = htons(6000);

    g_assert(!(readl(ASPEED_I2C_BASE + I2C_CTRL_GLOBAL) & I2C_CTRL_NEW_REG_MODE));

    writel(ASPEED_I2C_BUS0_BASE + I2CD_FUN_CTRL_REG, I2CD_MASTER_EN);
    writel(ASPEED_I2C_BUS0_BASE + I2CD_INTR_CTRL_REG, 0xFFFFFFFF);

    aspeed_i2c_master_mode_tx(pkt, sizeof(pkt));

    n = recv(udp_socket, buf, sizeof(buf), 0);
    g_assert_cmphex(n, ==, START_LEN);
    g_assert_cmphex(buf[0], ==, pkt[0]);
    sendto(udp_socket, ack, sizeof(ack), 0, (const struct sockaddr_in*)&rx_addr, sizeof(rx_addr));

    for (i = 1; i < sizeof(pkt); i++) {
        n = recv(udp_socket, buf, sizeof(buf), 0);
        g_assert_cmphex(n, ==, DATA_LEN);
        g_assert_cmphex(buf[0], ==, pkt[i]);
        sendto(udp_socket, ack, sizeof(ack), 0, (const struct sockaddr_in*)&rx_addr, sizeof(rx_addr));
    }

    n = recv(udp_socket, buf, sizeof(buf), 0);
    g_assert_cmphex(n, ==, STOP_LEN);
}

static void test_slave_mode_rx_byte_buf(void)
{
    uint8_t b;
    uint32_t sts;

    g_assert(!(readl(ASPEED_I2C_BASE + I2C_CTRL_GLOBAL) & I2C_CTRL_NEW_REG_MODE));

    writel(ASPEED_I2C_BUS0_BASE + I2CD_INTR_STS_REG, 0xFFFFFFFF);
    writel(ASPEED_I2C_BUS0_BASE + I2CD_INTR_CTRL_REG, 0xFFFFFFFF);

    aspeed_i2c_slave_mode_enable(0x10);

    struct sockaddr_in dst;
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = inet_addr("127.0.0.1");
    dst.sin_port = htons(6000);

    uint8_t pkt[] = {0x20, 0xde, 0xad, 0xbe, 0xef};
    uint8_t buf[10] = {};

    buf[0] = 0x20;
    sendto(udp_socket, buf, START_LEN, 0, (const struct sockaddr*)&dst, sizeof(dst));
    b = aspeed_i2c_slave_mode_rx_byte();
    g_assert_cmphex(b, ==, buf[0]);

    for (int i = 1; i < sizeof(pkt); i++) {
        buf[0] = pkt[i];
        sendto(udp_socket, buf, DATA_LEN, 0, (const struct sockaddr*)&dst, sizeof(dst));
        b = aspeed_i2c_slave_mode_rx_byte();
        g_assert_cmphex(b, ==, buf[0]);
    }

    sendto(udp_socket, buf, STOP_LEN, 0, (const struct sockaddr*)&dst, sizeof(dst));
    for (int i = 0; i < 10000; i++) {
        sts = readl(ASPEED_I2C_BUS0_BASE + I2CD_INTR_STS_REG);
        writel(ASPEED_I2C_BUS0_BASE + I2CD_INTR_STS_REG, sts);
        if (sts & I2CD_INTR_NORMAL_STOP) {
            break;
        }
    }
    g_assert(sts & I2CD_INTR_NORMAL_STOP);
}

static int udp_socket_init(const char *ip_addr, uint16_t port)
{
    bool reuseaddr = true;
    struct sockaddr_in addr;
    int fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr*)&addr, sizeof(addr))) {
        perror("bind");
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    int ret;

    udp_socket = udp_socket_init("127.0.0.1", 5000);
    if (udp_socket == -1) {
        return 1;
    }

    g_test_init(&argc, &argv, NULL);

    global_qtest = qtest_initf("-machine fby35-bmc "
                               "-netdev socket,id=socket0,udp=localhost:5000,localaddr=localhost:6000 "
                               "-device i2c-netdev2,bus=aspeed.i2c.bus.0,address=0x32,netdev=socket0");

    qtest_add_func("/ast2600/i2c/write_in_old_byte_mode", test_write_in_old_byte_mode);
    qtest_add_func("/ast2600/i2c/slave_mode_rx_byte_buf", test_slave_mode_rx_byte_buf);

    ret = g_test_run();
    qtest_quit(global_qtest);
    close(udp_socket);

    return ret;
}
