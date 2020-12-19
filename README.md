# tbsense2scratch3

Firmware for Thunderboard Sense 2 tha collaborates with the corresponding Scratch 3 extension

The basic idea was to make Silicon Labs' Thunderboard Sense 2 usable in Scratch 3 just like micro:bit. First, I tired to find the source of Scratch's firmware for micro:bit however, I was unable to find it - thus I "reverse engineered" it from the Scratch 3 JavaScript extension.

The current implementation mimics Scratch's micro:bit firmware operation. There are two characteristics:

- `rx`: read and notify properties enabled, all values passed to the client in an `uint8_t` array
- `tx`: write property enabled, starts with a command byte followed by the function specific data

Currently, LED color (brightness) and LED state (on/off) as control and X/Y acceleration (tilt) and two buttons as sensing supported.

To compile the project Simplicity Studio v5 should be installed. Clone/download the repo and import in Simplicity Studio. If the compilation went OK burn it to Thunderboard Sense 2 (BRD4166A).

You need to add [Scratch 3 Thunderboard Sense 2 extension](https://github.com/sza2/scratch3_tbsense2) to Scratch to make the board work in Scratch 3.

Additionally you need to run [Scratch Link](https://en.scratch-wiki.info/wiki/Scratch_Link), the websocket <-> Bluetooth proxy to access Bluetooth devices from Scratch 3. Since my preference is Linux I used [scratch_link.py](https://github.com/kawasaki/pyscrlink) and not the official Scratch Link. ~~At the time of writing this document my pull request is not accepted so, probably you better off to go with my fork: https://github.com/sza2/pyscrlink.~~ My pull request was accepted in pyscrlink.

Note: this project is independent from Silicon Labs and Scratch not officially supported by any of these organizations.
