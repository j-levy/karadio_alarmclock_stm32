
#include "karadio_alarmclock_stm32.h"


HardwareTimer timer(2);
LiquidCrystal_I2C lcd(0x3f, LCD_WIDTH, LINES);

bool state = false; // start stop on Ok ir key
char line[SERIAL_RX_BUFFER_SIZE]; // receive buffer
char station[BUFLEN]; //received station
char title[BUFLEN]; // received title
char nameset[BUFLEN] = {"0"}; ; // the local name of the station
int16_t volume;

volatile char flag_command[6] = {0}; // volatile is for interrupts.

time_t timestamp = 0;
unsigned char hours;
unsigned char minutes;
unsigned char seconds;

char flag_screen[6] = {0};

bool isLight = true;
bool isUARTused = false;
bool isLCDused = false;
bool isPlaying = true;
bool isMode2ON = false;

unsigned ledcount = 0; // led counter
unsigned timerScreen = 0;
unsigned timerScroll = 0;
unsigned timein = 0;

struct tm *dt;

/*-----( Declare objects )-----*/
struct InfoScroll {
  char line; // line number, 0-3
  char *s_connect;
  char fw;
  int waiting;
};

InfoScroll Song = {0, "INIT...", 2, 900};

uint16 alarm;

// ip
char oip[20] = "___.___.___.___";

// init timer 2 for irmp led screen etc
void timer2_init ()
{
  timer.pause();
  //    timer.setPrescaleFactor( ((F_CPU / F_INTERRUPTS)/8) - 1);
  //    timer.setOverflow(7);
  timer.setPrescaleFactor(  ((F_CPU / F_INTERRUPTS) / 10) );
  timer.setOverflow(9);
  timer.setChannelMode(TIMER_CH1, TIMER_OUTPUT_COMPARE);
  timer.setCompare(TIMER_CH1, 0);  // Interrupt  after each update
  timer.attachInterrupt(TIMER_CH1, TIM2_IRQHandler);
  // Refresh the timer's count, prescale, and overflow
  timer.refresh();
  // Start the timer counting
  timer.resume();
}

void TIM2_IRQHandler()     // Timer2 Interrupt Handler
{
  // one second tempo: led blink, date/time and scrren and scroll
  if ( ++ledcount == (F_INTERRUPTS)) //1 sec
  {

    ledcount = 0;//
    //digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    // Time compute
    localTime();
    timestamp++;  // time update


    /*
      if (state) timein = 0; // only on stop state
      else timein++;
      dt=gmtime(&timestamp);
      if (((timein % DTIDLE)==0)&&(!state)  ) {
       if ((timein % (30*DTIDLE))==0){ itAskTime=true;timein = 0;} // synchronise with ntp every x*DTIDLE
       if (stateScreen != stime) {itAskTime=true;itAskStime=true;} // start the time display
      }
      if (((dt->tm_sec)==0)&&(stateScreen == stime)) { mTscreen = 1;  }

    */
    // Other slow timers
    timerScreen++;
    timerScroll++;
  }
}


void localTime() {
  // Using a timer to get triggered. Maybe one day I'll use the RTClock... but is it that necessary ?
  //Serial.println(seconds);
  seconds++;
  flag_screen[NEWTIME]++; // this makes the : blink
  if (seconds >= 60)
  {
    seconds = 0;
    minutes++;
    if (minutes >= 60)
    {
      minutes = 0;
      hours++;
      if (hours >= 24)
      {
        hours = 0;
      } //endif (hours)
    } // endif (minutes)
    // check alarm clock.
          
    if (hours == READ_ALARM_HOURS && minutes == READ_ALARM_MINUTES && seconds == 0 &&!isPlaying && READ_ALARM_STATE == 1)
      flag_command[PLAYPAUSE]++; // act as if the play-pause button was pressed !
  } // endif(seconds)
}

static void printScrollRTOSTask(void *pvParameters) {
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

      while (isLCDused); // wait for the LCD to be available.

      // Write that stuff
      isLCDused = true;
      lcd.setCursor(0, Song.line);
      lcd.print(p_message);
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
      if (flag_screen[NEWTITLE] == 1)
      {
        finished = true;
        flag_screen[NEWTITLE]--; // remember, we have to update 2 different locations. First is, when we CLEAN UP the whole line, Second is here.
      }
    } // endwhile(!finished)
  } // endwhile(1)
}


