#include <iostream>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>

#include <hueplusplus/Hue.h>
#include <hueplusplus/LinHttpHandler.h>
#include <hueplusplus/json/json.hpp>

#include <wiringPi.h>

using namespace std;
using json = nlohmann::json;

struct LightVals {
  float x;
  float y;
  int bri;
  bool on;
};

// declare global variables
string weather_id, weather_city, hue_id;
shared_ptr<Hue> bridge(nullptr);
vector<reference_wrapper<HueLight>> lights;
map<float, LightVals> schedule; 
shared_ptr<LinHttpHandler> handler = make_shared<LinHttpHandler>();

// declare functions
json get_weather();
bool initialize();
bool initialize_hue();
bool person_detected();
LightVals get_light_setting(float, const map<float, LightVals>&);
LightVals interpolate_light_vals(const LightVals&, const LightVals&, float, float, float);
bool set_light_vals(const LightVals& v);

int main() {
 initialize:
  // perform initialization
  // consists of 4 steps
  // 1. read input configuration file
  // 2. confirm succseful reply from weather api
  // 3. connect to hue bridge on local network, and retrieve lights
  // 4. initialize sensors connected to the pi
  cout << "Initializing ..." << endl;
  
  // load parameters from configuration file
  ifstream config_file;
  config_file.open("../config.txt");
  config_file >> hue_id;
  config_file >> weather_city;
  config_file >> weather_id;
  int n;
  config_file >> n;
  for (int i = 0; i < n; ++ i) {
    LightVals temp;
    float t;
    config_file >> t >> temp.x >> temp.y >> temp.bri >> temp.on;
    schedule[t] = temp;
  }
  cout << "lights schedule: " << endl;
  for (auto it = schedule.begin(); it != schedule.end(); it++) {
    cout << it->first << ": " << it->second.x << it->second.y << it->second.bri << it->second.on << endl;
  }
  config_file.close();
  
  // test the weather api
  json j = get_weather();
  if (!j.empty()) {
    cout << "Sunrise: " << j["sys"]["sunrise"] << endl;
    cout << "Sunset: " << j["sys"]["sunset"] << endl;
  } else {
    cout << "failed on getting weather" << endl;
    return 0;
  }
  
  // connect to the hue bridge
  if (initialize_hue() == false) {return 0;}
  cout << "Lights:" << endl;
  for (int i = 0; i < lights.size(); ++i) {
    cout << i << ": " << lights[i].get().getName() << endl;
  }
  
  // initialize sensors
  wiringPiSetup();
  const int led = 5;
  pinMode(led, OUTPUT);
  cout << "Intitialization successful" << endl << endl;

  goto loop;
  
 loop:
  time_t sunrise = 0;
  time_t sunset = 0;
  time_t time_till_next_midnight = 0;
  while (person_detected()) {
    // get the current linux "epoch time", since sunrise and sunset are specified relative to this time
    // stack exchange says the c standard does not gurantee time(0) represents the begining of epoch time,
    // however I have manually confirmed it works on the raspberry pi
    time_t curr_time = time(0);
    if (curr_time > time_till_next_midnight) {
      // update the sunrise and sunset
      json j = get_weather();
      if (j.empty()) {
        // for now if getting the weather keeps failing, we just keep trying at the next iteration
        break;
      }
      sunrise = j["sys"]["sunrise"];
      sunset = j["sys"]["sunset"];
      
      // convert sunset to local time, and reset it to midnight
      struct tm *sunset_tm = localtime(&curr_time);
      sunset_tm->tm_sec = 0;
      sunset_tm->tm_min = 0;
      sunset_tm->tm_hour = 0;
      time_till_next_midnight = mktime(sunset_tm);
      // set the next midnight time, + 5 seconds
      time_till_next_midnight += 3600*24 + 5;
      cout << "Sunrise updated: " << put_time(localtime(&sunrise), "%x %X") << endl;
      cout << "Sunset updated: "  << put_time(localtime(&sunset), "%x %X") << endl;
      cout << "Next weather update: " << put_time(localtime(&time_till_next_midnight), "%x %X") << endl;
    }
    // check the last command
		
    
    // get the light settings, and prescribe light settings
    float curr_time_f = static_cast<float>((curr_time - sunrise)) / (sunset - sunrise);
    LightVals v = get_light_setting(curr_time_f, schedule);
    struct tm *timeinfo = localtime(&curr_time);
    cout << put_time(timeinfo, "%x %X") << " " << curr_time_f << " " << v.on << endl;
    set_light_vals(v);
   
    sleep(15);
  }
  
 wait:
  LightVals v;
  v.on = false;
  set_light_vals(v);
  while (!person_detected()) {
    sleep(1.5);
  }
  
  goto loop;

 end:
  return 0;
}

