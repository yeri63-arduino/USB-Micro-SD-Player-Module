/*
 Program: USB Micro SD Player Module
    Date: 18-Jan-2021
Revision: 24i
  Author: Gerry Walterscheid, jr.

MP3-Player using DFPlayer mini, ATtiny85, USB, photoresistor or push button 
switch, potentiometer, and status LED. The 3 x 3.5cm module comes with the 
following features.

 - User input available using either photocell or push button switch.
 - Module status (Error, Ready, Play Mode, ect.) via onboard LED.
 - Powered by standard Micro USB 1.0 A phone charger.
 - Read up to 32GB Micro SD with MP3 songs/audio clips.
 - Emulates external SD drive when connected to computer using USB cable.
 - Arduino programming of ATTINY85 available using ISP port.

Module commands are entered by waving hand over the photocell, or press-
ing push button switch. Only one input method can be installed on the 
module. A command is considered complete if no addditional input is 
received within 500 ms. 

Note: A short press or shadow over the photocell is anything less
than 500 ms. A long press is anything greater.

Key . = Short Press, - = Long Press, * = default

 .    = Play next song.
 -    = Stop playing song, and fade to silence.

 ..   = Play previous song.
 .-   = Change play mode (full*, short, book).

 ...  = Change volume (low, med*, high) of songs.
 ..-  = Change interval (5s, 10s, 30s, off*) between each song.

 .... = Reset user settings (play mode, volume, and interval).
 ...- = Change count (1*, 2, 3, unlimited) of songs to play.
 
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

- Allow the ability to disable autoplay using the interval setting.
  This lets the module retain the ability to use autoplay in both
  single and short modes.

- Change 60s interval to off, to disable autoplay feature. In this
  setting, the photoresistor module would not start playing once the
  room lights are turned on, but when a play command was issued by
  a hand wave over the sensor. This has no effect on the push button
  model.

- Change the default interval setting from 10s to off, so that
  autoplay is not running by default. The module will only play a
  song after a play command is issued.

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

#define N 128                         // Max number of audio files.

#define Full 0                        // Full song mode
#define Short 1                       // 30 sec song mode
#define Book 2                        // Book mode

#define seedAddr 0                    // EEPROM address for random seed.
#define fileAddr 1                    // EEPROM address for file.
#define modeAddr 2                    // EEPROM address for mode.
#define volAddr 3                     // EEPROM address for volume.
#define intAddr 4                     // EEPROM address for interval.
#define cntAddr 5                     // EEPROM address for count.

#define modeCount 3                   // Number of possible modes.
#define volCount 3                    // Number of possible volume settings.
#define intvlCount 4                  // Number of possible interval settings.
#define cntCount 4                    // Number of possible count settings.

unsigned long timer1;                 // Time since last button press/release.
unsigned long timer2;                 // Time playing audio file.
unsigned long timer3 = 0;             // Time since playing last audio file.
unsigned long playTime = 30000;       // 30 sec playtime.
unsigned long CmdTimeOut = 500;       // Command time out in ms.
unsigned long sleepTimeOut = 5000;    // Sleep time out in ms.
byte fadeTime = 3000;                 // 3 sec audio fade.
byte debounce = 20;                   // 20 ms debounce for switch/photoresistor.

byte fileCount;                       // Number of audio files on Micro SD card.
byte file;                            // Next audio file to play.
byte prevFile;                        // Previous audio file played.
byte state = 99;                      // State machine variable.
byte fileStep = 1;                    // File step for increment and decrement.
byte temp;                            // Used to swap file with prevFile.
byte playCount;                       // Number of times played.

byte modeReset = 0;                   // Play mode (0 = full, 20s, book).
byte mode;

byte volArray[3] = { 10, 18, 25 };    // Volume settings (low, med, high).
byte volReset = 1;                    // Init volume setting (1 = medium).
byte volume;

byte intArray[4] = { 5, 10, 30, 0 };  // Interval settings (5s, 10s, 30s, off).
byte intvlReset = 3;                  // Init interval setting (off).
byte interval;

byte cntArray[4] = { 1, 2, 3, 0 };    // Counter settings (1, 2, 3, unlimited) times.
byte cntReset = 0;                    // Init counter setting (play 1 song).
byte count;

bool paused = false;                  // State of current song.

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
  file = EEPROM.read(fileAddr);
  mode = EEPROM.read(modeAddr) % modeCount;
  volume = EEPROM.read(volAddr) % volCount;
  interval = EEPROM.read(intAddr) % intvlCount;
  count = EEPROM.read(cntAddr) % cntCount;

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
  // to occur with large files, ie. audio books. Change fileStep to 
  // two to select only odd numbered files.
  DFPlayer.volume(0);
  
  DFPlayer.play(2);
  DFPlayer.stop();
  wait_ms(200);

  if (DFPlayer.readCurrentFileNumber() != 2)
    fileStep = 2;

  // If booting in book mode, decrement the file so that the
  // DFPlayer module will resume on the same chapter it was at
  // prior to shutdown.
  if (mode == Book) {
    file -= fileStep;
    if (file < 1)
      file = fileCount;
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

        paused = true;
      }

      // Set sleepTimeOut, used to reset count following lights out condition
      // or longer than normal button press.
      timer1 = millis() + sleepTimeOut;
      
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

      // Select command based on paused state. Do not resume play of last
      // song if this command was issued following a reset. Instead, play
      // the next song.
      if (paused and file > 0) 
        DFPlayer.start();
        
      else {
        DFPlayer.stop();
        wait_ms(100);

        // If in book mode, play next chapter. Otherwise pick random song.
        if (mode == Book) {
          playNext();
          
          // Save in EEPROM, to restore after next power cycle.
          EEPROM.update(fileAddr, file);
        }
        else
          playRandom();
      }

      // Start song playtime timer and reset playCount to 1 and pause state.
      timer2 = millis() + playTime;
      playCount = 1;
      paused = false;
            
      state = 99;
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
      mode = ++mode % modeCount; 

      /// Save in EEPROM, to restore after next power cycle.
      EEPROM.update(modeAddr, mode);
      
      // Provide user feedback of current play mode (full, 20s, book).
      displayStatus(mode); 

      // If entering book mode, go to chapter 1 and pause.
      if (mode == Book) {
        file = 1;

        DFPlayer.volume(0);
        DFPlayer.play(file);

        // Pause the player and reset the volume.
        DFPlayer.pause();
        wait_ms(100);
        DFPlayer.volume(volArray[volume]);

        // Save in EEPROM, to restore after next power cycle.
        EEPROM.update(fileAddr, file);

        // Set pause state.
        paused = true;
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
      }
     else {
       // Save the current file as the next previous file.
       temp = file;
       file = prevFile;
       prevFile = temp;

       DFPlayer.play(file);

      // Start song playtime timer.
      timer2 = millis() + playTime;
      }

      // Reset play count to 1 and pause state.
      playCount = 1;
      paused = false;
      
      state = 99;
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
    //  ..-  = Change interval time.

    // If no more input, adjust interval setting.
    if (millis() > timer1) {
      interval = ++interval % intvlCount;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.update(intAddr, interval);

      // Provide user feedback of interval time.
      displayStatus(interval);

      // Resume auto play to demonstrate new interval time.
      paused = false;
    
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
      volume = ++volume % volCount;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.update(volAddr, volume);

      // Provide user feedback of current volume level (low, med, high).
      displayStatus(volume); 
      DFPlayer.volume(volArray[volume]);

      state = 99;
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
    //  ...- = Count, used to determine how many times to play audio. 

    // If no more input, ignore and wait for button release.
    if (millis() > timer1) {
      count = ++count % cntCount;
      playCount = 0;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.update(cntAddr, count);

      // Provide user feedback of play count limit.
      displayStatus(count);
    
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
      file = 0;
      mode = modeReset;
      interval = intvlReset;
      volume = volReset;
      count = cntReset;
      playCount = 0;

      // Save in EEPROM, to restore after next power cycle.
      EEPROM.write(fileAddr, file);
      EEPROM.write(modeAddr, mode);
      EEPROM.write(volAddr, volume);
      EEPROM.write(intAddr, interval);
      EEPROM.write(cntAddr, count);

      // Provide user feedback of reset.
      displayStatus(0); 

      // Stop playing current file, and reset the volume.
      DFPlayer.stop();
      wait_ms(100);
      DFPlayer.volume(volArray[volume]);

      // Wait for play command before playing next song.
      paused = true;
      
      state = 99;
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

      // Reset playCount after waking up (lights just turned on), or longer
      // than normal button press.
      if (millis() > timer1) {
        playCount = 0;
        paused = false;
      }
        
      // Restart the delay interval timer.
      timer3 = millis() + (long) intArray[interval] * 1000;

      state = 99;
    }
      
    break; 

  case 99 : // Debounce delay following command input.
    // Add delay to complete last DFPlayer command.
    wait_ms(100);

    // Ignore input generated from low light flicker.
    timer1 = millis() + CmdTimeOut;

    do {
      // If button press, set command timeout timer and debounce.
      if (digitalRead(Button_switch) == ON) {
        wait_ms(debounce);
        timer1 = millis() + CmdTimeOut;
      }
    } while (millis() < timer1);   
      
    digitalWrite(LED_status, LED_on);
    state = 0;

    break;
  }

  // ------------------------------------------------------------------ 
  // Items to check and perform periodically, outside of state machine.
  // ------------------------------------------------------------------ 

  // Reset timer3 if song is playing.
  if (digitalRead(DFPlayer_busy) == ON)
    timer3 = millis() + (long) intArray[interval] * 1000;
      
  // Fade song after reaching time limit in Short mode.
  if (mode == Short)
  
    // If still playing song, fade volume after 20s.
    if (digitalRead(DFPlayer_busy) == ON and millis() > timer2) {
      digitalWrite(LED_status, LED_off);
      fadeVolumeDown(volArray[volume]);

      // Stop playing and reset the volume.
      DFPlayer.stop();
      wait_ms(100);
      DFPlayer.volume(volArray[volume]);
      wait_ms(100);

      paused = false;     
      digitalWrite(LED_status, LED_on);
    }
  
  // Auto play next song based on the current interval and count settings.
  // Do not auto play in book mode or currently in paused state.
  if (mode != Book and !paused and intArray[interval] !=0) 
  
    // If no button is pressed and in state 0...
    if (digitalRead(Button_switch) == OFF and state == 0)
      
      // Play next song after reaching interval, then set playTime.
      if (digitalRead(DFPlayer_busy) == OFF and millis() > timer3)
      
        // Check the play counter to determine whether to play audio.
        if (cntArray[count] == 0 or playCount < cntArray[count]) {
          playRandom();
          playCount++;
          
          timer2 = millis() + playTime;
          paused = false;
        }
}

// ------------------------------------------------------------------ 
//  Functions
// ------------------------------------------------------------------ 

void wait_ms(int Time) {
  int unsigned long waitTimer = millis() + Time;

  while(millis() < waitTimer);
}

void playRandom() {
  // Play a random file, with adjustment when only odd files are accepted
  // on the DFPlayer Mini module. Don;t play the same file twice in a row.
  
  wait_ms(100);
  prevFile = file;
    
  do {
    file = randomNumber(fileCount) + 1;

    if (fileStep == 2 and file % 2 == 0)
      file++;
      
  } while (file == prevFile or file > fileCount);

  // If the player can't play the file, try again.
  do {
    DFPlayer.play(file);
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void playNext() {
  // Play the next audio file from the Micro SD card.

  wait_ms(100);

  file += fileStep;

  // Make sure the file id doesn't go out of bounds.
  if (file > fileCount)
    file = 1;

  // If the player can't play the file, try again.
  do {
    DFPlayer.play(file);
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void playPrev() {
  // Play the previous audio file from the Micro SD card.

  wait_ms(100);   

  // Make sure the file id doesn't go out of bounds.
  if (file == 1)
    file = fileCount - fileStep + 1;
  else
    file -= fileStep;

  // If the player can't play the file, try again.
  do {
    DFPlayer.play(file); 
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void fadeVolumeDown(byte maxVolume) {
  // Fade the volume down from the current max to 0, within fadeTime.

  int slice;
  slice = (fadeTime / maxVolume) * 10;
  
  for (int vol=maxVolume; vol >= 0; vol--) {
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
