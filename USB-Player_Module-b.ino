/*
 Program: USB Micro SD Player Module
    Date: 24-Dec-2020
Revision: 24b
  Author: Gerry Walterscheid, jr.

MP3-Player using DFPlayer mini, ATtiny85, USB, photoresistor or push button switch, 
potentiometer, and status LED. The 3 x 3.5cm module comes with the follow-
ing features.

 - User input available using either photocell or push button switch.
 - Module status (Error, Ready, Play Mode, ect.) via onboard LED.
 - Powered by standard Micro USB 1.0 A phone charger.
 - Read up to 32GB Micro SD with MP3 songs/audio clips.
 - Emulates external SD drive when connected to computer using USB cable.
 - Arduino programming of ATTINY available using ISP port.

Module commands are entered by waving hand over the photocell, or press-
ing push button switch. Only one switching option can be installed.
A command is considered complete if no addditional input is received
within 500 ms. 

Note: A short press or shadow over the photocell is anything less
than 500 ms. A long press is anything greater.

Key . = Short Press, - = Long Press, * = default

 .    = Play next song.
 -    = Stop playing song, and fade to silence.

 ..   = Play previous song.
 .-   = Change play mode (full song, 20 sec, book, auto).

 ...  = Change volume (low, med*, high) in single and 20 sec mode.
 ..-  = Change interval (5s, 10s, 30s, 60s) in autoplay mode.

 .... = Reset user settings (play mode, volume, and interval).
 
  Module Status (via LED):

  Blinking = Error, unable to read files on Micro SD card.
  On       = Ready for new command, or playing song.
  Off      = Processing last command, or sleeping.
  n blinks = Used to show current setting of play mode, volume, etc.
    
ATTINY85 pin layout:
  pin 1: Reset        pin 8: Vcc
  pin 2: P3           pin 7: P2
  pin 3: P4           pin 6: P1
  pin 4: Gnd          pin 5: P0

DFPlayer Mini pin layout:
  pin 1: Vcc          pin 16: Busy
  pin 2: Rx           pin 15: USB-
  pin 3: Tx           pin 14: USB+
  pin 4: DAC_R        pin 13: ADKey2
  pin 5: DAC_L        pin 12: ADKey1
  pin 6: Spk1         pin 11: IO2
  pin 7: Gnd          pin 10: Gnd
  pin 8: Spk2         ping 9: IO1
  
The circuit:
 - ATTiny85 Reset (pin 1) to 10K resistor to Vcc.
 - ATTiny85 P3 (pin 2) to 1K resistor to DFPlayer RX (pin 2).
 - ATTiny85 P4 (pin 3) to DFPlayer TX (pin 3).
 - ATTiny85 GND (pin 4) to GND to DFPlayer Gnd (pins 7, 10).
 - ATTiny85 VCC (pin 8) to VCC to DFPlayer Vcc (pin 1).
 - ATTiny85 P2 (pin 7) to LDR and 50Kohm Pot.
 - ATTiny85 P2 (pin 7) to button switch to 1K resistor to Vcc.
 - ATTiny85 P1 (pin 6) to 10Kohm resistor to status LED to Gnd.
 - ATTiny85 P0 (pin 5) to DFPlayer Busy (pin 16).
 - Vcc to 100nF capacitor to Gnd.

Updates in this version:

- Updated code to be compatible with JL AA20HFj616-94 decoder on the 
  DFPlayer Mini module.

- When entering Book mode, reset the file counter to start at
  Chapter 1. 
  
- Created aliases for the different modes to make these easier to see 
  within the code.

- Following a reset, stop playing file.

- Update file variable everytime a new chapter is started, and save
  to EEPROM to survive a power cycle.

- Limit the sequential functions playNext() and playPrev() to Book mode only.

- Change the playPrev() and playNext() functions so that the file is played
  1st, and then incremented, so that the book won't start playing until a
  new play command is issued.

- On startup, check if in Book mode, to adjust file pointer to point to the
  same chapter that was playing prior to shutdown.

- Fix issues where the song volume is sometimes very low, probably due to the
  next command being ignored following a pause() or stop(). Added short time
  delay, 100 ms to fix.

- Fixed 60 sec delay issue by adding (long) cast to prevent int rollover with
  millis() future time values greater than 32,767 which became -32,768. Unless
  math variables are cast, they will assume the type int.
  
*/

