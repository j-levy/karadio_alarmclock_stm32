#KaRadio Addon for STM32 with LCD and alarm clock features

### Description

This is the code for a KaRadio addon using a STM32F103C8T6 microcontroller, also known as "Blue Pill". It runs thanks to the STM32duino project.

My code has taken as base an original KaRadio addon using STM32 : https://github.com/karawin/karadio-addons/tree/master/karadioUCSTM32

See more about KaRadio :
https://github.com/karawin/Ka-Radio

See more about my project on my blog : https://electronics-kitchen-table.blogspot.com

### Features
- 6 buttons to control the KaRadio and set alarm clock with common features : Volume +/-, Channel +/-, Play/Pause, and Mode button to set the alarm clock
- A 20x4 LCD screen to display all relevant information
- (Coming soon) alarm clock kept in Flash (EEPROM emulation) between reboots
- (Coming soon) current clock displayed on a separate, 4-digits display
- (Maybe one day) Non-ASCII characters fallback mode (currently, non-ASCII characters are replaced by bit shifting and truncate, creating funny symbols).

### Wiring

Coming (some day !)
