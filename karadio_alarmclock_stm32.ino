
#include "karadio_alarmclock_stm32.h"

HardwareTimer timer(2);
LiquidCrystal_I2C lcd(0x3f, LCD_WIDTH, LINES);

bool state = false;               // start stop on Ok ir key
char line[SERIAL_RX_BUFFER_SIZE]; // receive buffer
char station[BUFLEN];             //received station
char title[BUFLEN];               // received title
char nameset[BUFLEN] = {"0"};
; // the local name of the station
uint8_t volume;
uint8_t dispvolume;

unsigned char hours;
unsigned char minutes;
unsigned char seconds;

bool flag_screen[10] = {false};

bool isLCDused = false;
bool isPlaying = true;
bool isMode2ON = false;

unsigned ledcount = 0; // led counter
char volumeCounter = -1;

struct tm *dt;

/*-----( Declare objects )-----*/
struct InfoScroll
{
  char line; // line number, 0-3
  char *s_connect;
  char fw;
  int waiting;
};

InfoScroll Song = {0, (char *)"INIT...", 2, 900};

//bool flag_command[6] = {0}; // volatile is for interrupts.
//bool UART_using_flag_command[6] = {0}; // volatile is for interrupts.

struct UART_cmd{
   uint8_t flag;
   uint8_t waiting;
   int16_t timestamp;
   uint8_t timeout;
};

UART_cmd command = {0,0,-1,2};

uint16 alarm;
bool isAskingTime;
bool isAskingIP;
bool isIPInvalid;
bool isTimeInvalid;

// ip
char oip[20] = "___.___.___.___";

// EEPROM configuration
uint16 AddressWrite = 0x00; // write at the beginning of the page.


// a handle to enable/disable scrolling task
TaskHandle_t xHandlePrintScroll = NULL;
TaskHandle_t xHandleMain = NULL;

#define DEBUG


// init timer 2 for irmp led screen etc
void timer2_init()
{
  timer.pause();
  //    timer.setPrescaleFactor( ((F_CPU / F_INTERRUPTS)/8) - 1);
  //    timer.setOverflow(7);
  timer.setPrescaleFactor(((F_CPU / F_INTERRUPTS) / 10));
  timer.setOverflow(9);
  timer.setChannelMode(TIMER_CH1, TIMER_OUTPUT_COMPARE);
  timer.setCompare(TIMER_CH1, 0); // Interrupt  after each update
  timer.attachInterrupt(TIMER_CH1, TIM2_IRQHandler);
  // Refresh the timer's count, prescale, and overflow
  timer.refresh();
  // Start the timer counting
  timer.resume();
}

void TIM2_IRQHandler() // Timer2 Interrupt Handler
{
  
  if (++ledcount == (F_INTERRUPTS)) //1 sec
  {

    ledcount = 0;
    
    // Count the time (+1 second).
    localTime();

    if (TIMESTAMP_CUR > command.timestamp + command.timeout)
    {
      command.timestamp = -1;
      command.waiting = 0; // do not wait for anything anymore.
    }
      
    
    //Serial.println(seconds);
  }
  
}

void localTime()
{
  // Using a timer to get triggered. Maybe one day I'll use the RTClock... but is it that necessary ?
  //Serial.println(seconds);
  seconds++;
  flag_screen[NEWTIME]=true; // this makes the ":" blink
  
  if (volumeCounter>=0)
	  volumeCounter--;
  if (volumeCounter==0)
	  flag_screen[NEWVOLUMEDONE]=true;
	

  
  if (seconds >= 60)
  {
    seconds = 0;
    minutes++;
    // every minute, check if time is invalid, to ask for NTP.
    // Anyway, NTP cannot process a request every second !!
      if (isTimeInvalid)
    isAskingTime = true;
  
    if (isIPInvalid)
      isAskingIP = true;
      
    if (minutes >= 60)
    {
      minutes = 0;
      hours++;
      // ask for NTP time every hour, to avoid derivating.
      isTimeInvalid = true;

      if (hours >= 24)
      {
        hours = 0;

      } //endif (hours)
    }   // endif (minutes)
    // check alarm clock.

    if (hours == READ_ALARM_HOURS && minutes == READ_ALARM_MINUTES && seconds == 0 && !isPlaying && READ_ALARM_STATE == 1)
      SET_BIT(command.flag, PLAYPAUSE);
  } // endif(seconds)
}