#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <EEPROM.h>

#define DFPlayer_Tx 3                 // ATTiny85 PB3 to DFPlayer Tx
#define DFPlayer_Rx 4                 // ATTiny85 PB4 to DFPlayer Rx
#define Button_switch 2               // ATTiny85 PB2 to switch or LDR
#define LED_status 1                  // ATTiny85 PB1 to LED
#define DFPlayer_busy 0               // ATTiny85 PB0 to DFPlayer Busy

#define LED_on 1                      // LED on = HIGH
#define LED_off 0                     // LED off = LOW

#define ON LOW                        // Dark or button press.
#define OFF HIGH                      // Light or button release.

#define TRUE 1                        // Boolean constant
#define FALSE 0                       // Boolean constant

#define N 128                         // Max number of audio files.

#define Full 0                        // Full song mode
#define Short 1                       // 20 sec song mode
#define Auto 2                        // Auto play mode
#define Book 3                        // Book mode

#define seedAddr 0                    // EEPROM address for random seed.
#define modeAddr 1                    // EEPROM address for mode.
#define volAddr 2                     // EEPROM address for volume.
#define intAddr 3                     // EEPROM address for interval.
#define fileAddr 4                    // EEPROM address for file.

unsigned long timer1;                 // Time since last button press/release.
unsigned long timer2;                 // Time playing audio file.
unsigned long timer3 = 0;             // Time since playing last audio file.
unsigned long playTime = 20000;       // 20 sec playtime.
unsigned long CmdTimeOut = 500;       // Command time out in ms.
byte fadeTime = 3000;                 // 3 sec audio fade.
byte debounce = 20;                   // 20 ms debounce for switch.

byte fileCount;                       // Number of audio files on Micro SD card.
byte file;                            // Last audio file played.
byte state = 0;                       // State machine variable.

byte modeReset = 0;                   // Play mode (0 = full, 20s, auto, book).
byte mode;

byte volArray[3] = { 15, 23, 30 };    // Volume settings (low, med, high).
byte volReset = 1;                    // Init volume setting (1 = medium).
byte volume;

byte intArray[4] = { 5, 10, 30, 60 }; // Interval settings (5s, 10s, 30s, 60s).
byte intvlReset = 1;                  // Init interval setting (10s);
byte interval;

bool paused = false;                  // State of current song.
bool allFiles = false;                // State of valid file numbers for this Micro SD.

SoftwareSerial SoftSerial(DFPlayer_Rx, DFPlayer_Tx);
DFRobotDFPlayerMini DFPlayer;

// ------------------------------------------------------------------ 
//  Initialize
// ------------------------------------------------------------------ 

