# USB-Micro-SD-Player-Module
A compact, USB powered, micro SD audio player module.

MP3-Player using DFPlayer mini, ATtiny85, USB, photoresistor or push button switch, potentiometer, and status LED. The 3 x 3.5cm module comes with the following features.

 - User input available using either photocell or push button switch.
 - Module status (Error, Ready, Play Mode, ect.) via onboard LED.
 - Powered by standard Micro USB 1.0 A phone charger.
 - Read up to 32GB Micro SD with MP3 songs/audio clips.
 - Emulates external SD drive when connected to computer using USB cable.
 - Arduino programming of ATTINY available using ISP port.
 
Module commands are entered by waving hand over the photocell, or pressing push button switch. Only one switching option can be installed. A command is considered complete if no addditional input is received within 500 ms. 

Note: A short press or shadow over the photocell is anything less than 500 ms. A long press is anything greater.

Key . = Short Press, - = Long Press, * = default
 - .    = Play next song.
 - -    = Stop playing song, and fade to silence.
 - ..   = Play previous song.
 - .-   = Change play mode (full song, 20 sec, book, auto).
 - ...  = Change volume (low, med*, high) in single and 20 sec mode.
 - ..-  = Change interval (5s, 10s, 20s) in autoplay mode.
 
 Module Status (via LED):
 - Blinking = Error, unable to read files on Micro SD card, or the last power cycle was too short (less than 10 sec).
 - On       = Ready for new command, or playing song.
 - Off      = Processing last command, or sleeping.
 - n blinks = Current setting of play mode, volume, or interval.
