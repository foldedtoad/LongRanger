/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Aaron Tsui <aaron.tsui@outlook.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/hci_vs.h>

#include "hts.h"

#define LOG_LEVEL 3
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HTS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static void start_advertising_coded(struct k_work *work);

static K_WORK_DEFINE(start_advertising_worker, start_advertising_coded);

static struct bt_le_ext_adv *adv;

static struct bt_conn *default_conn;
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
static void connected(struct bt_conn * conn, uint8_t conn_err)
{
	int err;
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn_err) {
		LOG_ERR("Connection failed (err 0x%02X)", conn_err);
		return;
	}

	LOG_INF("Connected");

	default_conn = bt_conn_ref(conn);
	err = bt_hci_get_conn_handle(default_conn, &default_conn_handle);
	if (err) {
		LOG_ERR("No connection handle: %d", err);
	}

	err = bt_conn_get_info(conn, &info);
	if (err) {
		LOG_ERR("Failed to get connection info: %d", err);
	} 
	else {
		const struct bt_conn_le_phy_info *phy_info;
		phy_info = info.le.phy;

		LOG_INF("Connected: %s, tx_phy %u, rx_phy %u",
		       addr, phy_info->tx_phy, phy_info->rx_phy);
	}
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_WRN("Disconnected (reason 0x%02X)", reason);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void le_phy_updated(struct bt_conn *conn,
			   struct bt_conn_le_phy_info *param)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("LE PHY Updated: %s Tx 0x%X, Rx 0x%X", 
		    addr, param->tx_phy, param->rx_phy);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.le_phy_updated = le_phy_updated,
};

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static int create_advertising_coded(void)
{
	int err;

	struct bt_le_adv_param param =
		BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE |
				     BT_LE_ADV_OPT_EXT_ADV |
				     BT_LE_ADV_OPT_CODED,
				     BT_GAP_ADV_FAST_INT_MIN_2,
				     BT_GAP_ADV_FAST_INT_MAX_2,
				     NULL);

	err = bt_le_ext_adv_create(&param, NULL, &adv);
	if (err) {
		LOG_ERR("Failed to create advertiser set: %d", err);
		return err;
	}

	LOG_INF("Created extended advertising");

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Failed to set advertising data: %d", err);
		return err;
	}

	return 0;
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void start_advertising_coded(struct k_work * work)
{
	int err;

	err = bt_le_ext_adv_start(adv, NULL);
	if (err) {
		LOG_ERR("Failed to start advertising set: %d", err);
		return;
	}

	LOG_INF("Advertiser set started");
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
#if 0
static void bt_ready(void)
{
	int err;

	LOG_INF("Bluetooth initialized");

	hts_init();

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}
#endif

#if 0
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing cancelled: %s", addr);
}


/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};
#endif

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*---------------------------------------------------------------------------*/
void main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return;
	}

#if 0
	bt_ready();
#endif	

	err = create_advertising_coded();
	if (err) {
		LOG_ERR("Advertising failed to create: %d", err);
		return;
	}

#if 0
	bt_conn_auth_cb_register(&auth_cb_display);
#endif

	LOG_INF("Bluetooth initialized as Peripheral advertising as \"%s\"",
		    DEVICE_NAME);

	k_work_submit(&start_advertising_worker);

	/* Implement indicate. At the moment there is no suitable way
	 * of starting delayed work so we do it here
	 */
	while (1) {
		k_sleep(K_SECONDS(1));

		/* Temperature measurements simulation */
		hts_indicate();

		/* Battery level simulation */
		bas_notify();
	}
}
