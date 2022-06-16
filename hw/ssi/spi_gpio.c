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

#define SPI_CPHA BIT(0) /* clock phase (1 = SPI_CLOCK_PHASE_SECOND) */
#define SPI_CPOL BIT(1) /* clock polarity (1 = SPI_POLARITY_HIGH) */

static void do_leading_edge(SpiGpioState *s)
{
    if (s->CPHA) {
        s->miso = !!(s->output_byte & 0x80);
        object_property_set_bool(OBJECT(s->aspeed_gpio),
                                 "gpioX5", s->miso, NULL);
    } else {
        s->input_byte |= s->mosi ? 1 : 0;
    }
}

static void do_trailing_edge(SpiGpioState *s)
{
    if (s->CPHA) {
        s->output_byte |= s->mosi ? 1 : 0;
    } else {
        s->miso = !!(s->output_byte & 0x80);
        object_property_set_bool(OBJECT(s->aspeed_gpio),
                                 "gpioX5", s->miso, NULL);
    }
}

static void cs_set_level(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);
    s->cs = !!level;

    /* relay the CS value to the CS output pin */
    qemu_set_irq(s->cs_output_pin, s->cs);

    s->miso = !!(s->output_byte & 0x80);
    object_property_set_bool(OBJECT(s->aspeed_gpio),
                             "gpioX5", s->miso, NULL);

    s->clk = !!(s->mode & SPI_CPOL);
}

static void clk_set_level(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);

    bool cur = !!level;

    /* CS# is high/not selected, do nothing */
    if (s->cs) {
        return;
    }

    /* When the lock has not changed, do nothing */
    if (s->clk == cur) {
        return;
    }

    s->clk = cur;

    /* Leading edge */
    if (s->clk != s->CIDLE) {
        do_leading_edge(s);
    }

    /* Trailing edge */
    if (s->clk == s->CIDLE) {
        do_trailing_edge(s);
        s->clk_counter++;

        /*
         * Deliver the input to and
         * get the next output byte
         * from the SPI device
         */
        if (s->clk_counter == 8) {
            s->output_byte = ssi_transfer(s->spi, s->input_byte);
            s->clk_counter = 0;
            s->input_byte = 0;
         } else {
            s->input_byte <<= 1;
            s->output_byte <<= 1;
         }
    }
}

static void mosi_set_level(void *opaque, int n, int level)
{
    SpiGpioState *s = SPI_GPIO(opaque);
    s->mosi = !!level;
}

static void spi_gpio_realize(DeviceState *dev, Error **errp)
{
    SpiGpioState *s = SPI_GPIO(dev);

    s->spi = ssi_create_bus(dev, "spi");

    s->mode = 0;
    s->clk_counter = 0;

    s->cs = true;
    s->clk = true;
    s->mosi = true;

    /* Assuming the first output byte is 0 */
    s->output_byte = 0;
    s->CIDLE = !!(s->mode & SPI_CPOL);
    s->CPHA = !!(s->mode & SPI_CPHA);

    /* init the input GPIO lines */
    qdev_init_gpio_in_named(dev, cs_set_level, "SPI_CS_in", 1);
    qdev_init_gpio_in_named(dev, clk_set_level, "SPI_CLK", 1);
    qdev_init_gpio_in_named(dev, mosi_set_level, "SPI_MOSI", 1);

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
