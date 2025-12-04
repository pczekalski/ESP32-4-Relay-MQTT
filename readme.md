# Hardware
Hardware is an ESP32-based 4-relay module. This particular one has an integrated power source with an AC-DC converter.

![ESP32 4-relay module](https://github.com/pczekalski/ESP32-4-Relay-MQTT/blob/main/Screenshot%20from%202025-12-04%2017-00-30.png)

This solution, however, is compatible with any hardware that is ESP32-based, that uses:
- 4 separate GPIOs to control relays, one GPIO per relay, binary logic (1 is on, 0 is off)
- has a separate LED to present the state (also binary controlled).

For this development board, the configuration is as follows:
- Relay's GPIOs (in order 1 to 4): 32, 33, 25, 26;
- LED GPIO: 23

Other development boards may require changes in the GPIO config (top of `main.cpp` file).

# Software
Software works by exposing an access point (WiFi) that you can connect to. The default access code is 12345678.
Once connected, open a browser at http://192.168.4.1 and configure:
- WiFi (SSID, pass) the module should connect to
- MQTT broker configuration: 
  - IP address
  - port (standard is 1883 for non-encrypted and 8883 for TLS)
  - user
  - password
  - connection via TLS/plain text switch
  - either to verify the certificate (not tested)
  - certificate itself (not tested)
- 4 MQTT topics (one per relay)

> [!WARNING]
> There are no validations, so mind what you configure!

Once the config is saved, the device reboots and tries to connect to the WiFi AP using the provided credentials. If it fails for any reason, it will fall back to AP mode so you can correct your configuration again.
It is also possible to access the configuration panel when in WiFI client mode - check the IP address the DHCP server gave the device and point your browser there.

> [!NOTE]
> Just so you know, the current version does not distinguish between WiFi-related and MQTT-related fallbacks.

Once successfully connected to the WiFi and MQTT broker, the device subscribes to 4 topics as configured. It awaits a text as a payload, containing one of the following strings:
- ON (case sensitive) to switch the relay on
- OFF (case sensitive) to switch it off

## Development environment
I've suggested using the Visual Studio Code + PlatformIO plugin. `platformio.ini` is pre-configured but may require adjustments to the `upload_port` and `upload_speed` parameters, depending on your hardware and OS.

# Uploading (flashing) procedure
To use this particular development board, you need an external programmer (USB to serial converter) and also solder a 6-pin header (to the left of the ESP32 module, vertical): connect GND to GND, RX to TX, and TX to RX, eventually power from 5V if only needed (converter is usually powered by PC).
This video demonstrates how to connect it (https://www.youtube.com/shorts/Wqyj7O200z8). Anything else is the standard platformio flashing procedure.

Rapidly developed using ChatGPT.
