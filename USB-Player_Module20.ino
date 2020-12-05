/*
 Program: USB Micro SD Player Module
    Date: 04-Dec-2020
Revision: 20
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
 ..-  = Change interval (5s, 10s, 20s) in autoplay mode.
 
  Module Status (via LED):

  Blinking = Error, unable to read files on Micro SD card, or the last
             power cycle was too short (less than 10 sec).
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

- Simplifid menu structure from 7 commands down to 6.

- Fixed bug with book mode, where multiple requests were required to advance 
  to the next chapter.mp3 file, probably due to their large size. This was 
  not observed when reading smaller than 1MB.
  
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

unsigned long timer1;                 // Time since last button press/release.
unsigned long timer2;                 // Time playing audio file.
unsigned long timer3 = 0;             // Time since playing last audio file.
unsigned long playTime = 20000;       // 20 sec playtime.
unsigned long CmdTimeOut = 500;       // Command time out in ms.
byte fadeTime = 3000;                 // 3 sec audio fade.
byte debounce = 20;                   // 20 ms debounce for switch.

byte fileCount;                       // Number of audio files on Micro SD card.
byte state = 0;                       // State machine variable.

byte mode = 0;                        // Play mode (0 = full, 20s, auto, book).

byte volArray[3] = { 15, 20, 25 };    // Volume settings (low, med, high).
byte volume = 1;                      // Init volume setting (1 = medium).

byte intArray[5] = {5, 10, 20};       // Interval settings (5s, 10s, 20s).
byte interval = 0;                    // Init interval setting (30s);

bool paused = false;                  // State of current song.

SoftwareSerial SoftSerial(DFPlayer_Rx, DFPlayer_Tx);
DFRobotDFPlayerMini DFPlayer;

void setup()
{ 
  pinMode(LED_status, OUTPUT);
  pinMode(Button_switch, INPUT);
  pinMode(DFPlayer_busy, INPUT);

  // Turn on status LED to indicate ATTINY is running.
  digitalWrite(LED_status, LED_on);
  
  // Read seed stored in eeprom to get new series of pseudo random #s.
  unsigned int seed = EEPROM.read(0);
  randomSeed(seed);

  // Save new seed in eeprom, to use during next power on.
  EEPROM.write(0, seed + random(10));
  
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
  DFPlayer.volume(volArray[volume]);
  fileCount = DFPlayer.readFileCounts();

  // Indicate DFPlayer configuration has completed successfully.
  digitalWrite(LED_status, LED_on);
}

void loop()
{
  // Use state machine to process user input based on number of rapid
  // button presses. Command is determined to be complete following a
  // 1 sec pause.
  
  switch (state) {   
  case 0 : // No button press.
    // This is the default state. If a button press is detected, turn
    // off status LED and advance to the next state.
  
    // If button press, set hold time and advance to next state.
    if (digitalRead(Button_switch) == ON) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      
      digitalWrite(LED_status, LED_off);
      state = 1;
    }
    break;

  case 1 : // 1st button press.
    // If this is a long press, pause the song and return
    // to state 0. Otherwise, proceed to the next state.
  
    // If long press, fade, pause song, and wait for button 
    // release.
    if (millis() > timer1) {

      // If song is playing, fade volume.
      if (digitalRead(DFPlayer_busy) == ON) {
        fadeVolumeDown(volArray[volume]);

        // Pause the player and reset the volume.
        DFPlayer.pause();
        DFPlayer.volume(volArray[volume]); 
        
        paused = true;
      }

      // Set the autoplay delay interval timer.
      timer3 = millis() + intArray[interval] * 1000;
      
      state = 10;
      break; 
    }
          
    // If button released, set timer1 and advance to next state.
    if (digitalRead(Button_switch) == OFF) {
      wait_ms(debounce);
      timer1 = millis() + CmdTimeOut;
      state = 2;
    }
    break;

  case 2 : // 1st button release.
    // Play the next song and return to state 0. If another button 
    // press is detected, proceed to the next state.
      
    // If command timeout, play next song.
    if (millis() > timer1) {

      // Select command based on paused state.
      if (paused) 
        DFPlayer.start();
      else {
        DFPlayer.stop();

        // If in book mode, play next song. Otherwise pick random song.
        if (mode == 2)
          playNext();
        else
          playRandom();
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
    // If this is a long press, change play mode and return
    // to state 0. Otherwise, proceed to the next state.

    // If long press, change play mode.
    if (millis() > timer1) {
      mode = ++mode % 4; 

      // Provide user feedback of current play mode (full, 20s, auto, book).
      displayStatus(mode); 

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
    // Play the previous song and return to state 0. If another button press 
    // is detected, proceed to the next state.
  
    // If command timeout, change play mode.
    if (millis() > timer1) {
      DFPlayer.stop();
      playPrev();

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
      state = 5;
    }
    
    break;

  case 5 : // 3rd button press.
    // If this is a long press, change interval and return
    // to state 0. Otherwise, proceed to the next state.

    // If long press, change interval and wait for button release.
    if (millis() > timer1) {
      interval = ++interval % 3; 

      // Provide user feedback of interval time.
      displayStatus(interval);

      // Update the current song interval timer.
      timer3 = millis() + intArray[interval] * 1000;
      
      digitalWrite(LED_status, LED_on);
      state = 0;
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
    // Change the volume settings, and return to state 0.
  
    // If command timeout, change the volume settings.
    if (millis() > timer1) {
      volume = ++volume % 3;

      // Provide user feedback of current volume level (low, med, high).
      displayStatus(volume); 
      DFPlayer.volume(volArray[volume]);

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
      
      digitalWrite(LED_status, LED_on);
      state = 0;
    }
      
    break; 
  }

  // If autoplay mode and no button pressed...
  if (mode == 3 and digitalRead(Button_switch) == OFF) {
          
    // Play next song after reaching interval, then set playTime.
    if (digitalRead(DFPlayer_busy) == OFF and millis() > timer3) {
      playRandom();
      timer2 = millis() + playTime;
    }

    // Initialize timer3 after song ends, and reset paused status.
    if (digitalRead(DFPlayer_busy) == ON) {
      timer3 = millis() + intArray[interval] * 1000;
      paused = false;
    }
  }
  
  // If playing song in 20s mode and song still playing...
  if (mode == 1 and digitalRead(DFPlayer_busy) == ON)
  
    // Fade volume after 20s.
    if (millis() > timer2) {
      digitalWrite(LED_status, LED_off);
      fadeVolumeDown(volArray[volume]);

      // Stop playing and reset the volume.
      DFPlayer.stop();
      DFPlayer.volume(volArray[volume]); 
      
      digitalWrite(LED_status, LED_on);
    }
}

void wait_ms(int Time) {
  int unsigned long waitTimer = millis() + Time;

  while(millis() < waitTimer);
}

void playRandom() {
  // If the player can't play the file, get a new one and try again.
 
  do {
    DFPlayer.play(randomNumber(fileCount) + 1);
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void playNext() {
  // If the player can't play the file, try again.
 
  do {
    DFPlayer.next();
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void playPrev() {
  // If the player can't play the file, try again.
 
  do {
    DFPlayer.previous();
    wait_ms(100);
  } while (digitalRead(DFPlayer_busy) == OFF);
}

void fadeVolumeDown(byte maxVolume) {
  // Fade the volume down from the current max to 0, within fadeTime.

  int slice;
  slice = (fadeTime / maxVolume) * 10;
  
  for (int vol=maxVolume; vol > 0; vol--) {
    wait_ms(slice);
    DFPlayer.volume(vol);
  }
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

