# sunlights

A small raspberry pi project used to control the lights in my room.

It is intended to emulate the daily sunrise and sunset

It uses the open source [hueplusplus](https://github.com/enwi/hueplusplus) library to communicate with the hue lights, and the [OpenWeather](https://openweathermap.org/api) api to get the daily sunrise and sunset

## To compile:
```bash
mkdir build
cd build
cmake ..
```

## todo:
* handle exceptions when communication with bridge is unsuccessful
* incorporate IR sensor to detect when person is present in room or not
* add manual mode, when control of the lights is given to the user