void setup()
{ 
  pinMode(LED_status, OUTPUT);
  pinMode(Button_switch, INPUT);
  pinMode(DFPlayer_busy, INPUT);

  // Turn on status LED to indicate ATTINY is running.
  digitalWrite(LED_status, LED_on);
  
  // Read seed stored in eeprom to get new series of pseudo random #s.
  unsigned int seed = EEPROM.read(seedAddr);
  randomSeed(seed);

  // Restore data saved in EEPROM. Use mode function to limit range to
  // acceptable values.
  mode = EEPROM.read(modeAddr) % 4;
  volume = EEPROM.read(volAddr) % 3;
  interval = EEPROM.read(intAddr) % 3;
  file = EEPROM.read(fileAddr);

  // Save in EEPROM, to restore after next power cycle.
  EEPROM.write(seedAddr, seed + random(10));
  
  // Configure baud rate for communication to DFPlayer after short pause.
  wait_ms(250);
  SoftSerial.begin(9600);
  
  // Start DFPlayer communication. Indicate error with flashing LED.
  digitalWrite(LED_status, !digitalRead(LED_status));

  while (!DFPlayer.begin(SoftSerial)) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(LED_status, !digitalRead(LED_status));
      wait_ms(100);
    }
  }  
  
  // Initialize DFPlayer settings and determine file count.
  DFPlayer.setTimeOut(500);
  fileCount = DFPlayer.readFileCounts();

  // Determine if even numbered files are unavailable, which appears
  // to occur with large files, ie. audio books.
  DFPlayer.volume(0);

  DFPlayer.play(2);
  DFPlayer.stop();
  wait_ms(100);

  if (DFPlayer.readCurrentFileNumber() != 2)
    allFiles = FALSE;

  // If booting in book mode, decrement the file so that the
  // DFPlayer module will resume on the same chapter it was at
  // prior to shutdown.
  if (mode == Book) {

    // Audio book chapters are located on odd file numbers. 
    // Decrement and make sure the file id doesn't go out of 
    // bounds.
    if (file < 3)
      file = fileCount - 1;
    else
      file -= 2;
  }
      
  // Initialize player volume.
  wait_ms(100);
  DFPlayer.volume(volArray[volume]);
  wait_ms(100);
    
  // Indicate DFPlayer configuration has completed successfully.
  digitalWrite(LED_status, LED_on);
}