//-------------------------------------------------------
// Main task used for display the screen and blink the led
static void mainTask(void *pvParameters) {
  //Serial.println(F("mainTask"));
  bool alt = true;
  while (1) {

    /// HERE ADD SCREEN DRAWING FOR TITLE, STATION, TIME.
    int i = 0;
    if (!isLCDused) // Critical section ahead : takes over the LCD. "if" so that it can be skipped if anything else happens (not locked)
    {
      isLCDused = true;

      /*
        if (flag_screen[TOGGLELIGHT] == 1)
        {
        lcd.backlight();
        flag_screen[TOGGLELIGHT] = 0;
        }
        else if (flag_screen[TOGGLELIGHT] == -1)
        {
        lcd.noBacklight();
        flag_screen[TOGGLELIGHT] = 0;
        }
      */

      // Clean up the whole line
      if (flag_screen[NEWTITLE] == 2)
      {
        lcd.setCursor(0, 0);
        lcd.print("                    ");
        
        lcd.setCursor(0, 1);
        lcd.print("                    ");
        /*
        lcd.setCursor(0, 2);
        lcd.print("                    ");
        */
        // also displays IP here once :
        lcd.setCursor(0, 1);
        lcd.print(oip);
        flag_screen[NEWTITLE]--;
      }
      // Clean up the whole line
      if (flag_screen[NEWSTATION])
      {
        lcd.setCursor(0, 3);
        lcd.print("                    ");
        lcd.setCursor(0, 3);
        lcd.print(station); // display on the 4th line of my 20x4 LCD the station name (ICY0)
        flag_screen[NEWSTATION]--;
      }

      // update clock on screen. This shouldn't stay as is. Some day, I will simply display the alarm clock on this line.
      if (flag_screen[NEWTIME])
      {
        lcd.setCursor(15, 3);
        // the 'alt' thing is to alternate the blinking for ':'
        if (alt)
        {
          lcd.print(String(hours) + ":" + (minutes <= 9 ? "0" : "") + String(minutes));
          alt = !alt;
        }
        else
        {
          lcd.print(String(hours) + " " + (minutes <= 9 ? "0" : "") + String(minutes));
          alt = !alt;
        }
        flag_screen[NEWTIME]--;
      }

      if (flag_screen[NEWVOLUME] == 1)
      {
        lcd.setCursor(10, 2);
        lcd.print("          ");
        flag_screen[NEWVOLUME]--;
      }
      else if (flag_screen[NEWVOLUME] == 3*(1000/FREQ_TASK_SCREEN)) // just the beginning
      {
        lcd.setCursor(10, 2);
        lcd.print(String((round(((float) volume)*100/254) < 10 ? "  " : ((round(((float) volume)*100/254)<100 ? " " : ""))))+String("VOL : ")+String(round(((float) volume)*100/254))+"%");
        flag_screen[NEWVOLUME]--;
      } else if (flag_screen[NEWVOLUME])
      {
        flag_screen[NEWVOLUME]--;
      }

      if (flag_screen[NEWALARM])
      {
        lcd.setCursor(0, 2);
        lcd.print( (isMode2ON ? "     >" : "") 
                    + String(READ_ALARM_HOURS)
                    +":" 
                    + String(READ_ALARM_MINUTES)
                    +(READ_ALARM_STATE == 1 ? " ON " : " OFF")
                    +(!isMode2ON ? "      " : ""));
        flag_screen[NEWALARM]--;
      }
   
      isLCDused = false;
    }
    vTaskDelay(FREQ_TASK_SCREEN); // update the screen @5Hz (should be fast enough !)
  }
}

