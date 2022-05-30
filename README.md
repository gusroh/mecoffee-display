# meCoffee-Display
Arduino Display for [meCoffee](https://mecoffee.nl/) PID (BLE version).

- Author: Gustav Rohdin
- Based on https://github.com/nglx/mecoffee-display

## Description
This project utilizes the BLE functionality and display of the Arduino ESP32 to display information from the meCoffee PID (BLE version). Available features are:
- Boiler temperature
- Shot timer
- Warmup timer
- Cleaning helper

The code has been tested with a TTGO T-Display (https://github.com/Xinyuan-LilyGO/TTGO-T-Display), meCoffee BLE PID and a Rancilio Silvia (V3) espresso machine.

## How To Use
### Prerequisites
This project depends on the following libraries:
- Button2 (https://github.com/LennartHennigs/Button2)
- TFT_eSPI (https://github.com/Bodmer/TFT_eSPI)

You will also need Arduino core for the ESP32 (https://github.com/espressif/arduino-esp32).

### Connecting to meCoffee PID
When the ESP32 starts it will automatically try to connect to the meCoffee PID. If it can't find a meCoffee PID it will go to sleep. Once connected the current boiler temperature will be displayed. Please note that the meCoffee PID can only be connected to one device at a time. If it is already connected to the meBarista/uBarista app on a phone or tablet it will need to be disconnected before the ESP32 can connect to the meCoffee PID.

### Boiler temperature
The current boiler temperature will be shown on the display as long as the ESP32 is connected to the meCoffee PID. If the current boiler temperature is within the acceptable range (&pm;0.5 &deg;C) of the requested temperature (meCoffee default 101 &deg;C for brew and 125 &deg;C for steam) the text color will be green (`TFT_GREEN`), otherwise it will be orange (`TFT_ORANGE`).

### Shot timer
The shot timer will be shown when the meCoffee PID detects that a brew has been started. The timer will start when the brew button has been pushed on the espresso machine.

### Warmup timer
When the ESP32 connects to the meCoffee PID it will check the current boiler temperature to determine if the boiler needs to warm up before pulling the first shot. The warmup feature is based on the following boiler temperatures and warmup times:
| Boiler temp | Warmup |
| -----------: | ------: |
| < 50 &deg;C | 15 min |
| < 70 &deg;C | 10 min |
| < 85 &deg;C | 5 min |
| &ge; 85 &deg;C | no |

When the warmup timer has finished the display will revert to showing the default shot timer (0 seconds). The warmup timer can be disabled by clicking `BUTTON2` on the ESP32 and re-enabled by clicking `BUTTON1`. The warmup check on startup can be disabled by changing the line `static boolean warmupEnabled = true;` to `static boolean warmupEnabled = false;` in the code. The warmup timer can still be accessed by clicking `BUTTON1` on the ESP32.

***Please note that the code doesn't take into account the actual temperature of the individual parts of the espresso machine (e.g. group head and portafilter), only the boiler temperature probe of the meCoffee PID. This means that it is possible to bypass the warmup timer by letting the espresso machine warm to 85 &deg;C or more and then turning it off and on again, whereupon the ESP32 will think that the espresso machine is warm enough already. The warmup timer should only be seen as a suggestion.***

### Cleaning helper
The cleaning helper is a visual aid for performing a backflush cleaning routine on the espresso machine. The cleaning helper is started by long clicking (> 2 seconds) on `BUTTON1` on the ESP32. The helper can be exited at any point by long clicking (> 2 seconds) on `BUTTON1` on the ESP32.

The cleaning routine is tested with the espresso machine cleaning agent "Puly Caff Plus" and is based on the instructions printed on the container. Other espresso machine cleaning agents should have similar routines. The cleaning routine assumes that you have added the required amount of cleaning agent to the blind filter in the portafilter and inserted the portafilter into the group head. The routine consists of the following steps:
1. Backflush/brew for 10 seconds.
2. Stop/wait for 10 seconds.
3. Repeat steps 1-2 for 5 times.
4. Remove the portafilter from the group head and and rinse it with boiling water from the brew head.
5. Insert the portafilter into the group head and repeat steps 1-3.

The cleaning helper will automatically exit when the cleaning routine has finished.

The backflush/stop time and the number of repetitions can be changed by changing the following lines in the code:
```
static int cleanReps = 5;
static int cleanTime = 10; // Seconds
```

### Turning off (sleep) the ESP32
The ESP32 can be turned off (actually just a deep sleep mode) by long clicking (> 3 seconds) on `BUTTON2` on the ESP32. This allows for another device to connect to the meCoffee PID. Press the `RST` button on the ESP32 to turn it back on (reset).
