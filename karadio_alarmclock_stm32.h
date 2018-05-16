

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MapleFreeRTOS900.h>
#include <time.h>
#include <EEPROM.h>

/* TIMERS */
// Hardware timer 2 of the STM32
// used for all timers in this software
#ifndef F_INTERRUPTS
#define F_INTERRUPTS     15000   // interrupts per second, min: 10000, max: 20000, typ: 15000
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
#define NEWTITLE1   0
#define NEWTITLE2  1
#define NEWSTATION 2
#define NEWTIME 3
#define TOGGLELIGHT 4
#define NEWVOLUME 5
#define NEWVOLUMEDONE 6
#define NEWALARM 7
#define NEWIP 8

// 
#define FREQ_TASK_SCREEN 200

// FREERTOS modifiers
#undef INCLUDE_vTaskSuspend 
#define INCLUDE_vTaskSuspend 1

#undef configUSE_PREEMPTION
#define configUSE_PREEMPTION 1
/*
#undef configTICK_RATE_HZ
#define configTICK_RATE_HZ 100
*/
    // configTICK_RATE_HZ = 1 Hz now. It should produce weird behaviour !
    // no weir behaviour ! Is it because my tasks are fast enough, hence my UC is quite low ?
    // gotta profile the tasks! To know their exec time.

// macro to check UART being used for button:
#define UART_USED (UART_using_flag_command[0] || UART_using_flag_command[1] || UART_using_flag_command[2] || UART_using_flag_command[3] || UART_using_flag_command [4] || UART_using_flag_command[5])

// macros to manipulate the alarm
#define READ_ALARM_HOURS   ((alarm & 0b111111111111) / 60)
#define READ_ALARM_MINUTES ((alarm & 0b111111111111) % 60)
#define READ_ALARM_STATE   ((alarm & 1<<15) >> 15)
#define FLIP_ALARM_STATE   alarm = (~(alarm & 1<<15) & 1<<15) | (alarm & 0b111111111111)
#define CHANGE_ALARM(h,m)    ((alarm & 1<<15) & 1<<15) | \
                              (((((alarm & 0b111111111111) / 60 + h + (((alarm & 0b111111111111) / 60 + h) < 0 ? 24 : 0)) %24) * 60) \
                              + ((((alarm & 0b111111111111) % 60) + m + ((((alarm & 0b111111111111) % 60) + m) < 60 ? 60 : 0))%60))
