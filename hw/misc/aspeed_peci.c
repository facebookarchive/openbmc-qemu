/*
 * Aspeed PECI Controller
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)
 *
 * This code is licensed under the GPL version 2 or later. See the COPYING
 * file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/misc/aspeed_peci.h"

#define U(x) (x##U)
#define GENMASK(h, l) \
	(((~U(0)) - (U(1) << (l)) + 1) & \
	 (~U(0) >> (32 - 1 - (h))))

/* ASPEED PECI Registers */
/* Control Register */
#define ASPEED_PECI_CTRL (0x00 / 4)
#define   ASPEED_PECI_CTRL_SAMPLING_MASK 	GENMASK(19, 16)
#define   ASPEED_PECI_CTRL_RD_MODE_MASK 	GENMASK(13, 12)
#define     ASPEED_PECI_CTRL_RD_MODE_DBG BIT(13)
#define     ASPEED_PECI_CTRL_RD_MODE_COUNT BIT(12)
#define   ASPEED_PECI_CTRL_CLK_SRC_HCLK BIT(11)
#define   ASPEED_PECI_CTRL_CLK_DIV_MASK GENMASK(10, 8)
#define   ASPEED_PECI_CTRL_INVERT_OUT BIT(7)
#define   ASPEED_PECI_CTRL_INVERT_IN BIT(6)
#define   ASPEED_PECI_CTRL_BUS_CONTENTION_EN BIT(5)
#define   ASPEED_PECI_CTRL_PECI_EN  BIT(4)
#define   ASPEED_PECI_CTRL_PECI_CLK_EN  BIT(0)

/* Timing Negotiation Register */
#define ASPEED_PECI_TIMING_NEGOTIATION (0x04 / 4)
#define   ASPEED_PECI_T_NEGO_MSG_MASK  GENMASK(15, 8)
#define   ASPEED_PECI_T_NEGO_ADDR_MASK  GENMASK(7, 0)

/* Command Register */
#define ASPEED_PECI_CMD (0x08 / 4)
#define   ASPEED_PECI_CMD_PIN_MONITORING BIT(31)
#define   ASPEED_PECI_CMD_STS_MASK  GENMASK(27, 24)
#define     ASPEED_PECI_CMD_STS_ADDR_T_NEGO 0x3
#define   ASPEED_PECI_CMD_IDLE_MASK  \
   (ASPEED_PECI_CMD_STS_MASK | ASPEED_PECI_CMD_PIN_MONITORING)
#define   ASPEED_PECI_CMD_FIRE   BIT(0)

/* Read/Write Length Register */
#define ASPEED_PECI_RW_LENGTH (0x0c / 4)
#define   ASPEED_PECI_AW_FCS_EN   BIT(31)
#define   ASPEED_PECI_RD_LEN_MASK  GENMASK(23, 16)
#define   ASPEED_PECI_WR_LEN_MASK  GENMASK(15, 8)
#define   ASPEED_PECI_TARGET_ADDR_MASK  GENMASK(7, 0)

/* Expected FCS Data Register */
#define ASPEED_PECI_EXPECTED_FCS (0x10 / 4)
#define   ASPEED_PECI_EXPECTED_RD_FCS_MASK GENMASK(23, 16)
#define   ASPEED_PECI_EXPECTED_AW_FCS_AUTO_MASK GENMASK(15, 8)
#define   ASPEED_PECI_EXPECTED_WR_FCS_MASK GENMASK(7, 0)

/* Captured FCS Data Register */
#define ASPEED_PECI_CAPTURED_FCS (0x14 / 4)
#define   ASPEED_PECI_CAPTURED_RD_FCS_MASK GENMASK(23, 16)
#define   ASPEED_PECI_CAPTURED_WR_FCS_MASK GENMASK(7, 0)

/* Interrupt Register */
#define ASPEED_PECI_INT_CTRL (0x18 / 4)
#define   ASPEED_PECI_TIMING_NEGO_SEL_MASK GENMASK(31, 30)
#define     ASPEED_PECI_1ST_BIT_OF_ADDR_NEGO 0
#define     ASPEED_PECI_2ND_BIT_OF_ADDR_NEGO 1
#define     ASPEED_PECI_MESSAGE_NEGO  2
#define   ASPEED_PECI_INT_MASK   GENMASK(4, 0)
#define     ASPEED_PECI_INT_BUS_TIMEOUT  BIT(4)
#define     ASPEED_PECI_INT_BUS_CONTENTION BIT(3)
#define     ASPEED_PECI_INT_WR_FCS_BAD  BIT(2)
#define     ASPEED_PECI_INT_WR_FCS_ABORT BIT(1)
#define     ASPEED_PECI_INT_CMD_DONE  BIT(0)

/* Interrupt Status Register */
#define ASPEED_PECI_INT_STS (0x1c / 4)
#define   ASPEED_PECI_INT_TIMING_RESULT_MASK GENMASK(29, 16)
   /* bits[4..0]: Same bit fields in the 'Interrupt Register' */