json get_weather() {
  // send an http request to the weather api
  // uses the LinHttpHandler already implemented inside hueplusplus
  string url = "api.openweathermap.org";
  string msg = "http://" + url + "/data/2.5/weather?q=" + weather_city + "&units=metric&appid=" + weather_id;
  json j_empty;
  json reply;
  try {
    reply = handler->GETJson(msg, j_empty, url);
  } catch (...) {
    cout << "exception happend in get_weather()" << endl;
    reply = j_empty;
  }
  return reply;
}

bool initialize_hue() {
  // finds the hue bridge, and intializes the variables which store the lights
  try {
    HueFinder finder(handler);
    vector<HueFinder::HueIdentification> bridges = finder.FindBridges();
    if (bridges.empty()) {
      return false;
    } else {
      bridge = make_shared<Hue>(bridges[0].ip, bridges[0].port, hue_id, handler);
      cout << "bridge found with ip address: " << bridge->getBridgeIP() << endl;
      // instead of manually populating lights, we use the getAllLights function
      // however this method wraps the lights inside std::reference_wrapper, which from my understanding
      // requires the explicit get() method to access the light
      lights = bridge->getAllLights();
    }
  } catch (...) {
    cout << "exception happenned in initialize_hue()" << endl;
    return false;
  }
  return true;
}

bool person_detected() {
  return true;
}

// get the current light settings, based on the current time of day, and the lights schedule
LightVals get_light_setting(float curr_time_f, const map<float, LightVals>& schedule) {
  // find the first value in schedule greator or equal to current time
  auto it = schedule.lower_bound(curr_time_f);
  if (it == schedule.begin()) {
    // extrapolate left
    return it->second;
  } else if (it == schedule.end()) {
    // extrapolate right
    return schedule.rbegin()->second;
  }
  // we have to interpolate values
  auto it_prev = prev(it);
  return interpolate_light_vals(it_prev->second, it->second, it_prev->first, it->first, curr_time_f);
}

// interpolate light values between two LightVals settings
LightVals interpolate_light_vals(const LightVals& l1, const LightVals& l2, float x1, float x2, float x_target) {
  LightVals result = l1;
  float slope = (l2.x - l1.x) / (x2 - x1);
  result.x = slope*(x_target - x1) + l1.x;
  slope = (l2.y - l1.y) / (x2 - x1);
  result.y = slope*(x_target - x1) + l1.y;
  slope = (l2.bri - l1.bri) / (x2 - x1);
  result.bri = static_cast<int>(slope*(x_target - x1)) + l1.bri;
  return result;
}

// send LightVals to hue bridge
bool set_light_vals(const LightVals& v) {
  try {
    if (v.on) {
      lights[0].get().setColorXY(v.x, v.y);
      lights[1].get().setColorXY(v.x, v.y);
      lights[2].get().setColorXY(v.x, v.y);
      lights[0].get().setBrightness(v.bri);
      lights[1].get().setBrightness(v.bri);
      lights[2].get().setBrightness(v.bri);
      if (!lights[0].get().isOn())
        lights[0].get().On();
      if (!lights[1].get().isOn())
        lights[1].get().On();
      if (!lights[2].get().isOn())
        lights[2].get().On();
    } else {
      lights[0].get().Off();
      lights[1].get().Off();
      lights[2].get().Off();
    }
  } catch (...) {
    cout << "exception occured in set_light_vals()" << endl;
    return false;
  }
  return true;
}
