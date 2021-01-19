# USB-Micro-SD-Player-Module
A compact, USB powered, micro SD audio player module.

Features:
 - Multiple play modes (full, short, book).
 - Adjustable volume settings (low, med, high).
 - Intervalometer to control how often and how many songs are played.
 - User input available using either photocell or push button switch.
 - Read module status (Error, Ready, Play Mode, ect.) via onboard LED.
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
| .. | Play previous song or chapter. |
| .- | Change play mode (full*, short, book). |
| ... | Change volume (low, med*, high) in all modes. |
| ..- | Change interval (5 sec, 10 sec, 30 sec, off*) used in full and short modes. |
| .... | Reset user settings (play mode, volume, interval, count and chapter). |
| ...- | Change count (1*, 2, 3, unlimited) number of songs to play. |

Module status and user settings are displayed via an onboard Status LED.

| Pattern | Description |
| :---: | :--- |
| Blinking | Error, unable to read Micro SD card. |
| Steady On | Ready for new command, or playing audio. |
| Steady Off | Processing last command, or sleeping. |
| n blinks | Used to show current setting or completion of last command. |

The blink code provides the results from the last command entered. Locate the column of the command entered, and then move down to the row matching the number of blinks observed to see what information was provided.

| n | Play Mode | Volume | Interval | Count | Reset |
| :---: | :---: |  :---: | :---: | :---: | :---: |
| 1 | full | low | 5 sec | 1 | ok |
| 2 | short | med | 10 sec | 2 | - |
| 3 | book | high | 30 sec | 3 | - |
| 4 | - | - | off | Unlimited | - |
