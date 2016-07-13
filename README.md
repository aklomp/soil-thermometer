# WiFi-enabled soil thermometer

This repository contains the firmware for a home-made soil thermometer that my
father and I designed and built. A soil thermometer is a long rod that is
inserted into the ground, and measures soil temperature at various depths. If
you enter these measurements into a Fourier soil model, and assume that the
soil is uniform, you can extrapolate soil temperature to any depth, calibrated
by just one free constant. If you already know the temperature at various soil
depths, you can derive the constant experimentally, giving you a pretty
accurate model for soil temperature as a function of depth and seasonal
variation.

Our thermometer is just over 120 cm long and contains seven temperature sensors
spaced 20 cm apart. These sensors are steel-jacketed DS18B20 one-wire sensors,
which are powered up and sampled every 15 minutes by an ESP8266 microcontroller
on a NodeMCU board. The microcontroller checks the sensors for errors,
optionally consolidates multiple measurements, and transfers the data over WiFi
to a web server. (Currently, this is a Raspberry Pi on the house network which
runs some custom PHP scripts to store the data and display a graph.) The
Fourier model fitting is not done by this code or the web server, but
externally in an Excel spreadsheet.

The code contains provisions for a powersaving mode which is currently not
activated. In this mode, the ESP8266 polls the sensors every 15 minutes, but
only sends the data once an hour. The code can also average these measurements
to get a single representative temperature for the whole hour. These power
saving features became obsolete once my father installed a solar charging
system.

The code is written in C against the native non-RTOS API for the ESP8266,
specifically [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk). (I
started off writing the code in NodeMCU-flavoured Lua, but things quickly got
weird, flaky, and short on memory.) Necessarily, a number of parameters had to
be hardcoded, such as the sensor addresses, the web server's IP address, and
the wifi station and password. (These are inside a header file not included in
this repository, called `bin/secrets.h`.)

The code is probably not very general, and would have to be customized for any
other set of sensors. However, parts of it might be useful in other projects,
such as the OneWire driver and the DS18B20 driver code. The whole system is
event-driven by means of the Event API (posting event messages from various
submodules and catching them in the main loop), which I found to be an
interesting and effective design pattern for ESP8266 applications.

## License

All code in this repository is licensed under the GNU General Public License
v3.
