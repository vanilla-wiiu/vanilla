extern "C" {
#include "FreeRTOS.h"
}

#include <Arduino.h>
#include <wifi_conf.h>
#include <wifi_drv.h>

#include "ethernet_backend.h"
#include "usb_network.h"
#include "vanilla_pipe.h"
#include "src/diagnostics/wifi_diagnostics.h"
#include "src/diagnostics/serial_monitor.h"

void setup()
{
    // Start serial and wait for it to begin
    Serial.begin(115200);

    const unsigned long started = millis();
    while (!Serial && millis() - started < 2000UL) {
        delay(10);
    }

    Serial.println("vanilla-pipe active");
    Serial.print("diagnostics=");
    Serial.println(VANILLA_DIAGNOSTICS);

    Serial.println("Starting Wi-Fi and networking...");
    WiFiDrv::wifiDriverInit();
#if VANILLA_DIAGNOSTICS
    wifi_diagnostics::begin();
#endif

    // TODO: Make this region specific for legal reasons
    if (wifi_set_country(RTW_COUNTRY_US) != RTW_SUCCESS) {
        Serial.println("Failed to set Wi-Fi country");
    }

    if (wifi_set_autoreconnect(RTW_AUTORECONNECT_FINITE) != RTW_SUCCESS) {
        Serial.println("Failed to configure Wi-Fi autoreconnect");
    }

    // Wii U needs prompt reception even between bursts so keep
    // power saving disabled at all times
    if (wifi_disable_powersave() != RTW_SUCCESS) {
        Serial.println("Failed to disable Wi-Fi power saving");
    }

    if (!ethernet::begin()) {
        Serial.println("Failed to start Ethernet");
        return;
    }

    Serial.println("Starting USB...");

    if (!usb_network_begin()) {
        Serial.println("Failed to start USB");
        return;
    }

    Serial.println("Started USB - waiting for host");
}

void loop()
{
    serial_monitor::service();
    vanilla_pipe::service();
    delay(1);
}