//-------------------------------------------------------
// Uart task: receive the karadio log and parse it.
static void uartTask(void *pvParameters) {

  Serial.println(F("uartTask"));
  vTaskDelay(1000);
  SERIALX.print(F("\r")); // cleaner
  SERIALX.print(F("cli.info\r")); // Synchronise the current state
  while (1) {
    // Mode 1 - control the radio.
    if (flag_command[PLAYPAUSE] + flag_command[VOLPLUS] + flag_command[VOLMINUS] + flag_command[CHANPLUS] + flag_command[CHANMINUS] >= 1)
    {
      const char *cmd;
      cmd = (const char*) malloc(20 * sizeof(char));

      if (!isMode2ON)
      {
        if (flag_command[PLAYPAUSE])
        {
          cmd = (!isPlaying ? "cli.start\r" : "cli.stop\r");
          isPlaying = !isPlaying;
          flag_command[PLAYPAUSE] = 0;
        } else if (flag_command[CHANPLUS]) {
          cmd = "cli.next\r";
          flag_command[CHANPLUS] = 0;
        } else if (flag_command[CHANMINUS]) {
          cmd = "cli.prev\r";
          flag_command[CHANMINUS] = 0;
        } else if (flag_command[VOLPLUS]) {
          cmd = "cli.vol+\r";
          flag_command[VOLPLUS] = 0;
        } else if (flag_command[VOLMINUS]) {
          cmd = "cli.vol-\r";
          flag_command[VOLMINUS] = 0;
        }
        SERIALX.print(F("\r")); // cleaner
        SERIALX.print(F(cmd));
        digitalWrite(PC13, LOW);
      } else { // MODE 2 : ALARM 
        if (flag_command[PLAYPAUSE])
        {
          FLIP_ALARM_STATE;
          flag_command[PLAYPAUSE] = 0;
        } else if (flag_command[CHANPLUS]) {
          alarm = CHANGE_ALARM(1,0);
          flag_command[CHANPLUS] = 0;
        } else if (flag_command[CHANMINUS]) {
          alarm = CHANGE_ALARM(-1,0);
          flag_command[CHANMINUS] = 0;
        } else if (flag_command[VOLPLUS]) {
          alarm = CHANGE_ALARM(0,1);
          flag_command[VOLPLUS] = 0;
        } else if (flag_command[VOLMINUS]) {
          alarm = CHANGE_ALARM(0,-1);
          flag_command[VOLMINUS] = 0;
        }
        flag_screen[NEWALARM]++;
        digitalWrite(PC13, LOW);
      }
    }

     if (flag_command[MODE]) {
        isMode2ON = !isMode2ON;
        flag_screen[NEWALARM]++;
        flag_command[MODE]--;
        digitalWrite(PC13, LOW);
    }
    
    serial();
    vTaskDelay(1);
  }

}


static void NTPTask(void *pvParameters) {
  Serial.println(F("NTPTask"));
  while (1)
  {
    vTaskDelay(5000);
    SERIALX.print(F("\r")); // cleaner
    SERIALX.print(F("wifi.status\r")); // Synchronise the current state
    vTaskDelay(700);

    vTaskDelay(95000);
    SERIALX.print(F("\r")); // cleaner
    SERIALX.print(F("sys.date\r")); // Synchronise the current date over ntp
    vTaskDelay(95000);

  }
}



////////////////////////////////////////

int show;

void setup2(bool ini)
{
  int error;
  delay(1000); // needed to let the Blue Pill go into Serial Mode (instead of DFU mode ?)
  Serial.println("LCD...");

  //while (! Serial);

  Serial.println("Dose: check for LCD");

  // See http://playground.arduino.cc/Main/I2cScanner
  // PB6, PB7
  Wire.begin();
  Wire.beginTransmission(0x3f);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);

  if (error == 0) {
    Serial.println(": LCD found.");

  } else {
    Serial.println(": LCD not found.");
  } // if

  lcd.begin(20, 4); // initialize the lcd
  lcd.backlight();
  show = 0;


  // Initialize the alarm. First step : hardcoded fixed alarm (like in regular clocks), set to 7h41
  alarm = 0;
  /*
  alarm |= 1 << 11; // 11th bit : alarm on or off.
  alarm |= 7 << 6;  // bits 6 to 11 : hours
  alarm |= 41;      // bit 0 to 5 : minutes
  */
  alarm = 0; // initialize all bits to 0 !
  alarm = 9*60+35;
  alarm |= (1 << 15);
  flag_screen[NEWALARM]++;
} // setup()



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
    for (int i = 0; i < 6; i++)
    {
      // if we start pushing on it
      if (button[i] < 5  && prev_button[i] != button[i] && flag_command[i] == 0)
      {
        // Fill a counter... When the counter is full, take action !
        polling = FAST_POLLING;
        count[i] ++;
        if (count[i] >= 10*FAST_POLLING) // 20 ms
        {
          count[i] = 0;
          prev_button[i] = button[i];
          flag_command[i] = 1;
          // The delay and the DigitalWrite on PC13 is for debug. PC13 is BUILTIN_LED.
          vTaskDelay(20);
          digitalWrite(PC13, HIGH);
          
          polling = SLOW_POLLING; //ms. Reset polling frequency to something slower.
        }
      // if we release it
      } else if (button[i] > 40  && prev_button[i] != button[i] ) {
        // Fill a counter... When full, take action ! here, action = reset the prev_button so that we are able to push it again.
        polling = FAST_POLLING;
        count[i] ++;
        if (count[i] >= 10*FAST_POLLING) // mini 20 ms
        {
          count[i] = 0;
          prev_button[i] = button[i];
          polling = SLOW_POLLING;
        }
      } else {
        count[i] = 0;
      }
    }
    vTaskDelay(polling); // polling adaptatif !
  }
}


