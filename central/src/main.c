/* main.c - Application main entry point */

/*
 * Copyright (c) 2020 SixOctets Systems
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stddef.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define LOG_LEVEL 3
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

static int scan_start(void);

static struct bt_conn *default_conn;
static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static uint16_t default_conn_handle;

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void read_conn_rssi(uint16_t handle, int8_t *rssi)
{
	struct net_buf *buf;
	struct net_buf *rsp = NULL;
	struct bt_hci_cp_read_rssi *cp;
	struct bt_hci_rp_read_rssi *rp;

	uint8_t reason;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
	if (!buf) {
		LOG_INF("Unable to allocate command buffer");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
	if (err) {
		reason = rsp ? ((struct bt_hci_rp_read_rssi *)rsp->data)->status : 0;
		LOG_INF("Read RSSI err: %d reason 0x%02X", err, reason);
		return;
	}

	rp = (void *)rsp->data;
	*rssi = rp->rssi;

	net_buf_unref(rsp);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
int8_t get_rssi(void)
{
	int8_t rssi = 0xFF;

	read_conn_rssi(default_conn_handle, &rssi);

	return rssi;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static double pow(double x, double y)
{
	double result = 1;

	if (y < 0) {
		y = -y;
		while (y--) {
			result /= x;
		}
	} else {
		while (y--) {
			result *= x;
		}
	}

	return result;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static uint8_t notify_func(struct bt_conn *conn,
			   struct bt_gatt_subscribe_params *params,
			   const void *data, uint16_t length)
{
	double temperature;
	uint32_t mantissa;
	int8_t exponent;

	if (!data) {
		LOG_INF("[UNSUBSCRIBED]");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}

	/* temperature value display */
	mantissa = sys_get_le24(&((uint8_t *)data)[1]);
	exponent = ((uint8_t *)data)[4];
	temperature = (double)mantissa * pow(10, exponent);

	LOG_INF("Temperature %gC -- RSSI %ddB", temperature, get_rssi());

	return BT_GATT_ITER_CONTINUE;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_INF("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HTS)) {
		memcpy(&uuid, BT_UUID_HTS_MEASUREMENT, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed: %d", err);
		}
	} 
	else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_HTS_MEASUREMENT)) {
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed: %d", err);
		}
	} 
	else {
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_INDICATE;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Subscribe failed: %d", err);
		}
		else {			
			LOG_INF("[SUBSCRIBED]");		
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s: %u", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		scan_start();
		return;
	}

	LOG_INF("Connected: %s", addr);

	if (conn == default_conn) {
		memcpy(&uuid, BT_UUID_HTS, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
		discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			LOG_ERR("Discover failed(err %d)", err);
			return;
		}
	}
}

/*---------------------------------------------------------------------------*/
/*  Extended Inquiry Response (EIR) parser                                   */
/*---------------------------------------------------------------------------*/
static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	struct bt_conn_le_create_param *conn_params;
	int i;

	LOG_INF("[AdvData]: type %u, data_len %u", data->type, data->data_len);

	switch (data->type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data_len % sizeof(uint16_t) != 0U) {
			LOG_WRN("AdvData malformed");
			return true;
		}

		for (i = 0; i < data->data_len; i += sizeof(uint16_t)) {
			const struct bt_uuid *uuid;
			uint16_t u16;
			int err;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			err = bt_uuid_cmp(uuid, BT_UUID_HTS);
			if (err) {
				continue;
			}

			LOG_INF("Stop scanning");

			err = bt_le_scan_stop();
			if (err) {
				LOG_ERR("Stop LE scan failed: %d", err);
				continue;
			}

			LOG_INF("Create connection");

			conn_params = BT_CONN_LE_CREATE_PARAM(
					BT_CONN_LE_OPT_CODED | BT_CONN_LE_OPT_NO_1M,
					BT_GAP_SCAN_FAST_INTERVAL,
					BT_GAP_SCAN_FAST_INTERVAL);

			err = bt_conn_le_create(addr, conn_params,
						BT_LE_CONN_PARAM_DEFAULT,
						&default_conn);
			if (err) {
				LOG_ERR("Create connection failed: %d", err);
				scan_start();
			}

			return false;
		}
	}

	return true;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));

	LOG_INF("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i",
	       dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_HCI_ADV_IND || 
		type == BT_HCI_ADV_DIRECT_IND ||
		type == BT_GAP_ADV_TYPE_EXT_ADV) {

		bt_data_parse(ad, eir_found, (void *)addr);
	}
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int scan_start(void)
{
	int err;

	/* 
	 * Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime.
	 */
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
		.options    = BT_LE_SCAN_OPT_CODED | BT_LE_SCAN_OPT_NO_1M
	};

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		LOG_ERR("Scan start failed: %d", err);
	}
	else {
		LOG_INF("Scan start successfully");
	}

	return err;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason 0x%02x)", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	err = scan_start();
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
	}
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void le_phy_updated(struct bt_conn *conn,
			   struct bt_conn_le_phy_info *param)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("LE PHY Updated: %s Tx 0x%x, Rx 0x%x", 
		    addr, param->tx_phy, param->rx_phy);
}
#endif /* CONFIG_BT_USER_PHY_UPDATE */

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#if defined(CONFIG_BT_USER_PHY_UPDATE)
	.le_phy_updated = le_phy_updated,
#endif	
};

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	LOG_INF("Bluetooth initialized as Central scanning for \"%s\"",
		    DEVICE_NAME);

	err = scan_start();
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return err;
	}

	LOG_INF("Scanning successfully started");

	return 0;
}
