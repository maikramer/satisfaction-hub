//
// Created by maikeu on 14/08/2019.
//

#ifndef TOMADA_SMART_CONDO_CONFIG_H
#define TOMADA_SMART_CONDO_CONFIG_H

#include <driver/gpio.h>
#include <soc/adc_periph.h>
#include <hal/adc_types.h>
//Config Data
// Servico publico
#define PUBLIC_SERVICE_UUID                     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define PUBLIC_CHARACTERISTIC_RX_UUID           "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define PUBLIC_CHARACTERISTIC_TX_UUID           "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Base BLE Dinamica
#define SERVICE_UUID_BASE                       "4fafc202-1fb5-459e-8fcc-c5c9c33191"
#define CHARACTERISTIC_UUID_STATE_BASE          "6E400006-B5A3-F393-E0A9-E50E24DCCA"

//Pin Config
#define SDMISO              GPIO_NUM_2
#define SDCS0               GPIO_NUM_13
#define SDCLK               GPIO_NUM_14
#define SDMOSI              GPIO_NUM_15
#define SIGNAL_EMULATOR1    GPIO_NUM_16

//#define USE_RELE_LED
#define LED                 GPIO_NUM_2

#define RELE_TOMADA1        GPIO_NUM_23
#define RELE_TOMADA2        GPIO_NUM_25
#define RELE_TOMADA3        GPIO_NUM_26
#define RELE_TOMADA4        GPIO_NUM_27

#define WATT1               ADC_CHANNEL_5
#define WATT2               ADC_CHANNEL_6
#define WATT3               ADC_CHANNEL_7
#define WATT4               ADC_CHANNEL_0

//Config
#define MAX_OPEN_FILES 5
#define USER_MANAGEMENT_ENABLED
//#define USE_SDCARD
#define DEVICE_NAME "Tomada Smart Condo"
//#define SIMULATOR

#endif //TOMADA_SMART_CONDO_CONFIG_H
