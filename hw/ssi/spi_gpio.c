/*
 * QEMU model of the SPI GPIO controller
 *
 * Copyright (C)
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

#include "hw/ssi/spi_gpio.h"
#include "hw/irq.h"

static void cs_handler(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);
    s->cs = !!level;

    /* relay the CS value to the CS output pin */
    qemu_set_irq(s->cs_output_pin, s->cs);
}

static void clk_handler(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);
    s->clk = !!level;
}

static void mosi_handler(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);
    s->mosi = !!level;
}

static void spi_gpio_realize(DeviceState *dev, Error **errp)
{
    SpiGpioState *s = SPI_GPIO(dev);

    s->spi = ssi_create_bus(dev, "spi");

    s->cs = 1;
    s->clk = 1;
    s->mosi = 1;

    /* init the input GPIO lines */
    qdev_init_gpio_in_named(dev, cs_handler, "SPI_CS_in", 1);
    qdev_init_gpio_in_named(dev, clk_handler, "SPI_CLK", 1);
    qdev_init_gpio_in_named(dev, mosi_handler, "SPI_MOSI", 1);

    /* init the output GPIO lines */
    qdev_init_gpio_out_named(dev, &s->miso_output_pin, "SPI_MISO", 1);
    qdev_init_gpio_out_named(dev, &s->cs_output_pin, "SPI_CS_out", 1);
}

static void SPI_GPIO_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = spi_gpio_realize;
}

static const TypeInfo SPI_GPIO_info = {
    .name           = TYPE_SPI_GPIO,
    .parent         = TYPE_DEVICE,
    .instance_size  = sizeof(SpiGpioState),
    .class_init     = SPI_GPIO_class_init,
};

static void SPI_GPIO_register_types(void)
{
    type_register_static(&SPI_GPIO_info);
}

type_init(SPI_GPIO_register_types)
