# Hardware
Hardware is an ESP32-based 4-relay module. This particular one has an integrated power source with an AC-DC converter.




This solution, however, is compatible with any hardware that is ESP32-based, that uses:
- 4 separate GPIOs to control relays, one GPIO per relay, binary logic (1 is on, 0 is off)
- has a separate LED to present the state (also binary controlled).

# Software
Software works the way that it exposes an access point (WiFi) that you can connect to. The default access code is 12345678.
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

Once successfully connected to the WiFi and MQTT broker, the device subscribes to 4 topics as configured. It awaits a text as a payload, containing one of the following strings:
- ON (case sensitive) to switch the relay on
- OFF (case sensitive) to switch it off

Rapidly developed using ChatGPT.
