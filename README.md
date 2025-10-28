# hivesight_sensor
## Configurations
This project is a submodule of a larger project. It relies on a set of shell environment variables prior to building. The listed variables are provided here, the larger project root, as well has in the `platform.ini` file.
```shell
# Baud rate to use for serial comm
CONFIG_BAUD_RATE

# Hardcoded WiFi SSID Name
CONFIG_WIFI_SSID

# Hardcoded WiFi Password. As this is a 
# prototype, this is in plaintext
CONFIG_WIFI_PASSWORD
```