/// TODO : something is probably making the screen crash here. 
/// When crashing, only the title is not yet displayed. Then, my suspect is the title scroller task.
static void printScrollRTOSTask(void *pvParameters)
{
  while (1)
  {
    vTaskDelay(10);
    bool finished = false;
    int i = 0;
    int len = strlen(Song.s_connect);
    String p_connect = String(Song.s_connect);

    while (!finished)
    {
      // Get the song title, start at position 'i', truncate it if needed
      String p_message = p_connect.substring(i, (i + LCD_WIDTH < len ? i + LCD_WIDTH : len));

      // To avoid trailing at the right of the screen, fill with blanks
      for (int k = len - i; k < LCD_WIDTH; k++)
        p_message += " ";

      while (isLCDused)
        ; // wait for the LCD to be available.

      // Write that stuff
      isLCDused = true;
      vTaskSuspend(xHandleMain);
      lcd.setCursor(0, Song.line);
      lcd.print(p_message);
      vTaskResume(xHandleMain);
      isLCDused = false;

      // Wait a bit if it's the first line...
      if (i == 0)
        vTaskDelay(2 * Song.waiting);

      // Wait a bit and flag 'finished' if it's the last time
      if (i + LCD_WIDTH >= len)
      {
        finished = true;
        vTaskDelay(2 * Song.waiting);
      }
      // Go forward in the display !
      i += Song.fw;
      vTaskDelay(Song.waiting);

      // If we have to update the title, just flag 'finished' so that the new title is taken into account at the beginning.
      if (flag_screen[NEWTITLE2])
      {
        finished = true;
        flag_screen[NEWTITLE2]=false; // remember, we have to update 2 different locations. First is, when we CLEAN UP the whole line, Second is here.
        /*
        UART_using_flag_command[PLAYPAUSE] = false;
        UART_using_flag_command[CHANPLUS] = false;
        UART_using_flag_command[CHANMINUS] = false;
        */
      }
    } // endwhile(!finished)
  }   // endwhile(1)
}

//-------------------------------------------------------
// Main task used for display the screen
static void mainTask(void *pvParameters)
{
  //Serial.println(F("mainTask"));
  bool alt = true;
  while (1)
  {
    /// HERE ADD SCREEN DRAWING FOR TITLE, STATION, TIME.
    int i = 0;
    if (!isLCDused) // Critical section ahead : takes over the LCD. "if" so that it can be skipped if anything else happens (not locked)
    {
      
      isLCDused = true;
      

      // Clean up the whole line
      if (flag_screen[NEWTITLE1])
      {
        lcd.setCursor(0, 0);
        lcd.print("                    ");
        flag_screen[NEWTITLE1]=false;
        flag_screen[NEWTITLE2]=true;        
      }
      // Clean up the whole line
      if (flag_screen[NEWSTATION])
      {
        lcd.setCursor(0, 3);
        lcd.print("                    ");
        lcd.setCursor(0, 3);
        lcd.print(station); // display on the 4th line of my 20x4 LCD the station name (ICY0)
        flag_screen[NEWSTATION]=false;
      }

      // update clock on screen. This shouldn't stay as is. Some day, I will simply display the alarm clock on this line.
      if (flag_screen[NEWTIME])
      {
        lcd.setCursor(15, 3);
        // the 'alt' thing is to alternate the blinking for ':'
        if (alt)
        {
          lcd.print((hours <= 9 ? "0" : "") + String(hours) + ":" + (minutes <= 9 ? "0" : "") + String(minutes));
          alt = !alt;
        }
        else
        {
          lcd.print((hours <= 9 ? "0" : "") + String(hours) + " " + (minutes <= 9 ? "0" : "") + String(minutes));
          alt = !alt;
        }
        flag_screen[NEWTIME]=false;
      }
      
      if (flag_screen[NEWVOLUMEDONE])
      {
        lcd.setCursor(10, 2);
        lcd.print("          ");
        flag_screen[NEWVOLUMEDONE]=false;
      }
      else if (flag_screen[NEWVOLUME])
      {
		    
		    lcd.setCursor(10, 2);
        lcd.print("VOL: ");
        lcd.print(dispvolume);
        lcd.print("%");
        
        // lcd.print(String((round(((float)volume) * 100 / 254) < 10 ? "  " : ((round(((float)volume) * 100 / 254) < 100 ? " " : "")))) + String("VOL : ") + String(round(((float)volume) * 100 / 254)) + "%");
        volumeCounter = 2; //seconds to be displayed. Actually, will be between 2 and 3 seconds.
        flag_screen[NEWVOLUME]=false;
      }

      if (flag_screen[NEWALARM])
      {
        
        lcd.setCursor(0, 2);
        lcd.print((isMode2ON ? "     >" : "") \
					+ String((READ_ALARM_HOURS <= 9 ? "0" : "")) + String(READ_ALARM_HOURS) \
					+ ":" \
					+ String((READ_ALARM_MINUTES <= 9 ? "0" : "")) + String(READ_ALARM_MINUTES) \
					+ (READ_ALARM_STATE == 1 ? " ON " : " OFF") \
					+ (!isMode2ON ? "      " : ""));
        flag_screen[NEWALARM]=false;
      }
      
      if (flag_screen[NEWIP])
      {
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        lcd.setCursor(0, 1);
        lcd.print(oip);
        flag_screen[NEWIP]=false;
	    }

      if (flag_screen[TOGGLELIGHT])
      {
        if (isMode2ON)
        {
          lcd.backlight();
        } else {
          if (isPlaying)
            lcd.backlight();
          else
            lcd.noBacklight();
        }
      flag_screen[TOGGLELIGHT] = false;
      }
      
      isLCDused = false;
    }
    vTaskDelay(FREQ_TASK_SCREEN); // update the screen @5Hz (should be fast enough !)
  }
}

