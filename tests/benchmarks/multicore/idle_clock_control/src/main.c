/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/clocks.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

LOG_MODULE_REGISTER(idle_clock_control);

struct test_clk_ctx {
	const struct device *clk_dev;
	const struct nrf_clock_spec *clk_specs;
	size_t clk_specs_size;
};

const struct nrf_clock_spec test_clk_specs_hsfll[] = {
	{
		.frequency = MHZ(128),
		.accuracy = 0,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = MHZ(320),
		.accuracy = 0,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = MHZ(64),
		.accuracy = 0,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
};

const struct nrf_clock_spec test_clk_specs_fll16m[] = {
	{
		.frequency = MHZ(16),
		.accuracy = 20000,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = MHZ(16),
		.accuracy = 5020,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = MHZ(16),
		.accuracy = 30,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
};

static const struct test_clk_ctx fll16m_test_clk_ctx[] = {
	{
		.clk_dev = DEVICE_DT_GET(DT_NODELABEL(fll16m)),
		.clk_specs = test_clk_specs_fll16m,
		.clk_specs_size = ARRAY_SIZE(test_clk_specs_fll16m),
	},
};

static const struct test_clk_ctx hsfll_test_clk_ctx[] = {
	{
		.clk_dev = DEVICE_DT_GET(DT_NODELABEL(cpuapp_hsfll)),
		.clk_specs = test_clk_specs_hsfll,
		.clk_specs_size = ARRAY_SIZE(test_clk_specs_hsfll),
	},
};

const struct nrf_clock_spec test_clk_specs_lfclk[] = {
	{
		.frequency = 32768,
		.accuracy = 0,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = 32768,
		.accuracy = 20,
		.precision = NRF_CLOCK_CONTROL_PRECISION_DEFAULT,
	},
	{
		.frequency = 32768,
		.accuracy = 20,
		.precision = NRF_CLOCK_CONTROL_PRECISION_HIGH,
	},
};

static const struct test_clk_ctx lfclk_test_clk_ctx[] = {
	{
		.clk_dev = DEVICE_DT_GET(DT_NODELABEL(lfclk)),
		.clk_specs = test_clk_specs_lfclk,
		.clk_specs_size = ARRAY_SIZE(test_clk_specs_lfclk),
	},
};

static void test_request_release_clock_spec(const struct device *clk_dev,
					    const struct nrf_clock_spec *clk_spec)
{
	int ret = 0;
	int res = 0;
	struct onoff_client cli;
	uint32_t rate;

	LOG_INF("Clock under test: %s", clk_dev->name);
	sys_notify_init_spinwait(&cli.notify);
	ret = nrf_clock_control_request(clk_dev, clk_spec, &cli);
	__ASSERT_NO_MSG(ret == 0);
	do {
		ret = sys_notify_fetch_result(&cli.notify, &res);
		k_yield();
	} while (ret == -EAGAIN);
	__ASSERT_NO_MSG(ret == 0);
	__ASSERT_NO_MSG(res == 0);
	ret = clock_control_get_rate(clk_dev, NULL, &rate);
	__ASSERT_NO_MSG(ret == 0);
	__ASSERT_NO_MSG(rate == clk_spec->frequency);
	k_busy_wait(10000);
	ret = nrf_clock_control_release(clk_dev, clk_spec);
	__ASSERT_NO_MSG(ret == ONOFF_STATE_ON);
}

static void test_clock_control_request(const struct test_clk_ctx *clk_contexts,
				       size_t contexts_size)
{
	const struct test_clk_ctx *clk_context;
	size_t clk_specs_size;
	const struct device *clk_dev;
	const struct nrf_clock_spec *clk_spec;

	for (size_t i = 0; i < contexts_size; i++) {
		clk_context = &clk_contexts[i];
		clk_specs_size = clk_context->clk_specs_size;

		for (size_t u = 0; u < clk_specs_size; u++) {
			clk_dev = clk_context->clk_dev;
			clk_spec = &clk_context->clk_specs[u];

			LOG_INF("Applying clock (%s) spec: frequency %d, accuracy %d, precision "
				"%d",
				clk_dev->name, clk_spec->frequency, clk_spec->accuracy,
				clk_spec->precision);
			test_request_release_clock_spec(clk_dev, clk_spec);
			k_msleep(1000);
		}
	}
}

int main(void)
{
	LOG_INF("Idle clock_control, %s", CONFIG_BOARD_TARGET);
	k_msleep(100);
	while (1) {
		test_clock_control_request(hsfll_test_clk_ctx, ARRAY_SIZE(hsfll_test_clk_ctx));
		test_clock_control_request(fll16m_test_clk_ctx, ARRAY_SIZE(fll16m_test_clk_ctx));
		test_clock_control_request(lfclk_test_clk_ctx, ARRAY_SIZE(lfclk_test_clk_ctx));
	}

	return 0;
}
