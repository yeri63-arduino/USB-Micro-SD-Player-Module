# USB-Micro-SD-Player-Module
A compact, USB powered, micro SD audio player module.

Features:
 - User input available using either photocell or push button switch.
 - Module status (Error, Ready, Play Mode, ect.) via onboard LED.
 - User settings are saved between power cycles, with option to reset.
 - Powered by standard Micro USB 1.0 A phone charger.
 - Store up to 128 MP3 files on Micro SD (32G max).
 - Emulate external SD drive when connected to computer using USB cable.
 - Programmable controller (ATTiny85) using ISP port.
 
Module accepts commands by waving hand over the photocell, or pressing push button switch (depending on module configuration).

Key: (.) = Short Press, (-) = Long Press, (*) = default
| Input | Command |
| :---: | :--- |
| . | Play next song or chapter. |
| - | Stop playing song or chapter, and fade to silence. |
| .. | Play previous chapter (book mode only). |
| .- | Change play mode (full song*, 20 sec, auto, book). |
| ... | Change volume (low, med*, high) in all modes. |
| ..- | Change interval (5 sec, 10 sec*, 20 sec) used in auto play mode. |
| .... | Reset user settings (play mode, volume, interval and chapter). |

Module status and user settings are displayed via an onboard Status LED.

| Pattern | Description |
| :---: | :--- |
| Blinking | Error, unable to read Micro SD card. |
| Steady On | Ready for new command, or playing audio. |
| Steady Off | Processing last command, or sleeping. |
| n blinks | Used to show current setting or completion of last command. |

The blink code provides the results from the last command entered. Locate the column of the command entered, and then move down to the row matching the number of blinks observed to see what information was provided.

| n | Play Mode | Volume | Interval | Reset |
| :---: | :---: |  :---: | :---: | :---: |
| 1 | full song | low | 5 sec | ok |
| 2 | 20 sec | med | 10 sec | - |
| 3 | auto | high | 30 sec | - |
| 4 | book | - | 60 sec | - |