void loop()
{
  // ------------------------------------------------------------------ 
  // Use state machine to process user input based on number of rapid
  // button presses. Command is determined to be complete following a
  // 0.5 sec pause.
  // ------------------------------------------------------------------ 
  
  switch (state) {   
  case 0 : // No button press.
    // This is the default state. If a button press is detected, turn
    // off status LED and advance to the next state.
  
    // If button press, set command timeout timer and advance to next state.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      
      digitalWrite(LED_status, LED_off);
      state = 1;
    }
    break;

  case 1 : // 1st button press.
    //  -  = Stop playing song, and fade to silence.
  
    // If no more input, command was long press (-).
    if (millis() > timer1) {

      // If song is playing, fade volume.
      if (digitalRead(DFPlayer_busy) == ON) {
        fadeVolumeDown(volArray[volume]);

        // Pause the player and reset the volume.
        DFPlayer.pause();
        wait_ms(100);
        DFPlayer.volume(volArray[volume]);
        wait_ms(100);

        paused = true;
      }
 
      state = 10;
      break; 
    }
          
    // If button released, set command timeout timer and advance to next state.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 2;
    }
    break;

  case 2 : // 1st button release.
    //  .  = Play next song.
      
    // If no more input, play next song.
    if (millis() > timer1) {

      // Select command based on paused state.
      if (paused) 
        DFPlayer.start();
      else {
        DFPlayer.stop();
        wait_ms(100);

        // If in book mode, play next song. Otherwise pick random song.
        if (mode == Book) {
          playNext();
          
          // Save in EEPROM, to restore after next power cycle.
          EEPROM.update(fileAddr, file);
        }
        else
          playRandom(fileCount);
      }

      // Start song playtime timer and clear pause state.
      timer2 = millis() + playTime;
      paused = false;
            
      digitalWrite(LED_status, LED_on);
      state = 0;
      break; 
    }

    // If button press, advance to next state.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 3; }
           
    break;
        
  case 3 : // 2nd button press.
    //  .-  = Change play mode.

    // If no more input, change play mode.
    if (millis() > timer1) {
      mode = ++mode % 4; 

      /// Save in EEPROM, to restore after next power cycle.
      EEPROM.update(modeAddr, mode);
      
      // Provide user feedback of current play mode (full, 20s, auto, book).
      displayStatus(mode); 

      // If entering book mode, go to chapter 1 and clear pause state.
      if (mode == Book) {
        DFPlayer.stop();
        wait_ms(100);

        paused = false;        
        file = 1;

        // Save in EEPROM, to restore after next power cycle.
        EEPROM.update(fileAddr, file);
      }
      
      state = 10;
      break;
    }

    // If button release, advance to next state.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 4;
    }
    
    break;

  case 4 : // 2nd button release.
    //  ..  = Play previous song.
    
    // If no more input, play previous song (Book mode only).
    if (millis() > timer1) {

      if (mode == Book) {
        DFPlayer.stop();
        wait_ms(100);
        playPrev();

        // Save in EEPROM, to restore after next power cycle.
        EEPROM.update(fileAddr, file);
          
        // Clear pause state.
        paused = false;
      }
      
      digitalWrite(LED_status, LED_on);
      state = 0;
      break; 
    }

    // If button press, advance to next state.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 5;
    }
    
    break;

  case 5 : // 3rd button press.
    //  ..-  = Change interval time, which is used in autoplay mode.

    // If no more input, adjust interval setting.
    if (millis() > timer1) {
      interval = ++interval % 4;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.update(intAddr, interval);

      // Provide user feedback of interval time.
      displayStatus(interval);
    
      state = 10;
      break; 
    }

    // If button release, advance to next state.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 6;
    }
    
    break;
    
  case 6 : // 3nd button release.
    //  ...  = Change volume.
  
    // If no more input, change the volume settings.
    if (millis() > timer1) {
      volume = ++volume % 3;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.update(volAddr, volume);

      // Provide user feedback of current volume level (low, med, high).
      displayStatus(volume); 
      DFPlayer.volume(volArray[volume]);

      digitalWrite(LED_status, LED_on);
      state = 0;
      break; 
    }

    // If button release, advance to next state.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 7;
    }
    
    break;

  case 7 : // 4rd button press.
    //  ...- = Not used.

    // If no more input, ignore and wait for button release.
    if (millis() > timer1) {
      state = 10;
      break; 
    }

    // If button release, advance to next state.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 8;
    }
    
    break;
    
  case 8 : // 4th button release.
    //  ....  = Reset all user settings (except file) to their default state.
  
    // If no more input, reset user settings..
    if (millis() > timer1) {
      mode = modeReset;
      interval = intvlReset;
      volume = volReset;
      file = 1;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.write(modeAddr, mode);
      EEPROM.write(volAddr, volume);
      EEPROM.write(intAddr, interval);
      EEPROM.write(fileAddr, file);

      // Provide user feedback of reset.
      displayStatus(0); 

      // Stop playing current file, and reset the volume.
      DFPlayer.stop();
      wait_ms(100);
      DFPlayer.volume(volArray[volume]);
      wait_ms(100);
      
      digitalWrite(LED_status, LED_on);
      state = 0;
      break; 
    }

    // If button press, wait for button release.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      state = 10;
    }
    
    break;
    
  case 10 : // Wait for button release after long press.
    // If button release, reset state to 0.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);

      // Restart the autoplay delay interval timer. This is only used when
      // the autoplay mode is enabled.
      timer3 = millis() + (long) intArray[interval] * 1000;
      
      digitalWrite(LED_status, LED_on);
      state = 0;
    }
      
    break; 
  }

  // ------------------------------------------------------------------ 
  // Items to check and perform periodically, regardless of state.
  // ------------------------------------------------------------------ 
    
  switch(mode) {
  case Short : // 20s mode.
    // If playing song in 20s mode and song still playing...
    if (digitalRead(DFPlayer_busy) == ON)
  
      // Fade volume after 20s.
      if (millis() > timer2) {
        digitalWrite(LED_status, LED_off);
        fadeVolumeDown(volArray[volume]);

        // Stop playing and reset the volume.
        DFPlayer.stop();
        wait_ms(100);
        DFPlayer.volume(volArray[volume]);
        wait_ms(100);
      
        digitalWrite(LED_status, LED_on);
      }
    break;

  case Auto : // Autoplay mode.
    // If in autoplay mode, no button pressed and in state 0...
    if (digitalRead(Button_switch) == OFF and state == 0) {
      // Play next song after reaching interval, then set playTime.

      if (digitalRead(DFPlayer_busy) == OFF and millis() > timer3) {
        playRandom(fileCount);
        timer2 = millis() + playTime;
      }

      // Initialize timer3 after song ends, and reset paused status.
      if (digitalRead(DFPlayer_busy) == ON) {
        timer3 = millis() + (long) intArray[interval] * 1000;
        paused = false;
      }
    }
    break;
  }
}

