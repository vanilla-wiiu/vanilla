# vanilla-pipe for the RTL8720DF

## Introduction

### What is this?

While many devices are capable of connecting to the Wii U on their own, many more do not, either because they lack a 5GHz Wi-Fi radio, or they lack the software/firmware versatility to work with Nintendo's slightly customized security protocol.

While it is possible to run `vanilla-pipe` (the component responsible for making the connection to the Wii U) on a separate computer and route the data over a local network for these devices, this setup is cumbersome since it requires you to configure a whole separate computer and either leave it running indefinitely - wasting energy - or manually power it on and off every time you want to use Vanilla with your Wii U. This can also introduce latency and dropped frames depending on the network hardware involved and strength of the connection.

As such, there came a need for small embedded hardware that could plug directly into an otherwise incompatible device (so there would be no latency), be capable of connecting to the Wii U (for devices that lacked a 5GHz radio), and require no drivers (for locked down operating systems like Android, iOS, Windows, and macOS).

### RTL8720DF

The RTL8720DF board is a 5GHz Wi-Fi enabled microcontroller with a high-speed USB 2.0 data bus. This combination makes it one of the few microcontrollers even capable of routing network traffic between a Wii U and Vanilla (many microcontrollers either lack 5GHz altogether, or only provide relatively low speed SPI or SDIO buses that can't transfer data to the host fast enough).

### How does it work?

This folder contains a feature-complete `vanilla-pipe` implementation with some tricks to make it as functional and compatible as possible.

The RTL8720DF has fully featured 5GHz capability, including WPS (which the Wii U uses to "sync" gamepads), however out of the box it (naturally) has no support for the Wii U's custom security protocol. Unfortunately, the Wi-Fi firmware is closed-source, however it turns out much of it is derived from the open source `wpa_supplicant`, making it relatively easy to make changes and re-compile them into the firmware. Realtek engineers provided additional modifications to increase the maximum incoming frame size from 1,500 to ~2,000 (to accommodate the Wii U's large video frames).

For connecting to a host without requiring drivers, this implementation emulates a USB Ethernet adapter. Specifically the ASIX AX88772, which has wide and longstanding driver support on Windows, Linux, macOS, iOS, and Android (other more generic USB Ethernet standards like RNDIS or CDC-NCM are either not as widely supported or have only become supported relatively recently).

## Building

RTL8720DF boards can be purchased on [AliExpress](https://www.aliexpress.us/item/3256804923377828.html). **Make sure it is the *DF* variant, other variants like the RTL8720DN omit the necessary USB 2.0 bus.**

From there, the code in here can be compiled and flashed to the board using the Arduino IDE.

A USB 2.0 cable must be soldered manually to the board. Additionally, a resistor must be soldered to enable USB mode.

