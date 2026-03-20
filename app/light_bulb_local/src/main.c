/*
 * Local Zigbee On/Off light sample simplified for inspection.
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/usb/usb_device.h>

#include <zb_mem_config_med.h>
#include <zb_nrf_platform.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zigbee/zigbee_zcl_scenes.h>

#include "zb_on_off_light.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define WAIT_FOR_CONSOLE_MSEC 100U
#define WAIT_FOR_CONSOLE_DEADLINE_MSEC 2000U

#define LIGHT_ENDPOINT 10
#define BULB_INIT_BASIC_APP_VERSION 1
#define BULB_INIT_BASIC_STACK_VERSION 10
#define BULB_INIT_BASIC_HW_VERSION 11
#define BULB_INIT_BASIC_MANUF_NAME "Bonito"
#define BULB_INIT_BASIC_MODEL_ID "Bonito_Nano_OnOff_Light"
#define BULB_INIT_BASIC_DATE_CODE "20260311"
#define BULB_INIT_BASIC_POWER_SOURCE ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE
#define BULB_INIT_BASIC_LOCATION_DESC "Desk"
#define BULB_INIT_BASIC_PH_ENV ZB_ZCL_BASIC_ENV_UNSPECIFIED

#define BULB_LED_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(BULB_LED_NODE, okay)
static const struct gpio_dt_spec bulb_led = GPIO_DT_SPEC_GET(BULB_LED_NODE, gpios);
#else
#error "led0 alias must exist in the board definition or overlay"
#endif

typedef struct {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_scenes_attrs_t scenes_attr;
	zb_zcl_groups_attrs_t groups_attr;
	zb_zcl_on_off_attrs_t on_off_attr;
} bulb_device_ctx_t;

static bulb_device_ctx_t dev_ctx;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(
	identify_attr_list,
	&dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(
	groups_attr_list,
	&dev_ctx.groups_attr.name_support);

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(
	scenes_attr_list,
	&dev_ctx.scenes_attr.scene_count,
	&dev_ctx.scenes_attr.current_scene,
	&dev_ctx.scenes_attr.current_group,
	&dev_ctx.scenes_attr.scene_valid,
	&dev_ctx.scenes_attr.name_support);

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

ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST(
	on_off_attr_list,
	&dev_ctx.on_off_attr.on_off);

ZB_DECLARE_ON_OFF_LIGHT_CLUSTER_LIST(
	on_off_light_clusters,
	basic_attr_list,
	identify_attr_list,
	groups_attr_list,
	scenes_attr_list,
	on_off_attr_list);

ZB_DECLARE_ON_OFF_LIGHT_EP(
	on_off_light_ep,
	LIGHT_ENDPOINT,
	on_off_light_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
	on_off_light_ctx,
	on_off_light_ep);

static void light_set_state(bool on)
{
	if (gpio_pin_set_dt(&bulb_led, on ? 1 : 0)) {
		LOG_ERR("Failed to set bulb LED");
	}
}

static void on_off_set_value(zb_bool_t on)
{
	LOG_INF("OnOff set to %u", on);

	ZB_ZCL_SET_ATTRIBUTE(
		LIGHT_ENDPOINT,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
		(zb_uint8_t *)&on,
		ZB_FALSE);

	light_set_state(on == ZB_ZCL_ON_OFF_IS_ON);
}

static void toggle_identify_led(zb_bufid_t bufid)
{
	static bool on;

	on = !on;
	light_set_state(on);
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid,
			      ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

static void identify_cb(zb_bufid_t bufid)
{
	if (bufid) {
		LOG_INF("Identify active");
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
		return;
	}

	ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
	LOG_INF("Identify inactive");
	light_set_state(dev_ctx.on_off_attr.on_off == ZB_ZCL_ON_OFF_IS_ON);
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

static void bulb_clusters_attr_init(void)
{
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.app_version = BULB_INIT_BASIC_APP_VERSION;
	dev_ctx.basic_attr.stack_version = BULB_INIT_BASIC_STACK_VERSION;
	dev_ctx.basic_attr.hw_version = BULB_INIT_BASIC_HW_VERSION;

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name,
			      BULB_INIT_BASIC_MANUF_NAME,
			      ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MANUF_NAME));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id,
			      BULB_INIT_BASIC_MODEL_ID,
			      ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_MODEL_ID));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code,
			      BULB_INIT_BASIC_DATE_CODE,
			      ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_DATE_CODE));
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.location_id,
			      BULB_INIT_BASIC_LOCATION_DESC,
			      ZB_ZCL_STRING_CONST_SIZE(BULB_INIT_BASIC_LOCATION_DESC));

	dev_ctx.basic_attr.power_source = BULB_INIT_BASIC_POWER_SOURCE;
	dev_ctx.basic_attr.ph_env = BULB_INIT_BASIC_PH_ENV;
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
	dev_ctx.on_off_attr.on_off = ZB_ZCL_ON_OFF_IS_ON;

	ZB_ZCL_SET_ATTRIBUTE(
		LIGHT_ENDPOINT,
		ZB_ZCL_CLUSTER_ID_ON_OFF,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
		(zb_uint8_t *)&dev_ctx.on_off_attr.on_off,
		ZB_FALSE);
}

static void zcl_device_cb(zb_bufid_t bufid)
{
	zb_uint8_t cluster_id;
	zb_uint8_t attr_id;
	zb_zcl_device_callback_param_t *device_cb_param =
		ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);

	device_cb_param->status = RET_OK;

	switch (device_cb_param->device_cb_id) {
	case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
		cluster_id = device_cb_param->cb_param.set_attr_value_param.cluster_id;
		attr_id = device_cb_param->cb_param.set_attr_value_param.attr_id;

		if (cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF &&
		    attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
			on_off_set_value(
				(zb_bool_t)device_cb_param->cb_param.set_attr_value_param.values.data8);
		} else if (zcl_scenes_cb(bufid) == ZB_FALSE) {
			device_cb_param->status = RET_NOT_IMPLEMENTED;
		}
		break;

	default:
		if (zcl_scenes_cb(bufid) == ZB_FALSE) {
			device_cb_param->status = RET_NOT_IMPLEMENTED;
		}
		break;
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

	LOG_INF("Starting local Zigbee on/off light");

	if (!gpio_is_ready_dt(&bulb_led)) {
		LOG_ERR("Bulb LED GPIO not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&bulb_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure bulb LED");
		return err;
	}

	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings initialization failed");
	}

	ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);
	ZB_AF_REGISTER_DEVICE_CTX(&on_off_light_ctx);

	bulb_clusters_attr_init();
	on_off_set_value(dev_ctx.on_off_attr.on_off);
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(LIGHT_ENDPOINT, identify_cb);
	zcl_scenes_init();

	err = settings_load();
	if (err) {
		LOG_ERR("settings loading failed");
	}

	zigbee_enable();
	LOG_INF("Local Zigbee on/off light started");

	while (1) {
		k_sleep(K_SECONDS(1));
	}
}
