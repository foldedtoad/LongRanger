/*
 * Copyright (c) 2024 Callender Consulting LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/hci.h>

#include <zephyr/init.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/drivers/bluetooth/hci_driver.h>

#include <cyw43_bt.h>

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_hci_driver_pico_w);

#define HCI_CMD                 0x01
#define HCI_ACL                 0x02
#define HCI_SCO                 0x03
#define HCI_EVT                 0x04
#define HCI_ISO                 0x05

#define HCI_BT_CYW43_TIMEOUT K_MSEC(2000)

static K_SEM_DEFINE(hci_send_sem, 1, 1);

static bool is_hci_event_discardable(const uint8_t *evt_data)
{
	uint8_t evt_type = evt_data[0];

	switch (evt_type) {
#if defined(CONFIG_BT_BREDR)
	case BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI:
	case BT_HCI_EVT_EXTENDED_INQUIRY_RESULT:
		return true;
#endif
	case BT_HCI_EVT_LE_META_EVENT: {
		uint8_t subevt_type = evt_data[sizeof(struct bt_hci_evt_hdr)];

		switch (subevt_type) {
		case BT_HCI_EVT_LE_ADVERTISING_REPORT:
			return true;
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

static struct net_buf *bt_cyw43_evt_recv(uint8_t *data, size_t remaining)
{
	bool discardable = false;
	struct bt_hci_evt_hdr hdr;
	struct net_buf *buf;
	size_t buf_tailroom;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for event header");
		return NULL;
	}

	discardable = is_hci_event_discardable(data);

	memcpy((void *)&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	remaining -= sizeof(hdr);

	if (remaining != hdr.len) {
		LOG_ERR("Event payload length is not correct");
		return NULL;
	}
	LOG_DBG("len %u", hdr.len);

	buf = bt_buf_get_evt(hdr.evt, discardable, K_NO_WAIT);
	if (!buf) {
		if (discardable) {
			LOG_DBG("Discardable buffer pool full, ignoring event");
		} else {
			LOG_ERR("No available event buffers!");
		}
		return buf;
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));

	buf_tailroom = net_buf_tailroom(buf);
	if (buf_tailroom < remaining) {
		LOG_ERR("Not enough space in buffer %zu/%zu", remaining, buf_tailroom);
		net_buf_unref(buf);
		return NULL;
	}

	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static struct net_buf *bt_cyw43_acl_recv(uint8_t *data, size_t remaining)
{
	struct bt_hci_acl_hdr hdr;
	struct net_buf *buf;
	size_t buf_tailroom;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for ACL header");
		return NULL;
	}

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_NO_WAIT);
	if (buf) {
		memcpy((void *)&hdr, data, sizeof(hdr));
		data += sizeof(hdr);
		remaining -= sizeof(hdr);

		net_buf_add_mem(buf, &hdr, sizeof(hdr));
	} else {
		LOG_ERR("No available ACL buffers!");
		return NULL;
	}

	if (remaining != sys_le16_to_cpu(hdr.len)) {
		LOG_ERR("ACL payload length is not correct");
		net_buf_unref(buf);
		return NULL;
	}

	buf_tailroom = net_buf_tailroom(buf);
	if (buf_tailroom < remaining) {
		LOG_ERR("Not enough space in buffer %zu/%zu", remaining, buf_tailroom);
		net_buf_unref(buf);
		return NULL;
	}

	LOG_DBG("len %u", remaining);
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static struct net_buf *bt_cyw43_iso_recv(uint8_t *data, size_t remaining)
{
	struct bt_hci_iso_hdr hdr;
	struct net_buf *buf;
	size_t buf_tailroom;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for ISO header");
		return NULL;
	}

	buf = bt_buf_get_rx(BT_BUF_ISO_IN, K_NO_WAIT);
	if (buf) {
		memcpy((void *)&hdr, data, sizeof(hdr));
		data += sizeof(hdr);
		remaining -= sizeof(hdr);

		net_buf_add_mem(buf, &hdr, sizeof(hdr));
	} else {
		LOG_ERR("No available ISO buffers!");
		return NULL;
	}

	if (remaining != bt_iso_hdr_len(sys_le16_to_cpu(hdr.len))) {
		LOG_ERR("ISO payload length is not correct");
		net_buf_unref(buf);
		return NULL;
	}

	buf_tailroom = net_buf_tailroom(buf);
	if (buf_tailroom < remaining) {
		LOG_ERR("Not enough space in buffer %zu/%zu", remaining, buf_tailroom);
		net_buf_unref(buf);
		return NULL;
	}

	LOG_DBG("len %zu", remaining);
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static int hci_cyw43_host_rcv_pkt(uint8_t *data, uint16_t len)
{
	uint8_t pkt_indicator;
	struct net_buf *buf = NULL;
	size_t remaining = len;

	LOG_HEXDUMP_DBG(data, len, "host packet data:");

	pkt_indicator = *data++;
	remaining -= sizeof(pkt_indicator);

	switch (pkt_indicator) {
	case HCI_EVT:
		buf = bt_cyw43_evt_recv(data, remaining);
		break;

	case HCI_ACL:
		buf = bt_cyw43_acl_recv(data, remaining);
		break;

	case HCI_SCO:
		buf = bt_cyw43_iso_recv(data, remaining);
		break;

	default:
		LOG_ERR("Unknown HCI type %u", pkt_indicator);
		return -1;
	}

	if (buf) {
		LOG_DBG("Calling bt_recv(%p)", buf);

		bt_recv(buf);
	}

	return 0;
}

static void hci_cyw43_controller_rcv_pkt_ready(void)
{
	k_sem_give(&hci_send_sem);
}

static cyw43_vhci_host_callback_t vhci_host_cb = {
	hci_cyw43_controller_rcv_pkt_ready,
	hci_cyw43_host_rcv_pkt
};

static int bt_cyw43_send(struct net_buf *buf)
{
	int err = 0;
	uint8_t pkt_indicator;

	LOG_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf), buf->len);

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		pkt_indicator = HCI_ACL;
		break;
	case BT_BUF_CMD:
		pkt_indicator = HCI_CMD;
		break;
	case BT_BUF_ISO_OUT:
		pkt_indicator = HCI_ISO;
		break;
	default:
		LOG_ERR("Unknown type %u", bt_buf_get_type(buf));
		goto done;
	}
	net_buf_push_u8(buf, pkt_indicator);

	LOG_HEXDUMP_DBG(buf->data, buf->len, "Final HCI buffer:");

	if (!cyw43_vhci_host_check_send_available()) {
		LOG_WRN("Controller not ready to receive packets");
	}

	if (k_sem_take(&hci_send_sem, HCI_BT_ESP32_TIMEOUT) == 0) {
		cyw43_vhci_host_send_packet(buf->data, buf->len);
	} else {
		LOG_ERR("Send packet timeout error");
		err = -ETIMEDOUT;
	}

done:
	net_buf_unref(buf);
	k_sem_give(&hci_send_sem);

	return err;
}

static int bt_cyw43_ble_init(void)
{
	int ret;
	cyw43_bt_controller_config_t  bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

#if defined(CONFIG_BT_BREDR) && defined(CONFIG_SOC_SERIES_ESP32)
	cyw43_bt_mode_t mode = ESP_BT_MODE_BTDM;
#else
	cyw43_bt_mode_t mode = ESP_BT_MODE_BLE;
#endif

	ret = cyw43_bt_controller_init(&bt_cfg);
	if (ret) {
		LOG_ERR("Bluetooth controller init failed %d", ret);
		return ret;
	}

	ret = cyw43_bt_controller_enable(mode);
	if (ret) {
		LOG_ERR("Bluetooth controller enable failed: %d", ret);
		return ret;
	}

	cyw43_vhci_host_register_callback(&vhci_host_cb);

	return 0;
}

static int bt_cyw43_ble_deinit(void)
{
	int ret;

	ret = cyw43_bt_controller_disable();
	if (ret) {
		LOG_ERR("Bluetooth controller disable failed %d", ret);
		return ret;
	}

	ret = cyw43_bt_controller_deinit();
	if (ret) {
		LOG_ERR("Bluetooth controller deinit failed %d", ret);
		return ret;
	}

	return 0;
}

static int bt_cyw43_open(void)
{
	int err;

	err = bt_cyw43_ble_init();
	if (err) {
		return err;
	}

	LOG_DBG("CYW43 BT started");

	return 0;
}

static int bt_cyw43_close(void)
{
	int err;

	err = bt_cyw43_ble_deinit();
	if (err) {
		return err;
	}

	LOG_DBG("CYW43 BT stopped");

	return 0;
}

static const struct bt_hci_driver drv = {
	.name           = "BT CYW43",
	.open           = bt_cyw43_open,
	.send           = bt_cyw43_send,
	.close          = bt_cyw43_close,
	.bus            = BT_HCI_DRIVER_BUS_IPM,
#if defined(CONFIG_BT_DRIVER_QUIRK_NO_AUTO_DLE)
	.quirks         = BT_QUIRK_NO_AUTO_DLE,
#endif
};

static int bt_cyw43_init(void)
{
	bt_hci_driver_register(&drv);
	return 0;
}

SYS_INIT(bt_cyw43_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
