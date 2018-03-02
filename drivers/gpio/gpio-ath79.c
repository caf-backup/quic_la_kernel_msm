/*
 *  Atheros AR71XX/AR724X/AR913X GPIO API support
 *
 *  Copyright (C) 2010-2011 Jaiganesh Narayanan <jnarayanan@atheros.com>
 *  Copyright (C) 2008-2011 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  Parts of this file are based on Atheros' 2.6.15/2.6.31 BSP
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/gpio/driver.h>
#include <linux/platform_data/gpio-ath79.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>

#include <asm/mach-ath79/ar71xx_regs.h>

struct ath79_gpio_ctrl {
	struct gpio_chip chip;
	void __iomem *base;
	spinlock_t lock;
	int irq;
	int irq_base;
};

#define to_ath79_gpio_ctrl(c) container_of(c, struct ath79_gpio_ctrl, chip)

static void ath79_gpio_set_value(struct gpio_chip *chip,
				unsigned gpio, int value)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);

	if (value)
		__raw_writel(BIT(gpio), ctrl->base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(BIT(gpio), ctrl->base + AR71XX_GPIO_REG_CLEAR);
}

static int ath79_gpio_get_value(struct gpio_chip *chip, unsigned gpio)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);

	return (__raw_readl(ctrl->base + AR71XX_GPIO_REG_IN) >> gpio) & 1;
}

static int ath79_gpio_direction_input(struct gpio_chip *chip,
				       unsigned offset)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);
	unsigned long flags;

	spin_lock_irqsave(&ctrl->lock, flags);

	__raw_writel(
		__raw_readl(ctrl->base + AR71XX_GPIO_REG_OE) & ~BIT(offset),
		ctrl->base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

static int ath79_gpio_direction_output(struct gpio_chip *chip,
					unsigned offset, int value)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);
	unsigned long flags;

	spin_lock_irqsave(&ctrl->lock, flags);

	if (value)
		__raw_writel(BIT(offset), ctrl->base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(BIT(offset), ctrl->base + AR71XX_GPIO_REG_CLEAR);

	__raw_writel(
		__raw_readl(ctrl->base + AR71XX_GPIO_REG_OE) | BIT(offset),
		ctrl->base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

static int ar934x_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);
	unsigned long flags;

	spin_lock_irqsave(&ctrl->lock, flags);

	__raw_writel(
		__raw_readl(ctrl->base + AR71XX_GPIO_REG_OE) | BIT(offset),
		ctrl->base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

static int ar934x_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int value)
{
	struct ath79_gpio_ctrl *ctrl = to_ath79_gpio_ctrl(chip);
	unsigned long flags;

	spin_lock_irqsave(&ctrl->lock, flags);

	if (value)
		__raw_writel(BIT(offset), ctrl->base + AR71XX_GPIO_REG_SET);
	else
		__raw_writel(BIT(offset), ctrl->base + AR71XX_GPIO_REG_CLEAR);

	__raw_writel(
		__raw_readl(ctrl->base + AR71XX_GPIO_REG_OE) & ~BIT(offset),
		ctrl->base + AR71XX_GPIO_REG_OE);

	spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

static int ath79_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct ath79_gpio_ctrl *ctrl = container_of(chip, struct ath79_gpio_ctrl, chip);

	return ctrl->irq_base + offset;
}

static void ath79_gpio_irq_handler(struct irq_desc *desc)
{
	struct irq_data *data = irq_desc_get_irq_data(desc);
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_handler_data(data);
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	u32 pending;

	pending = __raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_PENDING) &
		  __raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_ENABLE);

	if (!pending) {
		spurious_interrupt();
		return;
	}

	while (pending) {
		int bit = __ffs(pending);
		generic_handle_irq(ctrl->irq_base + bit);

		pending &= ~BIT(bit);
	}

	chip->irq_ack(data);
}

static void ath79_gpio_irq_mask(struct irq_data *data)
{
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - ctrl->irq_base;

	__raw_writel(__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_ENABLE) & ~BIT(gpio),
		     ctrl->base + AR71XX_GPIO_REG_INT_ENABLE);
}

static void ath79_gpio_irq_unmask(struct irq_data *data)
{
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - ctrl->irq_base;

	__raw_writel(__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_ENABLE) | BIT(gpio),
		     ctrl->base + AR71XX_GPIO_REG_INT_ENABLE);
}

static void ath79_gpio_irq_enable(struct irq_data *data)
{
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - ctrl->irq_base;

	__raw_writel(__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_MODE) | BIT(gpio),
		     ctrl->base + AR71XX_GPIO_REG_INT_MODE);

	ath79_gpio_irq_unmask(data);
}

static void ath79_gpio_irq_disable(struct irq_data *data)
{
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - ctrl->irq_base;

	__raw_writel(__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_MODE) & ~BIT(gpio),
		     ctrl->base + AR71XX_GPIO_REG_INT_MODE);

	ath79_gpio_irq_mask(data);
}

static int ath79_gpio_irq_set_type(struct irq_data *data, unsigned type)
{
	struct ath79_gpio_ctrl *ctrl = irq_data_get_irq_chip_data(data);
	int gpio = data->irq - ctrl->irq_base;
	int val, polarity;

	if (gpio < 0 || gpio >= ctrl->chip.ngpio)
		return -EINVAL;

	switch (type) {
	case IRQF_TRIGGER_NONE:
		break;
	case IRQF_TRIGGER_RISING:
		val = 0;
		polarity = 1;
		break;
	case IRQF_TRIGGER_FALLING:
		val = 0;
		polarity = 0;
		break;
	case IRQF_TRIGGER_HIGH:
		val = 1;
		polarity = 1;
		break;
	case IRQF_TRIGGER_LOW:
		val = 1;
		polarity = 0;
		break;
	default:
		printk("invalid type %d for irq %ld\n",
			type, data->hwirq);
		return -EINVAL;
	}

	if (val)
		__raw_writel(
			__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_TYPE) | BIT(gpio),
			ctrl->base + AR71XX_GPIO_REG_INT_TYPE);
	else
		__raw_writel(
			__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_TYPE) & ~BIT(gpio),
			ctrl->base + AR71XX_GPIO_REG_INT_TYPE);

	if (polarity)
		__raw_writel(
			__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_POLARITY) | BIT(gpio),
			ctrl->base + AR71XX_GPIO_REG_INT_POLARITY);
	else
		__raw_writel(
			__raw_readl(ctrl->base + AR71XX_GPIO_REG_INT_POLARITY) & ~BIT(gpio),
			ctrl->base + AR71XX_GPIO_REG_INT_POLARITY);

	return 0;
}

static struct irq_chip ath79_gpio_irqchip = {
	.name			= "ath79-gpio",
	.irq_mask		= ath79_gpio_irq_mask,
	.irq_unmask		= ath79_gpio_irq_unmask,
	.irq_enable		= ath79_gpio_irq_enable,
	.irq_disable		= ath79_gpio_irq_disable,
	.irq_set_type		= ath79_gpio_irq_set_type,
};

static const struct gpio_chip ath79_gpio_chip = {
	.label			= "ath79",
	.get			= ath79_gpio_get_value,
	.set			= ath79_gpio_set_value,
	.direction_input	= ath79_gpio_direction_input,
	.direction_output	= ath79_gpio_direction_output,
	.base			= 0,
};

static const struct of_device_id ath79_gpio_of_match[] = {
	{ .compatible = "qca,ar7100-gpio" },
	{ .compatible = "qca,ar9340-gpio" },
	{},
};

static int ath79_gpio_probe(struct platform_device *pdev)
{
	struct ath79_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct device_node *np = pdev->dev.of_node;
	struct ath79_gpio_ctrl *ctrl;
	struct resource *res;
	u32 ath79_gpio_count;
	bool oe_inverted;
	int i, err;

	ctrl = devm_kzalloc(&pdev->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	if (np) {
		err = of_property_read_u32(np, "ngpios", &ath79_gpio_count);
		if (err) {
			dev_err(&pdev->dev, "ngpios property is not valid\n");
			return err;
		}
		if (ath79_gpio_count >= 32) {
			dev_err(&pdev->dev, "ngpios must be less than 32\n");
			return -EINVAL;
		}
		oe_inverted = of_device_is_compatible(np, "qca,ar9340-gpio");
	} else if (pdata) {
		ath79_gpio_count = pdata->ngpios;
		oe_inverted = pdata->oe_inverted;
	} else {
		dev_err(&pdev->dev, "No DT node or platform data found\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctrl->base = devm_ioremap_nocache(
		&pdev->dev, res->start, resource_size(res));
	if (!ctrl->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -EINVAL;

	ctrl->irq = res->start;

	spin_lock_init(&ctrl->lock);
	memcpy(&ctrl->chip, &ath79_gpio_chip, sizeof(ctrl->chip));
	ctrl->chip.dev = &pdev->dev;
	ctrl->chip.ngpio = ath79_gpio_count;
	if (oe_inverted) {
		ctrl->chip.direction_input = ar934x_gpio_direction_input;
		ctrl->chip.direction_output = ar934x_gpio_direction_output;
	}

	if (pdata->irq_base) {
		ctrl->irq_base = pdata->irq_base;
		ctrl->chip.to_irq = ath79_gpio_to_irq;
	}

	err = gpiochip_add(&ctrl->chip);
	if (err) {
		dev_err(&pdev->dev,
			"cannot add AR71xx GPIO chip, error=%d", err);
		return err;
	}

	for (i = 0; i < ctrl->chip.ngpio; i++) {
		irq_set_chip_data(i + ctrl->irq_base, ctrl);
		irq_set_chip_and_handler(i + ctrl->irq_base,
					 &ath79_gpio_irqchip,
					 handle_simple_irq);
	}

	irq_set_chained_handler_and_data(ctrl->irq, ath79_gpio_irq_handler, ctrl);

	return 0;
}

static struct platform_driver ath79_gpio_driver = {
	.driver = {
		.name = "ath79-gpio",
		.of_match_table	= ath79_gpio_of_match,
	},
	.probe = ath79_gpio_probe,
};

module_platform_driver(ath79_gpio_driver);
