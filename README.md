# tbsense2scratch3

Firmware for Thunderboard Sense 2 that collaborates with the corresponding Scratch 3 extension

The basic idea was to make Silicon Labs' Thunderboard Sense 2 usable in Scratch 3 just like micro:bit. First, I tried to find the source of Scratch's firmware for micro:bit however, I was unable to find it - thus I "reverse engineered" it from the Scratch 3 JavaScript extension.

The current implementation mimics Scratch's micro:bit firmware operation. There are two characteristics:

- `rx`: read and notify properties enabled, all values passed to the client in an `uint8_t` array
- `tx`: write property enabled, starts with a command byte followed by the function-specific data

Currently, LED color (brightness) and LED state (on/off) as control and X/Y acceleration (tilt), temperature and humidity sensor, and two buttons as sensing are supported.

To compile the project Simplicity Studio v5 (with Bluetooth SDK) should be installed. Clone/download the repo and import in Simplicity Studio. Open the .slcp file in the project root (the SDK selector may appear - choose the installed one) then click on *Force Generation* on the *Overview* tab before compile. If the compilation went OK burn it to Thunderboard Sense 2 (BRD4166A).

You need to add [Scratch 3 Thunderboard Sense 2 extension](https://github.com/sza2/scratch3_tbsense2) to Scratch to make the board work in Scratch 3. Note: the release versions must match.

Additionally, you need to run [Scratch Link](https://en.scratch-wiki.info/wiki/Scratch_Link), the WebSocket <-> Bluetooth proxy to access Bluetooth devices from Scratch 3. Since my preference is Linux I used [scratch_link.py](https://github.com/kawasaki/pyscrlink) and not the official Scratch Link.

Note: this project is independent of Silicon Labs and Scratch not officially supported by any of these organizations.