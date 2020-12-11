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
* incorporate IR sensor to detect when person is present in room or not
* add manual mode, when control of the lights is given to the user

## how to use config file
The program starts by reading a config file config.txt, which looks like the following:
```C++
hue id           // unique id used to authenticate communication with the hue bridge
city             // city to get the sunrise and sunset information, eg. Calgary
weather id       // unique id used for communication with the weather api, you need to create an account with them
2                // number of light schedules to load
../schedule1.sch // path to first light schedule
../schedule2.sh  // path to second light schedule
0                // attach schedule1 to light[0]
1                // attach schedule2 to light[1]
0                // attach schedule1 to light[2]
1                // and so on
```

The lights schedule is configured in the following way:
```C++
n    // number of points
time  colorX  colorY  brightness   on/off
```
n specifies the number of points in the schedule, time is the relative time of day (0 being sunrise, 1 being sunset), colorX and colorY are floats representing the color, and brightness is an int from 0-254 representing the brightness, and on/off is boolean value. See the file `default.sch` as an example.