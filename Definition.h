#pragma once

#ifdef MX1C_NODEMCU_IO_02G

#define SWITCH

#define BUTTON_PINS { 0 }
#define RELAY_PINS { 2 }
#define LED_PIN 16

#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

#define LED_ON      LOW
#define LED_OFF     HIGH

#define CHANNEL_COUNT   1

#endif

#ifdef MX3C_KEEPER_TYWE3S_IO_5GCDE40

#define SWITCH

#define BUTTON_PINS { 5, 16, 12 }
#define RELAY_PINS { 13, 14, 4 }
#define LED_PIN 0

#define RELAY_ON    HIGH
#define RELAY_OFF   LOW

#define LED_ON      LOW
#define LED_OFF     HIGH

#define CHANNEL_COUNT   3

#endif

#ifdef MX2C_SESOO_TYWE3S_IO_CED4G

#define SWITCH

//#define MODEL "JS2C"
//#define HARDWARE "JS2C_SESOO_TYWE3S_IO_CED4G"

#define BUTTON_PINS { 13 }
#define RELAY_PINS { 12 }
#define IRRECT_PIN 5
#define IRSEND_PIN 14
#define LED_PIN 4

#define RELAY_ON    HIGH
#define RELAY_OFF   LOW

#define LED_ON      LOW
#define LED_OFF     HIGH
//#define BAUND_RATE 115200
#define CHANNEL_COUNT 1
//#define SCHEDULE_COUNT 16
#endif


#ifdef MX3C_SESOO_TYWE3S_IO_C5EDF4G

#define SWITCH
//#define GENERIC_IO
//#define MODEL "JS3C"
//#define HARDWARE "JS3C_SESOO_TYWE3S_IO_C5EDF4G"
#define BUTTON_PINS { 12, 5, 14 }
#define RELAY_PINS { 13, 15, 4 }

#define LED_PIN 16

#define RELAY_ON    HIGH
#define RELAY_OFF   LOW

#define LED_ON      LOW
#define LED_OFF     HIGH
//#define BAUND_RATE 115200
#define CHANNEL_COUNT 3
//#define SCHEDULE_COUNT 16
#endif