//-------------------------------------------------------
// Uart task: receive the karadio log and parse it.
static void uartTask(void *pvParameters)
{

  Serial.println(F("uartTask"));
  SERIALX.print(F("\r"));         // cleaner
  SERIALX.print(F("cli.info\r")); // Synchronise the current state
  vTaskDelay(1000);
  while (1)
  {
    // Mode 1 - control the radio.
    if (command.flag)
    {
      if (!isMode2ON && !command.waiting && READ_BIT(command.flag, MODE) == 0)
      {
        char cmd[20];
      
        if (READ_BIT(command.flag, PLAYPAUSE))
        {
          strcpy(cmd,(!isPlaying ? "cli.start\r" : "cli.stop\r"));
          isPlaying = !isPlaying;
          CLEAR_BIT(command.flag, PLAYPAUSE);
          SET_BIT(command.waiting, PLAYPAUSE);
        }
        else if (READ_BIT(command.flag, CHANPLUS))
        {
          strcpy(cmd, "cli.prev\r");
          CLEAR_BIT(command.flag, CHANPLUS);
          SET_BIT(command.waiting, CHANPLUS);
        }
        else if (READ_BIT(command.flag, CHANMINUS))
        {
          strcpy(cmd, "cli.next\r");
          CLEAR_BIT(command.flag, CHANMINUS);
          SET_BIT(command.waiting, CHANMINUS);
        }
        else if (READ_BIT(command.flag, VOLPLUS))
        {
          strcpy(cmd,"cli.vol+\r");
          CLEAR_BIT(command.flag, VOLPLUS);
          SET_BIT(command.waiting, VOLPLUS);
        }
        else if (READ_BIT(command.flag, VOLMINUS))
        {
          strcpy(cmd,"cli.vol-\r");
          CLEAR_BIT(command.flag, VOLMINUS);
          SET_BIT(command.waiting, VOLMINUS);
        }
        SERIALX.print(F("\r")); // cleaner
        SERIALX.print(F(cmd));
        command.timestamp = TIMESTAMP_CUR;
        digitalWrite(PC13, LOW);
      }
      else if (isMode2ON)
      { // MODE 2 : ALARM
        if (READ_BIT(command.flag, PLAYPAUSE))
        {
          FLIP_ALARM_STATE;
          CLEAR_BIT(command.flag, PLAYPAUSE);
        }
        else if (READ_BIT(command.flag, CHANPLUS))
        {
          alarm = CHANGE_ALARM(1, 0);
          CLEAR_BIT(command.flag, CHANPLUS);
        }
        else if (READ_BIT(command.flag, CHANMINUS))
        {
          alarm = CHANGE_ALARM(-1, 0);
          CLEAR_BIT(command.flag, CHANMINUS);
        }
        else if (READ_BIT(command.flag, VOLPLUS))
        {
          alarm = CHANGE_ALARM(0, 1);
          CLEAR_BIT(command.flag, VOLPLUS);
        }
        else if (READ_BIT(command.flag, VOLMINUS))
        {
          alarm = CHANGE_ALARM(0, -1);
          CLEAR_BIT(command.flag, VOLMINUS);
        }
        flag_screen[NEWALARM]=true;
        digitalWrite(PC13, LOW);
      }
      
    }

    if (READ_BIT(command.flag, MODE))
    {
      // at mode exit, save the alarm in EEPROM.
      if (isMode2ON)
      {
        uint16 Status;
        Status = EEPROM.write(AddressWrite, alarm);
        #ifdef DEBUG
          Serial.print("EEPROM.write(0x");
          Serial.print(AddressWrite, HEX);
          Serial.print(", 0x");
          Serial.print(alarm, HEX);
          Serial.print(") : Status : ");
          Serial.println(Status, HEX);
        #endif
      }
      
      isMode2ON = !isMode2ON;
      flag_screen[NEWALARM]=true;
      flag_screen[TOGGLELIGHT]=true;
      CLEAR_BIT(command.flag, MODE);
      digitalWrite(PC13, LOW);
    }

    if (isAskingTime)
    {
      SERIALX.print(F("\r"));         // cleaner
      SERIALX.print(F("sys.date\r")); // Synchronise the current state
      isAskingTime = false;
      //vTaskDelay(500); // leave time to answer. This is exceptionnal (startup and once every hour)
    } 
    if (isAskingIP) // asking two things can't get them processed.
    {
      SERIALX.print(F("\r"));            // cleaner
      SERIALX.print(F("wifi.status\r")); // Synchronise the current state
      isAskingIP = false;
      //vTaskDelay(500); // leave time to answer. This only happens until it has an answer.
    }
    
    serial();
    vTaskDelay(2); // run @500Hz, should be fast enough, but will reduce CPU use.
  }
}

