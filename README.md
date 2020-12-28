# UniPaste - CAN-bus enabled steppermotor controller
 
 If you would like to support the project, some caffeine is much appreciated :)
 
 <a href="https://www.buymeacoffee.com/marcosprojects" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" width="175" ></a>
 
 ## Hardware capabilites
 
 <p float="left"><img src="images/UniPaste_front.jpg" width="270"> <img src="images/UniPaste_back.jpg" width="270"></p>
 
 UniPaste is essentially a one axis universal steppermotor controller, relying on a Trinamic TMC2209 based stepper driver.
 
 The hardware is based around an ESP32-WROOM microcontroller module, giving UniPaste a blazing fast 2-core 240MHz brain with integrated Wifi and Bluetooth-LE capabilities.
 Additionally, UniPaste has an SN65HVD230 CAN-transceiver IC onboard, which allows for a fast and reliable way of communication with the controller.
 
 The steppermotor and microcontroller are supplied by a single supply voltage ranging between 9V and 24V, which will be suitable for most applications.
 
 The ESP32 microcontroller is flashed with new software using the easily accessible serial interface with hardware flow-control capabilites and auto-reset/auto-program circuitry.
 
 
 ## Software capabilities
 
 A UniPaste board comes pre-flashed with the latest version of the UniPaste firmware, which allows to get started immediately.
 
 Support for WiFiManager and ArduinoOTA makes connecting UniPaste to your local WiFi network super easy and ArduinoOTA in turn enables updating the firmware without even needing to use the hardware serial interface.
 
 Since the software is still in heavy development at this time, here's a usually up-to-date ToDo list:
 
 - [x] ESP_WiFiManager
 - [x] ArduinoOTA
 - [x] TMC2209 Standalone mode with UART configuration
 - [ ] TMC2209 Full UART control
 - [x] ESP32 TWAI based CAN-bus communication
 - [ ] Basic GCode interpreter
 - [ ] UniPaste setup/configuration via WiFi
 - [ ] WiFi/BT-based wireless control
 
 
 ## CAN communication
 
CAN communication is handled by the ESP32-internal SJA1000 CAN controller, which is CAN2.0B compatible with a fixed baudrate of 500kbit/s. The Node-ID of your UniPaste controller is set using the provided 5-bits of solder jumpers.

At this point in time, the following CAN messages containing a supported GCode command are being handled:

| Command | Description |
| ------- | ----------- |
| G0 A\<pos\> F\<speed\> | Add a linear move to the queue to be performed after all previous moves are completed. |
| G90 | Absolute Positioning - In absolute mode all coordinates given in G-code are interpreted as positions in the logical coordinate space. |
| G91 | Relative Positioning - In this mode all coordinates are interpreted as relative to the last position. |
| G92 A\<pos\> | Set the current position to the value specified. |
| M92 A\<steps\> | Set the number of full steps/mm for your setup |
| M114 | Get the current position |
| M503 | Report settings |
| M906 A\<mA\> | Set TMC motor current in mA |
 
 
 ## Library dependencies
 
* ESP_WiFiManager by [khoih-prog on GitHub](https://github.com/khoih-prog/ESP_WiFiManager)
* LittleFS by [lorol on GitHub](https://github.com/lorol/LITTLEFS)
* ArduinoOTA
* TMCStepper by [teemuatlut on GitHub](https://github.com/teemuatlut/TMCStepper)
* AccelStepper by [Mike McCauley](https://www.airspayce.com/mikem/arduino/AccelStepper/)
* ESP32-Arduino-CAN by [miwagner on GitHub](https://github.com/miwagner/ESP32-Arduino-CAN)
