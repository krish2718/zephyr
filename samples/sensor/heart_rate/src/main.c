/*
 * Copyright (c) 2017, NXP
 * Copyright (c) 2025, CATIE
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(heart_rate, LOG_LEVEL_INF);

/* HR mode only activates the RED LED */
#define MAX30101_SENSOR_CHANNEL SENSOR_CHAN_RED

/* MAX30101 register addresses */
#define MAX30101_REG_MODE_CFG   0x09
#define MAX30101_REG_PART_ID    0xFF

#define MAX30101_PART_ID_VAL    0x15
#define MAX30101_RESET_BIT      0x40

static const struct device *const max30101_dev =
	DEVICE_DT_GET(DT_NODELABEL(max30101_sensor));

static const struct i2c_dt_spec max30101_i2c =
	I2C_DT_SPEC_GET(DT_NODELABEL(max30101_sensor));

static int max30101_soft_reset(void)
{
	uint8_t mode_cfg;
	int ret;

	ret = i2c_reg_write_byte_dt(&max30101_i2c, MAX30101_REG_MODE_CFG,
				    MAX30101_RESET_BIT);
	if (ret) {
		return ret;
	}

	k_msleep(100);

	for (int i = 0; i < 50; i++) {
		ret = i2c_reg_read_byte_dt(&max30101_i2c, MAX30101_REG_MODE_CFG,
					   &mode_cfg);
		if (ret) {
			return ret;
		}
		if (!(mode_cfg & MAX30101_RESET_BIT)) {
			return 0;
		}
		k_msleep(10);
	}

	return -ETIMEDOUT;
}

static int max30101_verify_part_id(void)
{
	uint8_t part_id;
	int ret;

	ret = i2c_reg_read_byte_dt(&max30101_i2c, MAX30101_REG_PART_ID,
				   &part_id);
	if (ret) {
		return ret;
	}

	if (part_id != MAX30101_PART_ID_VAL) {
		LOG_ERR("Part ID mismatch: got 0x%02x, expected 0x%02x",
			part_id, MAX30101_PART_ID_VAL);
		return -EIO;
	}

	return 0;
}

static int do_sensor_init(const struct shell *sh)
{
	int ret;

	if (sh) {
		shell_print(sh, "max30101: soft-resetting sensor...");
	} else {
		LOG_INF("Soft-resetting sensor...");
	}

	ret = max30101_soft_reset();
	if (ret) {
		if (sh) {
			shell_error(sh, "max30101: soft reset failed (err %d)", ret);
		} else {
			LOG_ERR("Soft reset failed (err %d)", ret);
		}
		return ret;
	}

	ret = max30101_verify_part_id();
	if (ret) {
		if (sh) {
			shell_error(sh, "max30101: Part ID check failed (err %d)", ret);
		} else {
			LOG_ERR("Part ID check failed (err %d)", ret);
		}
		return ret;
	}

	if (sh) {
		shell_print(sh, "max30101: Part ID verified (0x15)");
	} else {
		LOG_INF("Part ID verified (0x15)");
	}

	ret = device_init(max30101_dev);
	if (ret) {
		if (sh) {
			shell_error(sh, "max30101: device_init() failed (err %d)", ret);
		} else {
			LOG_ERR("device_init() failed (err %d)", ret);
		}
		return ret;
	}

	if (sh) {
		shell_print(sh, "max30101: initialized successfully");
	} else {
		LOG_INF("Initialized successfully");
	}

	return 0;
}

static int cmd_sensor_init(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (device_is_ready(max30101_dev)) {
		shell_print(sh, "max30101: already initialized");
		return 0;
	}

	return do_sensor_init(sh);
}

static int cmd_sensor_read(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!device_is_ready(max30101_dev)) {
		shell_error(sh, "max30101: not initialized, run 'sensor_init' first");
		return -ENODEV;
	}

	struct sensor_value val;

	sensor_sample_fetch(max30101_dev);
	sensor_channel_get(max30101_dev, MAX30101_SENSOR_CHANNEL, &val);
	shell_print(sh, "RED = %d", val.val1);
	return 0;
}

SHELL_CMD_REGISTER(sensor_init, NULL, "Re-initialize MAX30101 sensor",
		   cmd_sensor_init);
SHELL_CMD_REGISTER(sensor_read, NULL, "Read one MAX30101 sample",
		   cmd_sensor_read);

int main(void)
{
	if (!device_is_ready(max30101_i2c.bus)) {
		LOG_ERR("I2C bus not ready");
		return 0;
	}

	if (!device_is_ready(max30101_dev)) {
		do_sensor_init(NULL);
	}

	if (!device_is_ready(max30101_dev)) {
		LOG_WRN("Sensor not ready, use 'sensor_init' to retry");
	}

	return 0;
}