// ------------------------------------------------------------------ 
//  Functions
// ------------------------------------------------------------------ 

void wait_ms(int Time) {
  int unsigned long waitTimer = millis() + Time;

  while(millis() < waitTimer);
}

void playRandom(byte count) {
  // Play a random file, with adjustment when only odd files are accepted
  // on the DFPlayer Mini module.
  
  byte myFile;

  wait_ms(100);
  
  if (allFiles)
    myFile = randomNumber(count) + 1;
  else
    // Find an odd file number between 1 and count - 1, discarding all
    // even numbers returned.
    do {
      myFile = randomNumber(count - 1) + 1;
    } while (myFile % 2 == 0);
    
  DFPlayer.play(myFile);
  wait_ms(100);
}

void playNext() {
  // Used for the Book mode, to advance to the next chapter of an
  // audio book saved on the Micro SD card.

  wait_ms(100);
  DFPlayer.play(file);
    
  // Audio book chapters are located on odd file numbers. Increment
  // and make sure the file id doesn't go out of bounds.
  file += 2;
  
  if (file > fileCount )
    file = 1;
    
  wait_ms(100);
}

void playPrev() {
  // Used for the Book mode, to advance to the previous chapter of an
  // audio book saved on the Micro SD card.

  wait_ms(100);
  DFPlayer.play(file);
    
  // Audio book chapters are located on odd file numbers. Decrement
  // and make sure the file id doesn't go out of bounds.

  if (file < 3)
    file = fileCount - 1;
  else
    file -= 2;

  wait_ms(100);
}

void fadeVolumeDown(byte maxVolume) {
  // Fade the volume down from the current max to 0, within fadeTime.

  int slice;
  slice = (fadeTime / maxVolume) * 10;
  
  for (int vol=maxVolume; vol > 0; vol--) {
    wait_ms(slice);
    DFPlayer.volume(vol);
  }

  // Add a delay to avoid missing the pending pause or stop command.
  wait_ms(100);
}

void displayStatus(byte n) {
  // Provide program status using LED blinks.
  
  wait_ms(500);
  for (byte i = 0; i <= n; i++) {
    digitalWrite(LED_status, LED_on);        
    wait_ms(100);
    digitalWrite(LED_status, LED_off);
    wait_ms(200);
  }
   
  wait_ms(500);
}

int randomNumber(byte n) {
  // Generate random number using the Fisher-Yates shuffle, which guarantees
  // that every number is eventually selected with n tries, where n is the
  // the range of numbers to choose from.

  static byte randomTable[N];      // table of random numbers.
  static byte pos = n;             // position of next random number.
  static byte prev = n;            // previous random number.
  byte j, temp;                    // used for shuffle routine.

  // Limit index values to declared array size.
  if (n >= N)
    n = N - 1;

  if (pos == n) {                  // generate new random table?
    // Initialize randomTable with values between 0 and N-1.
    for (byte i = 0; i < n; i++)
      randomTable[i] = i;

    // Shuffle the randomTable entries.
    for (byte i = n - 1; i > 0; i--) { 
      j = random(i + 1);           // select random position to swap.
    
      temp = randomTable[i];
      randomTable[i] = randomTable[j];
      randomTable[j] = temp; }

    // Reset pointer used to retrieve random numbers from table.
    pos = 0;
    
    // After shuffle, there is a possibility that the next random number 
    // drawn will match the previous one. If this occurs, discard and get 
    // next number from the table. 
    if (n > 1 and randomTable[pos] == prev)
      pos++; 
    }

  prev = randomTable[pos];        // save last random number.
  return randomTable[pos++];      // return random number and increment pointer.
}
