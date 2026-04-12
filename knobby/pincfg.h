#ifndef _PINCFG_H_
#define _PINCFG_H_

// Board selection: define exactly one
#define BOARD_JC3636K518
// #define BOARD_JC3636K718

#if defined(BOARD_JC3636K718)

#define TFT_BLK 21
#define TFT_RST 17
#define TFT_CS 12
#define TFT_SCK 11
#define TFT_SDA0 13
#define TFT_SDA1 14
#define TFT_SDA2 15
#define TFT_SDA3 16

#define TOUCH_PIN_NUM_I2C_SCL 10
#define TOUCH_PIN_NUM_I2C_SDA 9
#define TOUCH_PIN_NUM_INT 7
#define TOUCH_PIN_NUM_RST 8

#define ROTARY_ENC_PIN_A 2
#define ROTARY_ENC_PIN_B 1

#define BATTERY_ADC_PIN_NUM 6

#define BTN_PIN 0

#define PIN_LCD_TE 18

#define AUDIO_I2S_BCK_IO 3
#define AUDIO_I2S_WS_IO 45
#define AUDIO_I2S_DO_IO 42
#define AUDIO_MUTE_PIN 46

#define MIC_I2S_SCK 5
#define MIC_I2S_SD 4

#elif defined(BOARD_JC3636K518)

#define TFT_BLK 47
#define TFT_RST 21
#define TFT_CS 14
#define TFT_SCK 13
#define TFT_SDA0 15
#define TFT_SDA1 16
#define TFT_SDA2 17
#define TFT_SDA3 18

#define TOUCH_PIN_NUM_I2C_SCL 12
#define TOUCH_PIN_NUM_I2C_SDA 11
#define TOUCH_PIN_NUM_INT 9
#define TOUCH_PIN_NUM_RST 10

#define ROTARY_ENC_PIN_A 8
#define ROTARY_ENC_PIN_B 7

#define BATTERY_ADC_PIN_NUM 1

#define BTN_PIN 0

#define AUDIO_I2S_MCK_IO -1
#define AUDIO_I2S_BCK_IO 18
#define AUDIO_I2S_WS_IO 16
#define AUDIO_I2S_DO_IO 17
#define AUDIO_MUTE_PIN 48

#define MIC_I2S_WS 45
#define MIC_I2S_SD 46
#define MIC_I2S_SCK 42

#else
#error "No board selected in pincfg.h — define BOARD_JC3636K518 or BOARD_JC3636K718"
#endif

#endif
