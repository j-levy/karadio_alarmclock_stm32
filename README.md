# KaRadio Addon for STM32 with LCD and alarm clock features

### Description

This is the code for a KaRadio addon using a STM32F103C8T6 microcontroller, also known as "Blue Pill". It runs thanks to the STM32duino project.

My code has taken as base an original KaRadio addon using STM32 : https://github.com/karawin/karadio-addons/tree/master/karadioUCSTM32

See more about KaRadio :
https://github.com/karawin/Ka-Radio

See more about my project on my blog : https://electronics-kitchen-table.blogspot.com

### Features
- 6 buttons to control the KaRadio and set alarm clock with common features : Volume +/-, Channel +/-, Play/Pause, and Mode button to set the alarm clock
- A 20x4 LCD screen to display all relevant information
- alarm clock kept in Flash (EEPROM emulation) between reboots
- Non-ASCII characters fallback mode for Asian LCD-i2C screen (currently, non-ASCII characters are replaced by bit shifting and truncate, creating funny symbols with the asian version of the LCD !!)
- (Maybe one day) current clock displayed on a separate, 4-digits display


### Wiring

#### LCD
SDA  <---> PB7
SCL  <---> PB6

#### Buttons

PA15 <---> Select Mode

Mode 1:
PA6 <---> PLAY / PAUSE
PA7 <---> Volume +
PA8 <---> Volume -
PA9 <---> Channel +
PA10 <---> Channel -

Mode 2:
PA6 <---> Enable / Disable alarm clock
PA7 <---> Minutes +
PA8 <---> Minutes -
PA9 <---> Hours +
PA10 <---> Hours -

#### UART

PB11 <---> TX (ESP8266)
PB10 <---> RX (ESP8266)
