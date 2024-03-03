# LongRanger - Zephyr + Bluetooth LE Coded-PHY 

## Overview
This poject's goal is to provide matching Bluetooth Code_PHY (Long Range) apps.  
In particular, a Health Thermometer service, implemented as a Central & Peripheral pair.  
This project's firmware is being developed on Zephyr Version 3.6.99, but should run on other recent version of Zephyr.

## Requirements
This project was developed and tested using two nRF52840 eval boards (PCA10056).
The Zephyr + ESP32-C3 as peripheral has been successfully tested by others.

## How to Build
There are two easy methods to build the firmware.

### 1) CMake Method
**Central build**
1) cd to your ./LongRanger/central directory
2) run "./configure.sh"
3) cd to build directory
4) make -j4
5) make flash

**Periperhal build**  
1) cd to your ./LongRanger/peripheral directory
2) run "./configure.sh"
3) cd to build directory
4) make -j4## How to Build
5) make flash

### 2) West Method
**Central build**
1) cd to your ./LongRanger/central directory
2) rm -rf build
3) west build -b nrf52840dk_nrf52840
4) west flash

**Periperhal build**
1) cd to your ./LongRanger/peripheral directory
2) rm -rf build
3) west build -b nrf52840dk_nrf52840
4) west flash
5) 

## How to Operate
This simplest way to observe these two apps after flashing them onto seperate nRF52840 boards is to open two serial terminals.  
Minicom or Putty are suggested.  
  
**Central-side Terminal Log**
```
*** Booting Zephyr OS build v3.6.0-159-g4a72cb16cfd5 ***
[00008204] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00008205] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
[00008206] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 3.6 Buil9
[00008230] <inf> bt_hci_core: Identity: EC:4C:11:40:DC:A0 (random)
[00008231] <inf> bt_hci_core: HCI: version 5.4 (0x0d) revision 0x0000, manufacturer 0x05f1
[00008232] <inf> bt_hci_core: LMP: version 5.4 (0x0d) subver 0xffff
[00008234] <inf> main: Bluetooth initialized as Central scanning for "LongRanger"
[00008248] <inf> main: Scan start successfully
[00008248] <inf> main: Scanning successfully started
[00367164] <inf> main: [DEVICE]: F9:82:63:BC:F5:23 (random), AD evt type 5, AD data len 23, R7
[00367165] <inf> main: [AdvData]: type 1, data_len 1
[00367165] <inf> main: [AdvData]: type 3, data_len 6
[00367165] <inf> main: Stop scanning
[00367171] <inf> main: Create connection
[00370939] <inf> main: Connected: F9:82:63:BC:F5:23 (random)
[00372722] <inf> main: [ATTRIBUTE] handle 26
[00375949] <inf> main: [ATTRIBUTE] handle 27
[00380858] <inf> main: [ATTRIBUTE] handle 29
[00380860] <inf> main: [SUBSCRIBED]
[00397249] <inf> main: Temperature 25C -- RSSI -50dB
[00430016] <inf> main: Temperature 25C -- RSSI -45dB
[00462785] <inf> main: Temperature 25C -- RSSI -45dB
[00495552] <inf> main: Temperature 25.25C -- RSSI -46dB
[00528321] <inf> main: Temperature 25C -- RSSI -50dB
[00561090] <inf> main: Temperature 25C -- RSSI -49dB
[00593857] <inf> main: Temperature 25C -- RSSI -46dB
[00626625] <inf> main: Temperature 25.25C -- RSSI -46dB
[00659393] <inf> main: Temperature 25.25C -- RSSI -46dB
[00692161] <inf> main: Temperature 25C -- RSSI -48dB
[00724929] <inf> main: Temperature 25C -- RSSI -48dB
```
**Peripheral-side Terminal Log**
```
*** Booting Zephyr OS build v3.6.0-159-g4a72cb16cfd5 ***
[00008471] <inf> bt_hci_core: HW Platform: Nordic Semiconductor (0x0002)
[00008473] <inf> bt_hci_core: HW Variant: nRF52x (0x0002)
[00008473] <inf> bt_hci_core: Firmware: Standard Bluetooth controller (0x00) Version 3.6 Buil9
[00008496] <inf> bt_hci_core: Identity: F9:82:63:BC:F5:23 (random)
[00008497] <inf> bt_hci_core: HCI: version 5.4 (0x0d) revision 0x0000, manufacturer 0x05f1
[00008498] <inf> bt_hci_core: LMP: version 5.4 (0x0d) subver 0xffff
[00008507] <inf> main: Created extended advertising
[00008513] <inf> main: Bluetooth initialized as Peripheral advertising as "LongRanger"
[00008525] <inf> main: Advertiser set started
[00019807] <inf> main: Connected
[00019809] <inf> main: Connected: X, tx_phy 4, rx_phy 4
[00041312] <inf> hts: temperature is 25C -- RSSI -45dB
[00074105] <inf> hts: temperature is 25C -- RSSI -45dB
[00106901] <inf> hts: temperature is 25C -- RSSI -48dB
[00139696] <inf> hts: temperature is 25C -- RSSI -49dB
[00172492] <inf> hts: temperature is 25C -- RSSI -45dB
[00205288] <inf> hts: temperature is 25C -- RSSI -46dB
[00238084] <inf> hts: temperature is 25C -- RSSI -50dB
[00270879] <inf> hts: temperature is 25C -- RSSI -47dB
[00303675] <inf> hts: temperature is 25C -- RSSI -44dB
[00336470] <inf> hts: temperature is 25C -- RSSI -47dB
[00369266] <inf> hts: temperature is 25C -- RSSI -49dB
[00402061] <inf> hts: temperature is 25C -- RSSI -50dB
[00434857] <inf> hts: temperature is 25C -- RSSI -45dB
[00467652] <inf> hts: temperature is 25C -- RSSI -44dB
[00500448] <inf> hts: temperature is 25C -- RSSI -48dB
```



## How to Sniff Coded-PHY Packets with Wireshark
For those with a spare (third) nRF52840DK (PCA10056) board or a nRF52840-Dongle (PCA10059)  
you can try Wireshark with Nordic's [add-on](https://infocenter.nordicsemi.com/topic/ug_sniffer_ble/UG/). (read there for details)  

Wireshark will allow you to sniff the Coded-PHY traffic between the Central and Peripheral apps.

![here](https://github.com/foldedtoad/LongRanger/blob/master/images/wireshark_01.png)

![here](https://github.com/foldedtoad/LongRanger/blob/master/images/wireshark_02.png)   

![here](https://github.com/foldedtoad/LongRanger/blob/master/images/wireshark_03.png)  