////////////////////////////////////////

int show;

static void buttonsPollingTask(void *pvParameters)
{
  int button[6] = {};
  int prev_button[6] = {64};
  int count[6] = {0};
  int polling = SLOW_POLLING; // By default, poll @15 Hz.
  while (1)
  {
    // PA6, 7, 8, 9, 10, 15
    button[PLAYPAUSE] = READPORTA(PLAYPAUSE);
    button[VOLPLUS] = READPORTA(VOLPLUS);
    button[VOLMINUS] = READPORTA(VOLMINUS);
    button[CHANPLUS] = READPORTA(CHANPLUS);
    button[CHANMINUS] = READPORTA(CHANMINUS);
    
    button[MODE] = READPORTA(MODE + 4); // PA15

    //debug
    //Serial.println(String(button[PLAYPAUSE]) + " " + String(count[PLAYPAUSE]));
    // #################### Disabled polling on A15 !! ###############
    for (int i = 0; i < NBR_BUTTONS; i++)
    {
      // if we start pushing on it
      if (button[i] < 5 && prev_button[i] != button[i] && READ_BIT(command.flag, i) == 0)
      {
        // Fill a counter... When the counter is full, take action !
        polling = FAST_POLLING;
        count[i]++;
        if (count[i] >= POLLCOUNTER * FAST_POLLING) // 10 ms
        {
          count[i] = 0;
          prev_button[i] = button[i];
          SET_BIT(command.flag, i);
          // The delay and the DigitalWrite on PC13 is for debug. PC13 is BUILTIN_LED.
          vTaskDelay(20);
          digitalWrite(PC13, HIGH);
          
          polling = SLOW_POLLING; //ms. Reset polling frequency to something slower.
        }
        // if we release it
      }
      else if (button[i] > 40 && prev_button[i] != button[i])
      {
        // Fill a counter... When full, take action ! here, action = reset the prev_button so that we are able to push it again.
        polling = FAST_POLLING;
        count[i]++;
        if (count[i] >= POLLCOUNTER * FAST_POLLING) // mini 20 ms
        {
          count[i] = 0;
          prev_button[i] = button[i];
          polling = SLOW_POLLING;
        }
      }
      else
      {
        count[i] = 0;
      }
    }
    vTaskDelay(polling); // polling adaptatif !
  }
}


