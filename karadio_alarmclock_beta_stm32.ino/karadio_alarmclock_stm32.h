#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MapleFreeRTOS.h>
#include <time.h>
#include <EEPROM.h>

//#include <TM1637Display.h>

/* TIMERS */
// Hardware timer 2 of the STM32
// used for all timers in this software
#ifndef F_INTERRUPTS
#define F_INTERRUPTS     10000   // interrupts per second, min: 10000, max: 20000, typ: 15000
#endif

/*  DEFINES   */

// your timezone offset
#define TZO 1

// Serial speed
#define BAUD 115200

// the LCD screen size
#define LCD_WIDTH 20
#define BUFLEN  256
#define LINES  4

#undef SERIAL_RX_BUFFER_SIZE
#define SERIAL_RX_BUFFER_SIZE 256
#define SERIALX Serial3
#define PIN_LED LED_BUILTIN

// adaptative polling times (ms) for buttons, and macro for reading values
#define FAST_POLLING 2
#define SLOW_POLLING 63
#define READPORTA(n) (GPIOA->regs->IDR & (1 << (6+n)))

// FLAG_BUTTONS
#define PLAYPAUSE 0
#define VOLPLUS 1
#define VOLMINUS 2
#define CHANPLUS 3
#define CHANMINUS 4
#define MODE 5

// FLAG_SCREEN
#define NEWTITLE   0
#define NEWSTATION 1
#define NEWTIME 2
#define TOGGLELIGHT 3
#define NEWVOLUME 4
#define NEWALARM 5

// 
#define FREQ_TASK_SCREEN 200

// macros to manipulate the alarm
#define READ_ALARM_HOURS   ((alarm & 0b111111111111) / 60)
#define READ_ALARM_MINUTES ((alarm & 0b111111111111) % 60)
#define READ_ALARM_STATE   ((alarm & 1<<15) >> 15)
#define FLIP_ALARM_STATE   alarm = (~(alarm & 1<<15) & 1<<15) | (alarm & 0b111111111111)
#define CHANGE_ALARM(h,m)    ((alarm & 1<<15) & 1<<15) | \
                              (((((alarm & 0b111111111111) / 60 + h + (((alarm & 0b111111111111) / 60 + h) < 0 ? 24 : 0)) %24) * 60) \
                              + ((((alarm & 0b111111111111) % 60) + m + ((((alarm & 0b111111111111) % 60) + m) < 60 ? 60 : 0))%60))
