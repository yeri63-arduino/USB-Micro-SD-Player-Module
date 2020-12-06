# USB-Micro-SD-Player-Module
A compact, USB powered, micro SD audio player module.

Features:
 - User input available using either photocell or push button switch.
 - Module status (Error, Ready, Play Mode, ect.) via onboard LED.
 - Powered by standard Micro USB 1.0 A phone charger.
 - Read up to 32GB Micro SD with MP3 songs/audio clips.
 - Emulates external SD drive when connected to computer using USB cable.
 - Programmable controller (ATTiny85) using ISP port.
 
Module accepts commands by waving hand over the photocell, or pressing push button switch. Only one of these is installed. Program status and settings are displayed via an onboard Status LED, by blinking patterns.

| Input | Command |
| :---: | :--- |
| . | Play next song. |
| - | Stop playing song, and fade to silence. |
| .. | Play previous song. |
| .- | Change play mode (full song*, 20 sec, book, auto). |
| ... | Change volume (low, med*, high) in single and 20 sec mode. |
| ..- | Change interval (5s*, 10s, 20s) in autoplay mode. |
Key: (.) = Short Press, (-) = Long Press, (*) = default

Program Status (via LED):

| Pattern | Description |
| :---: | :--- |
| Blinking | Error, unable to read files on Micro SD card, or the last power cycle was too short (less than 10 sec). |
| Steady On | Ready for new command, or playing song. |
| Steady Off | Processing last command, or sleeping. |
| n blinks | Used to show current setting of play mode, volume, etc. |