//Setup all things, check for contrast adjust and show initial page.
// ######################### SETUP #################################
void setup(void)
{

  // ######################### BUTTONS SETUP #################################
  // To control things, buttons and two-speeds polling.
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, LOW);
  //PA6 pullup cramÃ©e? elle pull down seulement :(
  //GPIOA->regs->PUPDR |= & (1<<6); // read PA6
  gpio_init(GPIOA);
  for (int i = 1; i <= 15; i++)
    gpio_set_mode(GPIOA, i, GPIO_INPUT_PU);

  // ######################### SERIAL AND LCD #################################
  SERIALX.begin(BAUD);
  Serial.begin(BAUD);

  Serial.println(F("Setup"));
  int error;
  //while (! Serial);
  Serial.println("LCD...");
  Serial.println("Dose: check for LCD");

  // See http://playground.arduino.cc/Main/I2cScanner
  // PB6, PB7
  Wire.begin();
  Wire.beginTransmission(0x3f);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error == 0)
    Serial.println(": LCD found.");
  else
    Serial.println(": LCD not found.");

  lcd.begin(20, 4); // initialize the lcd
  lcd.backlight();
  show = 0;

  // ######################### ALARM INIT #################################
  // Initialize the alarm. First step : hardcoded fixed alarm (like in regular clocks), set to 7h21
  alarm = 0; // initialize all bits to 0 beforehand!
  alarm = 7 * 60 + 21;
  alarm |= (1 << 15);
  flag_screen[NEWALARM]=true;

  // ######################### FLAGS NTP / IP #################################
  //Flags for NTP and IP requests
  isAskingTime = true;
  isAskingIP = true;
  isTimeInvalid = true;
  isIPInvalid = true;

  // set the pointer to the title... this is kinda dumb because it only needs to be done once...
  Song.s_connect = title;

  // ######################### EEPROM INIT ####################################
  uint16 Status;
  EEPROM.PageBase0 = 0x801F000;
  EEPROM.PageBase1 = 0x801F800; // 2 pages, may be reduced to 1 later...
  EEPROM.PageSize  = 0x400;

  Status = EEPROM.init();
  Serial.print("EEPROM.init() : ");
  Serial.println(Status, HEX);
  Serial.println();

  Status = EEPROM.read(AddressWrite, &alarm);
  #ifdef DEBUG
    Serial.print("EEPROM.read(0x");
    Serial.print(AddressWrite, HEX);
    Serial.print(", &..) = 0x");
    Serial.print(alarm, HEX);
    Serial.print(" : Status : ");
    Serial.println(Status, HEX);
  #endif



  // ######################### FREERTOS TASKS #################################
  // names
  char* printScrollName = NULL;
  printScrollName = (char*) malloc(15*sizeof(char));
  strcpy(printScrollName, "printScroll");
  printScrollName[11] = 0;
  //
  int s1 = xTaskCreate(mainTask, NULL , configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 2, &xHandleMain);
  int s2 = xTaskCreate(uartTask, NULL, configMINIMAL_STACK_SIZE + 600, NULL, tskIDLE_PRIORITY + 3, NULL);
  int s3 = pdPASS;
  int s4 = pdPASS;
  int s5 = xTaskCreate(printScrollRTOSTask, NULL, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandlePrintScroll);
  int s6 = xTaskCreate(buttonsPollingTask, NULL, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  
  if (s1 != pdPASS || s2 != pdPASS || s3 != pdPASS || s4 != pdPASS || s5 != pdPASS || s6 != pdPASS)
  {
    Serial.println(F("Task or Semaphore creation problem.."));
    while (1)
      ;
  }
  
  // ######################### TASKS LAUNCH #################################
  timer2_init(); // initialize timer2
  // Start the task scheduler - here the process starts
  vTaskStartScheduler();
  Serial.println(F("Started"));
  // In case of a scheduler error
  Serial.println(F("Die - insufficient heap memory?"));
  while (1)
    ;
}

////////////////////////////////////////
void removeUtf8(byte *characters)
{
  int iindex = 0;
  while (characters[iindex])
  {
    if ((characters[iindex] >= 0xc2) && (characters[iindex] <= 0xc3)) // only 0 to FF ascii char
    {
      characters[iindex + 1] = ((characters[iindex] << 6) & 0xFF) | (characters[iindex + 1] & 0x3F);
      int sind = iindex + 1;
      while (characters[sind])
      {
        characters[sind - 1] = characters[sind];
        sind++;
      }
      characters[sind - 1] = 0;
    }
    iindex++;
  }
}

