/*
 * Local Zigbee temperature sensor using a Texas Instruments HDC1080 I2C sensor.
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/usb/usb_device.h>

#include <zcl/zb_zcl_temp_measurement_addons.h>

#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

#include "zb_temp_sensor.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define WAIT_FOR_CONSOLE_MSEC 100U
#define WAIT_FOR_CONSOLE_DEADLINE_MSEC 2000U

#define SENSOR_ENDPOINT 42
#define SENSOR_UPDATE_PERIOD_MSEC 30000
#define SENSOR_FIRST_READ_DELAY_MSEC 3000

#define SENSOR_INIT_BASIC_APP_VERSION 1
#define SENSOR_INIT_BASIC_STACK_VERSION 10
#define SENSOR_INIT_BASIC_HW_VERSION 11
#define SENSOR_INIT_BASIC_MANUF_NAME "Bonito"
#define SENSOR_INIT_BASIC_MODEL_ID "Bonito_Nano_HDC1080_Temp"
#define SENSOR_INIT_BASIC_DATE_CODE "20260312"
#define SENSOR_INIT_BASIC_POWER_SOURCE ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
#define ZIGBEE_CHANNEL_MASK_2GHZ_ALL ZB_TRANSCEIVER_ALL_CHANNELS_MASK
#define FORCE_FACTORY_NEW_STARTUP 1

#define TEMP_SENSOR_NODE DT_NODELABEL(hdc1080)
#define I2C_BUS_NODE DT_NODELABEL(i2c0)
#define STATUS_LED_NODE DT_ALIAS(led0)

#define SENSOR_TEMP_CELSIUS_MIN (-40)
#define SENSOR_TEMP_CELSIUS_MAX (125)
#define SENSOR_TEMP_CELSIUS_TOLERANCE (1)
#define ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER 100

#define FW_BUILD_STAMP __DATE__ " " __TIME__

#if DT_NODE_HAS_STATUS(TEMP_SENSOR_NODE, okay)
static const struct device *const temp_sensor = DEVICE_DT_GET(TEMP_SENSOR_NODE);
#else
#error "hdc1080 node must exist in the overlay"
#endif

#if DT_NODE_HAS_STATUS(I2C_BUS_NODE, okay)
static const struct device *const i2c_bus = DEVICE_DT_GET(I2C_BUS_NODE);
#else
#error "i2c0 must exist in the board definition or overlay"
#endif

#if DT_NODE_HAS_STATUS(STATUS_LED_NODE, okay)
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET(STATUS_LED_NODE, gpios);
#define HAVE_STATUS_LED 1
#else
#define HAVE_STATUS_LED 0
#endif

typedef struct {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_temp_measurement_attrs_t temp_attr;
	zb_uint16_t humidity_measure_value;
	zb_uint16_t humidity_min_measure_value;
	zb_uint16_t humidity_max_measure_value;
} sensor_device_ctx_t;

static sensor_device_ctx_t dev_ctx;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(
	identify_attr_list,
	&dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
	basic_attr_list,
	&dev_ctx.basic_attr.zcl_version,
	&dev_ctx.basic_attr.app_version,
	&dev_ctx.basic_attr.stack_version,
	&dev_ctx.basic_attr.hw_version,
	dev_ctx.basic_attr.mf_name,
	dev_ctx.basic_attr.model_id,
	dev_ctx.basic_attr.date_code,
	&dev_ctx.basic_attr.power_source,
	dev_ctx.basic_attr.location_id,
	&dev_ctx.basic_attr.ph_env,
	dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(
	temp_attr_list,
	&dev_ctx.temp_attr.measure_value,
	&dev_ctx.temp_attr.min_measure_value,
	&dev_ctx.temp_attr.max_measure_value,
	&dev_ctx.temp_attr.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(
	humidity_attr_list,
	&dev_ctx.humidity_measure_value,
	&dev_ctx.humidity_min_measure_value,
	&dev_ctx.humidity_max_measure_value);

ZB_DECLARE_TEMP_SENSOR_CLUSTER_LIST(
	temp_sensor_clusters,
	basic_attr_list,
	identify_attr_list,
	temp_attr_list,
	humidity_attr_list);

ZB_DECLARE_TEMP_SENSOR_EP(
	temp_sensor_ep,
	SENSOR_ENDPOINT,
	temp_sensor_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
	temp_sensor_ctx,
	temp_sensor_ep);

static float sensor_value_to_float_local(const struct sensor_value *value)
{
	float sign = (value->val1 < 0 || value->val2 < 0) ? -1.0f : 1.0f;
	int32_t abs_val1 = value->val1 < 0 ? -value->val1 : value->val1;
	int32_t abs_val2 = value->val2 < 0 ? -value->val2 : value->val2;

	return sign * (abs_val1 + abs_val2 / 1000000.0f);
}

static void status_led_set(bool on)
{
#if HAVE_STATUS_LED
	if (gpio_pin_set_dt(&status_led, on ? 1 : 0)) {
		LOG_ERR("Failed to set status LED");
	}
#else
	ARG_UNUSED(on);
#endif
}

static void identify_blink(zb_bufid_t bufid)
{
	static bool on;

	on = !on;
	status_led_set(on);
	ZB_SCHEDULE_APP_ALARM(identify_blink, bufid,
			      ZB_MILLISECONDS_TO_BEACON_INTERVAL(250));
}

static void identify_cb(zb_bufid_t bufid)
{
	if (bufid) {
		LOG_INF("Identify active");
		ZB_SCHEDULE_APP_CALLBACK(identify_blink, bufid);
		return;
	}

	ZB_SCHEDULE_APP_ALARM_CANCEL(identify_blink, ZB_ALARM_ANY_PARAM);
	LOG_INF("Identify inactive");
	status_led_set(ZB_JOINED());
}

#ifdef CONFIG_USB_DEVICE_STACK
static void wait_for_console(void)
{
	const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	uint32_t time = 0;
	int err;

	if (!device_is_ready(console)) {
		LOG_ERR("USB console device not ready");
		return;
	}

	err = usb_enable(NULL);
	if (err) {
		LOG_ERR("Failed to enable USB (%d)", err);
		return;
	}

	while (!dtr && time < WAIT_FOR_CONSOLE_DEADLINE_MSEC) {
		(void)uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(WAIT_FOR_CONSOLE_MSEC));
		time += WAIT_FOR_CONSOLE_MSEC;
	}

	LOG_INF("USB CDC console %s", dtr ? "connected" : "timeout");
}
#endif

static void temp_sensor_clusters_attr_init(void)
{
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.app_version = SENSOR_INIT_BASIC_APP_VERSION;
	dev_ctx.basic_attr.stack_version = SENSOR_INIT_BASIC_STACK_VERSION;
	dev_ctx.basic_attr.hw_version = SENSOR_INIT_BASIC_HW_VERSION;

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name,
			      SENSOR_INIT_BASIC_MANUF_NAME,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_MANUF_NAME));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id,
			      SENSOR_INIT_BASIC_MODEL_ID,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_MODEL_ID));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code,
			      SENSOR_INIT_BASIC_DATE_CODE,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_DATE_CODE));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.location_id,
			      "Desk",
			      ZB_ZCL_STRING_CONST_SIZE("Desk"));

	dev_ctx.basic_attr.power_source = SENSOR_INIT_BASIC_POWER_SOURCE;
	dev_ctx.basic_attr.ph_env = ZB_ZCL_BASIC_ENV_UNSPECIFIED;
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	dev_ctx.temp_attr.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_attr.min_measure_value =
		SENSOR_TEMP_CELSIUS_MIN * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER;
	dev_ctx.temp_attr.max_measure_value =
		SENSOR_TEMP_CELSIUS_MAX * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER;
	dev_ctx.temp_attr.tolerance =
		SENSOR_TEMP_CELSIUS_TOLERANCE *
		ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER;
	dev_ctx.humidity_measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_min_measure_value = 0U;
	dev_ctx.humidity_max_measure_value = 10000U;
}

static void configure_zigbee_channel_masks(void)
{
	zb_set_bdb_primary_channel_set(ZIGBEE_CHANNEL_MASK_2GHZ_ALL);
	zb_set_bdb_secondary_channel_set(ZIGBEE_CHANNEL_MASK_2GHZ_ALL);

	LOG_INF("Configured Zigbee channel mask: 0x%08x",
		(uint32_t)ZIGBEE_CHANNEL_MASK_2GHZ_ALL);
}

static void configure_sleepy_test_mode(void)
{
#ifdef APP_SLEEPY_TEST
	zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));
	zigbee_configure_sleepy_behavior(true);
	LOG_INF("Sleepy test enabled: period=%u ms", SENSOR_UPDATE_PERIOD_MSEC);
#endif
}

static int temp_sensor_init(void)
{
	if (!device_is_ready(temp_sensor)) {
		LOG_ERR("HDC1080 sensor device not ready");
		return -ENODEV;
	}

	LOG_INF("HDC1080 ready on I2C");
	return 0;
}

static void i2c_scan_bus(void)
{
	uint8_t dummy = 0;
	int found = 0;

	if (!device_is_ready(i2c_bus)) {
		LOG_ERR("I2C bus not ready");
		return;
	}

	LOG_INF("Scanning I2C bus on D2/D3");

	for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
		int err = i2c_write(i2c_bus, &dummy, 0, addr);

		if (err == 0) {
			LOG_INF("I2C device found at 0x%02x", addr);
			found++;
		}
	}

	if (found == 0) {
		LOG_INF("No I2C devices found");
	}
}

static int publish_temperature_measurement(void)
{
	struct sensor_value sensor_temperature;
	struct sensor_value sensor_humidity;
	float measured_temperature;
	float measured_humidity;
	int16_t temperature_attribute;
	zb_uint16_t humidity_attribute;
	int err;

	err = sensor_sample_fetch(temp_sensor);
	if (err) {
		LOG_ERR("Failed to fetch sensor sample (%d)", err);
		return err;
	}

	err = sensor_channel_get(temp_sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor_temperature);
	if (err) {
		LOG_ERR("Failed to read temperature channel (%d)", err);
		return err;
	}

	err = sensor_channel_get(temp_sensor, SENSOR_CHAN_HUMIDITY, &sensor_humidity);
	if (err) {
		LOG_ERR("Failed to read humidity channel (%d)", err);
		return err;
	}

	measured_temperature = sensor_value_to_float_local(&sensor_temperature);
	measured_humidity = sensor_value_to_float_local(&sensor_humidity);
	temperature_attribute = (int16_t)
		(measured_temperature * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
	humidity_attribute = (zb_uint16_t)(measured_humidity * 100.0f);

	LOG_INF("Temperature %.2f C, humidity %.2f %% -> attr %d",
		(double)measured_temperature, (double)measured_humidity,
		temperature_attribute);

	err = zb_zcl_set_attr_val(SENSOR_ENDPOINT,
				  ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
				  ZB_ZCL_CLUSTER_SERVER_ROLE,
				  ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
				  (zb_uint8_t *)&temperature_attribute,
				  ZB_FALSE);
	if (err) {
		LOG_ERR("Failed to update Zigbee temperature attribute (%d)", err);
		return err;
	}

	err = zb_zcl_set_attr_val(SENSOR_ENDPOINT,
				  ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
				  ZB_ZCL_CLUSTER_SERVER_ROLE,
				  ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
				  (zb_uint8_t *)&humidity_attribute,
				  ZB_FALSE);
	if (err) {
		LOG_ERR("Failed to update Zigbee humidity attribute (%d)", err);
		return err;
	}

	return 0;
}

static void check_temperature(zb_bufid_t bufid)
{
	int err;

	ZVUNUSED(bufid);

	err = publish_temperature_measurement();
	if (err) {
		LOG_ERR("Temperature update failed (%d)", err);
	}

	err = ZB_SCHEDULE_APP_ALARM(check_temperature, 0,
				    ZB_MILLISECONDS_TO_BEACON_INTERVAL(
					    SENSOR_UPDATE_PERIOD_MSEC));
	if (err) {
		LOG_ERR("Failed to schedule next measurement (%d)", err);
	}
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *signal_header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	switch (signal) {
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		LOG_INF("Zigbee stack started");
		ZB_SCHEDULE_APP_ALARM(check_temperature, 0,
				      ZB_MILLISECONDS_TO_BEACON_INTERVAL(
					      SENSOR_FIRST_READ_DELAY_MSEC));
		break;
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		LOG_INF("Device reboot signal status %d", status);
		break;
	case ZB_BDB_SIGNAL_STEERING:
		LOG_INF("Network steering status %d", status);
		break;
	case ZB_ZDO_SIGNAL_LEAVE:
		LOG_INF("Leave signal status %d", status);
		break;
	default:
		break;
	}

	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
	status_led_set(ZB_JOINED());

	if (bufid) {
		zb_buf_free(bufid);
	}
}

int main(void)
{
	int err;

#ifdef CONFIG_USB_DEVICE_STACK
	wait_for_console();
#endif

	LOG_INF("Starting Zigbee HDC1080 temperature sensor");
	LOG_INF("Firmware build: %s", FW_BUILD_STAMP);
	LOG_INF("Sampling config: first=%u ms, period=%u ms",
		SENSOR_FIRST_READ_DELAY_MSEC, SENSOR_UPDATE_PERIOD_MSEC);

#if HAVE_STATUS_LED
	err = gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure status LED (%d)", err);
	}
#endif

	err = settings_subsys_init();
	if (err) {
		LOG_ERR("Settings init failed (%d)", err);
	}

	i2c_scan_bus();

	err = temp_sensor_init();
	if (err) {
		LOG_ERR("Sensor init failed (%d)", err);
	}

	ZB_AF_REGISTER_DEVICE_CTX(&temp_sensor_ctx);
	temp_sensor_clusters_attr_init();
	configure_zigbee_channel_masks();
	configure_sleepy_test_mode();
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SENSOR_ENDPOINT, identify_cb);

#if FORCE_FACTORY_NEW_STARTUP
	zigbee_erase_persistent_storage(ZB_TRUE);
	LOG_INF("Forced Zigbee factory-new startup");
#endif

	err = settings_load();
	if (err) {
		LOG_ERR("Settings load failed (%d)", err);
	}

	zigbee_enable();
	LOG_INF("Zigbee temperature sensor started");

	return 0;
}