/* Rx/Tx Data Buffer Registers */
#define ASPEED_PECI_WR_DATA0 (0x20 / 4)
#define ASPEED_PECI_WR_DATA1 (0x24 / 4)
#define ASPEED_PECI_WR_DATA2 (0x28 / 4)
#define ASPEED_PECI_WR_DATA3 (0x2c / 4)
#define ASPEED_PECI_RD_DATA0 (0x30 / 4)
#define ASPEED_PECI_RD_DATA1 (0x34 / 4)
#define ASPEED_PECI_RD_DATA2 (0x38 / 4)
#define ASPEED_PECI_RD_DATA3 (0x3c / 4)
#define ASPEED_PECI_WR_DATA4 (0x40 / 4)
#define ASPEED_PECI_WR_DATA5 (0x44 / 4)
#define ASPEED_PECI_WR_DATA6 (0x48 / 4)
#define ASPEED_PECI_WR_DATA7 (0x4c / 4)
#define ASPEED_PECI_RD_DATA4 (0x50 / 4)
#define ASPEED_PECI_RD_DATA5 (0x54 / 4)
#define ASPEED_PECI_RD_DATA6 (0x58 / 4)
#define ASPEED_PECI_RD_DATA7 (0x5c / 4)
#define   ASPEED_PECI_DATA_BUF_SIZE_MAX 32

/** PECI read/write supported responses */
#define PECI_CC_RSP_SUCCESS              (0x40U)
#define PECI_CC_RSP_TIMEOUT              (0x80U)
#define PECI_CC_OUT_OF_RESOURCES_TIMEOUT (0x81U)
#define PECI_CC_RESOURCES_LOWPWR_TIMEOUT (0x82U)
#define PECI_CC_ILLEGAL_REQUEST          (0x90U)

static void aspeed_peci_instance_init(Object *obj)
{
}

static uint64_t aspeed_peci_read(void *opaque, hwaddr addr, unsigned size)
{
    AspeedPECIState *s = ASPEED_PECI(opaque);

    if (addr >= ASPEED_PECI_NR_REGS << 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Out-of-bounds read at offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }
    addr >>= 2;

    uint32_t reg = s->regs[addr];
    //printf("%s:  0x%08lx 0x%08x %u\n", __func__, addr << 2, reg, size);

    return reg;
}

static void aspeed_peci_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    AspeedPECIState *s = ASPEED_PECI(opaque);

    //printf("%s: 0x%08lx 0x%08x %u\n", __func__, addr, reg, size);

    if (addr >= ASPEED_PECI_NR_REGS << 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Out-of-bounds write at offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return;
    }
    addr >>= 2;

    switch (addr) {
    case ASPEED_PECI_INT_STS:
        s->regs[addr] &= ~data;
        break;
    default:
        s->regs[addr] = data;
        break;
    }

    switch (addr) {
    case ASPEED_PECI_CMD:
        if (!(s->regs[ASPEED_PECI_CMD] & ASPEED_PECI_CMD_FIRE)) {
            break;
        }
        s->regs[ASPEED_PECI_RD_DATA0] = PECI_CC_RSP_SUCCESS;
        s->regs[ASPEED_PECI_WR_DATA0] = PECI_CC_RSP_SUCCESS;

        s->regs[ASPEED_PECI_INT_STS] |= ASPEED_PECI_INT_CMD_DONE;
        qemu_irq_raise(s->irq);
        break;
    case ASPEED_PECI_INT_STS:
        if (s->regs[ASPEED_PECI_INT_STS]) {
            break;
        }
        qemu_irq_lower(s->irq);
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: register 0x%03" HWADDR_PRIx " writes unimplemented\n",
                      __func__, addr);
        break;
    }
}

static const MemoryRegionOps aspeed_peci_ops = {
    .read = aspeed_peci_read,
    .write = aspeed_peci_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aspeed_peci_realize(DeviceState *dev, Error **errp)
{
    AspeedPECIState *s = ASPEED_PECI(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->mmio, OBJECT(s), &aspeed_peci_ops, s, TYPE_ASPEED_PECI, 0x1000);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);
}

static void aspeed_peci_reset(DeviceState *dev)
{
    AspeedPECIState *s = ASPEED_PECI(dev);

    memset(s->regs, 0, sizeof(s->regs));
}

static void aspeed_peci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = aspeed_peci_realize;
    dc->reset = aspeed_peci_reset;
    dc->desc = "Aspeed PECI Controller";
}

static const TypeInfo aspeed_peci_info = {
    .name = TYPE_ASPEED_PECI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_init = aspeed_peci_instance_init,
    .instance_size = sizeof(AspeedPECIState),
    .class_init = aspeed_peci_class_init,
    .class_size = sizeof(AspeedPECIClass),
    .abstract = false,
};

static void aspeed_peci_register_types(void)
{
    type_register_static(&aspeed_peci_info);
}

type_init(aspeed_peci_register_types);