//Setup all things, check for contrast adjust and show initial page.
// ######################### SETUP #################################
void setup(void) {
  // To control things, buttons and two-speeds polling.
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, LOW);
  //PA6 pullup cramée? elle pull down seulement :(
  //GPIOA->regs->PUPDR |= & (1<<6); // read PA6
  gpio_init(GPIOA);
  for (int i = 1; i <= 15; i++)
    gpio_set_mode(GPIOA, i, GPIO_INPUT_PU);


  SERIALX.begin(BAUD);
  Serial.begin(BAUD);

  Serial.println(F("Setup"));

  // set the pointer to the title... this is kinda dumb because it only needs to be done once...
  Song.s_connect = title;

  /// HERE START THE LCD SCREEN
  setup2(false);

  /// HERE TASKS ARE CREATED (FREERTOS)
  int s1 = xTaskCreate(mainTask, NULL, configMINIMAL_STACK_SIZE + 250, NULL, tskIDLE_PRIORITY + 2, NULL);
  int s2 = xTaskCreate(uartTask, NULL, configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 3, NULL);
  int s3 = xTaskCreate(NTPTask, NULL, configMINIMAL_STACK_SIZE + 100, NULL, tskIDLE_PRIORITY + 10, NULL);
  //int s4 = xTaskCreate(localTimeTask, NULL, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  int s4 = pdPASS;
  int s5 = xTaskCreate(printScrollRTOSTask, NULL, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
  int s6 = xTaskCreate(buttonsPollingTask, NULL, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

  if ( s1 != pdPASS || s2 != pdPASS || s3 != pdPASS || s4 != pdPASS || s5 != pdPASS || s6 != pdPASS) {
    Serial.println(F("Task or Semaphore creation problem.."));
    while (1);
  }
  timer2_init(); // initialize timer2
  // Start the task scheduler - here the process starts
  vTaskStartScheduler();
  Serial.println(F("Started"));
  // In case of a scheduler error
  Serial.println(F("Die - insufficient heap memory?"));
  while (1);

}

////////////////////////////////////////
void removeUtf8(byte *characters)
{
  int iindex = 0;
  while (characters[iindex])
  {
    // apparently this if-then-else doesn't work ! Lazy to remove it.
    if ((characters[iindex] >= 0xc2) && (characters[iindex] <= 0xc3)) // only 0 to FF ascii char
    {
      //      SERIALX.println((characters[iindex]));
      if (characters[iindex] == 'é' || characters[iindex] == 'è' || characters[iindex] == 'ê' || characters[iindex] == 'ë')
        characters[iindex] = 'e';

      else if (characters[iindex] == 'à')
        characters[iindex] = 'a';

      else if (characters[iindex] == 'î' || characters[iindex] == 'ï' || characters[iindex] == 'ì')
        characters[iindex] = 'i';

      else if (characters[iindex] == 'ù')
        characters[iindex] = 'u';
      else if (characters[iindex] == 'ô' || characters[iindex] == 'ö')
        characters[iindex] = 'o';
      else
      {
        characters[iindex + 1] = ((characters[iindex] << 6) & 0xFF) | (characters[iindex + 1] & 0x3F);
        int sind = iindex + 1;
        while (characters[sind]) {
          characters[sind - 1] = characters[sind];
          sind++;
        }
        characters[sind - 1] = 0;
      }
    }
    iindex++;
  }
}


////////////////////////////////////////
// parse the karadio received line and do the job
void parse(char* line)
{
  char* ici;
  Serial.println(line);
  removeUtf8((byte*)line);

  //////  reset of the esp
  if ((ici = strstr(line, "VS Version")) != NULL)
  {
    ///clearAll();
    ///setup2(true);
  }
  else
    ////// Meta title   ##CLI.META#:
    if ((ici = strstr(line, "META#: ")) != NULL)
    {
      ///cleartitle(3);
      strcpy(title, ici + 7);
      flag_screen[NEWTITLE] = 2; // 2 things to update !
      ///separator(title);
    } else

      ////// ICY0 station name   ##CLI.ICY0#:
      if ((ici = strstr(line, "ICY0#: ")) != NULL)
      {
        //      clearAll();
        if (strlen(ici + 7) == 0) strcpy (station, nameset);
        else strcpy(station, ici + 7);
        flag_screen[NEWSTATION]++;
        ///separator(station);
      } else
        ////// STOPPED  ##CLI.STOPPED#
        if ((ici = strstr(line, "STOPPED")) != NULL)
        {
          state = false;
          ///  cleartitle(3);
          strcpy(title, "STOPPED");
          lcd.noBacklight();
          flag_screen[NEWTITLE] = 2;
          //flag_screen[TOGGLELIGHT] = -1;
        } else if ((ici = strstr(line, "YING#")) != NULL)
        {
          lcd.backlight();
          //flag_screen[TOGGLELIGHT] = 1;
        }


  /*
    /////// Station Ip      ' Station Ip:
    else
    if ((ici=strstr(line,"Station Ip: ")) != NULL)
    {
      eepromReadStr(EEaddrIp, oip);
      if ( strcmp(oip,ici+12) != 0)
        eepromWriteStr(EEaddrIp,ici+12 );
    } else
    //////Nameset    ##CLI.NAMESET#:
    if ((ici=strstr(line,"MESET#: ")) != NULL)
    {
    clearAll();
    strcpy(nameset,ici+8);
    ici = strstr(nameset," ");
    if (ici != NULL)
    {
       strncpy(nameNum,nameset,ici-nameset+1);
       nameNum[ici - nameset+1] = 0;
       strcpy (futurNum,nameNum);
    }
    strcpy(nameset,nameset+strlen(nameNum));
    lline[STATIONNAME] = nameset;
    markDraw(STATIONNAME);
    } else
    //////Playing    ##CLI.PLAYING#
    if ((ici=strstr(line,"YING#")) != NULL)
    {
    digitalWrite(PIN_PLAYING, HIGH);
    state = true;
    if (stateScreen == stime) Screen(smain0);
    if (strcmp(title,"STOPPED") == 0)
    {
      title[0] = 0;
      separator(title);
    }
    } else
    //////Volume   ##CLI.VOL#:
    if ((ici=strstr(line,"VOL#:")) != NULL)
    {
     volume = atoi(ici+6);
     strcpy(aVolume,ici+6);
     if (dvolume)
        Screen(svolume);
     else
        dvolume = true;
     markDraw(VOLUME);
     timerScreen = 0;
    } else
    //////Volume offset    ##CLI.OVOLSET#:
    if ((ici=strstr(line,"OVOLSET#:")) != NULL)
    {
     dvolume = false; // don't show volume on start station
    }else
    //////list station   #CLI.LISTINFO#:
    if ((ici=strstr(line,"LISTINFO#:")) != NULL)
    {
     char* ptrstrstr;
     strcpy(sline, ici+10);
     ptrstrstr = strstr(sline,",");
     if (ptrstrstr != NULL)  *ptrstrstr =0;
     Screen(sstation);
     timerScreen = 0;
    } else
  */
  //////Date Time  ##SYS.DATE#: 2017-04-12T21:07:59+01:00

  if ((ici = strstr(line, "SYS.DATE#:")) != NULL)
  {
    struct tm dtl; // time structure.
    char lstr[BUFLEN];
    if (*(ici + 11) != '2') {
      vTaskDelay(100);  // invalid date. try again
      return;
    }
    strcpy(lstr, ici + 11);
    sscanf(lstr, "%d-%d-%dT%d:%d:%d", &dtl.tm_year, &dtl.tm_mon, &dtl.tm_mday, &dtl.tm_hour, &dtl.tm_min, &dtl.tm_sec);
    dtl.tm_year -= 1900;
    //      timein = dtl.tm_sec; //
    timestamp = mktime(&dtl);
    hours = (dtl.tm_hour) % 24;
    minutes = dtl.tm_min;
    seconds = dtl.tm_sec;
    flag_screen[NEWTIME]++;
  }

  if ((ici = strstr(line, "IP: ")) != NULL)
  {
    strcpy(oip, ici + 4);
  }

    //////Volume   ##CLI.VOL#:
    if ((ici=strstr(line,"VOL#:")) != NULL)
    {
     volume = atoi(ici+6);
     flag_screen[NEWVOLUME]=(1000/FREQ_TASK_SCREEN)*3;// 3*(200ms * 5) = 3 seconds
    }

}

////////////////////////////////////////
// receive the esp8266 stream
void serial()
{
  int temp;
  static unsigned  Rbindex = 0; // receive buffer Rbindex
  //    SERIALX.println(F("Serial"));
  //    if (SERIALX.available() == 0) return;
  while ((temp = SERIALX.read()) != -1)
  {
    switch (temp)
    {
      case '\\': break;
      case '\n' :
      case '\r' : if (Rbindex == 0) break;
        line[Rbindex] = 0; // end of string
        parse(line);
        Rbindex = 0;
        break;
      default : // put the received char in line
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