////////////////////////////////////////
// parse the karadio received line and do the job
void parse(char *line)
{
  char *ici;
  Serial.println(line);
  removeUtf8((byte *)line);

  //////  reset of the esp
  if ((ici = strstr(line, "VS Version")) != NULL)
  {
    setup();
  }
  else
      ////// Meta title   ##CLI.META#:
      if ((ici = strstr(line, "META#: ")) != NULL)
  {
    ///cleartitle(3);
    strcpy(title, ici + 7);
    flag_screen[NEWTITLE1] = true; // the 2nd NEWTITLE2 flag is set up on time...
    ///separator(title);
  }
  else

      ////// ICY0 station name   ##CLI.ICY0#:
      if ((ici = strstr(line, "ICY0#: ")) != NULL)
  {
    //      clearAll();
    if (strlen(ici + 7) == 0)
      strcpy(station, nameset);
    else
      strcpy(station, ici + 7);
    flag_screen[NEWSTATION]=true;
    ///separator(station);
  }
  else if ((ici = strstr(line, "STOPPED#")) != NULL)  ///// STOPPED  ##CLI.STOPPED#
  {

    ///  cleartitle(3);
    strcpy(title, "STOPPED");
    
    if (isPlaying)
    {
      vTaskSuspend(xHandlePrintScroll); // relieve the CPU by disabling one useless task.

      /*
        UART_using_flag_command[PLAYPAUSE] = false;
        UART_using_flag_command[CHANPLUS] = false;
        UART_using_flag_command[CHANMINUS] = false;
      */
      CLEAR_BIT(command.waiting, PLAYPAUSE);  
      CLEAR_BIT(command.waiting, CHANPLUS);  
      CLEAR_BIT(command.waiting, CHANMINUS);  
      
      isPlaying = false;
    }
    flag_screen[NEWTITLE1] = true;
    flag_screen[TOGGLELIGHT] = true;
  }
  else if ((ici = strstr(line, "YING#")) != NULL)
  {
    
    if (isPlaying==false)
    {
      vTaskResume(xHandlePrintScroll); // start back the scrlling
        
      CLEAR_BIT(command.waiting, PLAYPAUSE);  
      CLEAR_BIT(command.waiting, CHANPLUS);  
      CLEAR_BIT(command.waiting, CHANMINUS);  
        
      isPlaying = true;
    }
    flag_screen[TOGGLELIGHT] = true; // isPlaying == true here, so light should be on.
  }
  //////Date Time  ##SYS.DATE#: 2017-04-12T21:07:59+01:00

  if ((ici = strstr(line, "SYS.DATE#:")) != NULL)
  {
    struct tm dtl; // time structure.
    char lstr[BUFLEN];
    if (*(ici + 11) != '2')
    {
      vTaskDelay(100); // invalid date. try again
      return;
    }
    strcpy(lstr, ici + 11);
    sscanf(lstr, "%d-%d-%dT%d:%d:%d", &dtl.tm_year, &dtl.tm_mon, &dtl.tm_mday, &dtl.tm_hour, &dtl.tm_min, &dtl.tm_sec);
    dtl.tm_year -= 1900;
    //      timein = dtl.tm_sec; //
    hours = (dtl.tm_hour) % 24;
    minutes = dtl.tm_min;
    seconds = dtl.tm_sec;
    isAskingTime = false;
    isTimeInvalid = false;
    flag_screen[NEWTIME]=true;
  }

  if ((ici = strstr(line, "IP: ")) != NULL)
  {
    strcpy(oip, ici + 4);
    isIPInvalid = false;
    isAskingIP = false;
	flag_screen[NEWIP]=true;
  }

  //////Volume   ##CLI.VOL#:
  if ((ici = strstr(line, "VOL#:")) != NULL)
  {
    volume = atoi(ici + 6);
    dispvolume = (uint8_t) ((float) volume / 2.56);
    CLEAR_BIT(command.waiting, VOLPLUS);  
    CLEAR_BIT(command.waiting, VOLMINUS); 

    flag_screen[NEWVOLUME] = true; 
  }
}

////////////////////////////////////////
// receive the esp8266 stream
void serial()
{
  int temp;
  static unsigned Rbindex = 0; // receive buffer Rbindex
  //    SERIALX.println(F("Serial"));
  //    if (SERIALX.available() == 0) return;
  while ((temp = SERIALX.read()) != -1)
  {
    switch (temp)
    {
    case '\\':
      break;
    case '\n':
    case '\r':
      if (Rbindex == 0)
        break;
      line[Rbindex] = 0; // end of string
      parse(line);
      Rbindex = 0;
      break;
    default: // put the received char in line
      line[Rbindex++] = temp;
      if (Rbindex > SERIAL_RX_BUFFER_SIZE - 1)
      {
        Serial.println(F("overflow"));
        line[Rbindex] = 0;
        parse(line);
        Rbindex = 0;
      }
    }
  }
}
void loop()
{
} // loop() UNUSED WITH FREERTOS

