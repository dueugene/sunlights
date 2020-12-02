#include <iostream>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>

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
vector<HueLight> lights;
map<float, LightVals> schedule; 
shared_ptr<LinHttpHandler> handler = make_shared<LinHttpHandler>();

// declare functions
json get_weather();
bool initialize_hue();
bool person_detected();
LightVals prescribe_light_setting(float, const map<float, LightVals>&);
LightVals interpolate_light_vals(const LightVals&, const LightVals&, float, float, float);

int main() {
  // perform initialization
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
  for (auto it = schedule.begin(); it != schedule.end(); it++) {
    cout << it->first << ": " << it->second.x << it->second.y << it->second.bri << it->second.on << endl;
  }
  config_file.close();
  // test the weather api
  json j = get_weather();
  cout << "Sunrise: " << j["sys"]["sunrise"] << endl;
  cout << "Sunset: " << j["sys"]["sunset"] << endl;
  // connect to the hue bridge
  initialize_hue();
  cout << "Lights:" << endl;
  for (int i = 0; i < lights.size(); ++i) {
    cout << i << ": " << lights[i].getName() << endl;
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
  while (person_detected()) {
    // get the current linux "epoch time", since sunrise and sunset are specified relative to this time
    // stack exchange says the c standard does not gurantee time(0) represents the begining of epoch time,
    // however I have manually confirmed it works on the raspberry pi
    time_t curr_time = time(0);
    if (curr_time > sunset) {
      // update the sunrise and sunset
      // this is not the most efficient, as sunrise and sunset will not update until we reach the next day
      // so this will keep calling the api unecessarily, however it works for now
      json j = get_weather();
      sunrise = j["sys"]["sunrise"];
      sunset = j["sys"]["sunset"];
    }
    // check the last command
		
    
    // get the light settings, and prescribe light settings
    float curr_time_f = static_cast<float>((curr_time - sunrise)) / (sunset - sunrise);
    LightVals v = prescribe_light_setting(curr_time_f, schedule);
    if (v.on) {
      if (!lights[0].isOn())
        lights[0].On();
      if (!lights[1].isOn())
        lights[1].On();
      if (!lights[2].isOn())
        lights[2].On();
      lights[0].setColorXY(v.x, v.y);
      lights[1].setColorXY(v.x, v.y);
      lights[2].setColorXY(v.x, v.y);
      lights[0].setBrightness(v.bri);
      lights[1].setBrightness(v.bri);
      lights[2].setBrightness(v.bri);
    } else {
      lights[0].Off();
      lights[1].Off();
      lights[2].Off();
    }
   
    cout << curr_time << " " << curr_time_f << " " << v.on << endl;
    sleep(15);
  }
  
 wait:
  lights[0].Off();
  lights[1].Off();
  lights[2].Off();
  while (!person_detected()) {
    sleep(1.5);
  }
  
  goto loop;
  
  return 0;
}

json get_weather() {
  // send an http request to the weather api
  // uses the LinHttpHandler already implemented inside hueplusplus
  string url = "api.openweathermap.org";
  string msg = "http://" + url + "/data/2.5/weather?q=" + weather_city + "&units=metric&appid=" + weather_id;
  json j_empty;
  return handler->GETJson(msg, j_empty, url);
}

bool initialize_hue() {
  // finds the hue bridge, and intializes the variables which store the lights
  HueFinder finder(handler);
  vector<HueFinder::HueIdentification> bridges = finder.FindBridges();
  if (bridges.empty()) {
    return false;
  } else {
    bridge = make_shared<Hue>(bridges[0].ip, bridges[0].port, hue_id, handler);
    cout << "bridge found with ip address: " << bridge->getBridgeIP() << endl;
    // populate lights vector, (manually configured to the setup of my room)
    lights.push_back(bridge->getLight(1));
    lights.push_back(bridge->getLight(2));
    lights.push_back(bridge->getLight(3));
  }
  return true;  
}

bool person_detected() {
  return true;
}

LightVals prescribe_light_setting(float curr_time_f, const map<float, LightVals>& schedule) {
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
