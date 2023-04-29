Health Thermometer Sensor -- Central
##############################################

Overview
********
Long Range version of Central HT.

Requirements
************

Debug Output
**********
```
*** Booting Zephyr OS build zephyr-v3.3.0-1787-g286f10323ce1 ***
[00:00:02.332,153] <inf> bt_hci_core: hci_vs_init: HW Platform: Nordic Semiconductor (0x0002)
[00:00:02.332,183] <inf> bt_hci_core: hci_vs_init: HW Variant: nRF52x (0x0002)
[00:00:02.332,214] <inf> bt_hci_core: hci_vs_init: Firmware: Standard Bluetooth controller (0x00) Version 3.3 Build 99
[00:00:02.333,068] <inf> bt_hci_core: bt_dev_show_info: Identity: F6:66:C4:3B:31:A0 (random)
[00:00:02.333,099] <inf> bt_hci_core: bt_dev_show_info: HCI: version 5.4 (0x0d) revision 0x0000, manufacturer 0x05f1
[00:00:02.333,129] <inf> bt_hci_core: bt_dev_show_info: LMP: version 5.4 (0x0d) subver 0xffff
[00:00:02.333,129] <inf> main: main: Bluetooth initialized as Central
[00:00:02.333,526] <inf> main: main: Scanning successfully started
[00:00:18.069,305] <inf> main: connected: Connected: EC:4C:11:40:DC:A0 (random)
[00:00:18.121,276] <inf> main: discover_func: [ATTRIBUTE] handle 25
[00:00:18.220,611] <inf> main: discover_func: [ATTRIBUTE] handle 26
[00:00:18.420,562] <inf> main: discover_func: [ATTRIBUTE] handle 28
[00:00:18.420,623] <inf> main: discover_func: [SUBSCRIBED]
[00:00:18.970,489] <inf> main: notify_func: Temperature 21.5C.
[00:00:19.970,489] <inf> main: notify_func: Temperature 21.5C.
[00:00:20.970,520] <inf> main: notify_func: Temperature 21.5C.
[00:00:21.970,489] <inf> main: notify_func: Temperature 21.75C.
[00:00:22.970,520] <inf> main: notify_func: Temperature 21.5C.
``
