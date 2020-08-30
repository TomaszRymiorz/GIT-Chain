#include <Arduino.h>
#include <avdweb_Switch.h>

const char device[7] = "chain";
const int version = 1;

const int bipolar_enable_pin = D6;
const int bipolar_direction_pin = D5;
const int bipolar_step_pin = D3;

int steps = 0;
int tilt = 10;

struct Smart {
  bool enabled;
  String days;
  int tilting_time;
  int opening_time;
  int closing_time;
  uint32_t access;
};

int destination = 0;
int actual = 0;

bool measurement = false;
float temperature = -127.0;

void setStepperOff();
String toPercentages(int value, int steps);
int toSteps(int value, int steps);
bool readSettings(bool backup);
void saveSettings();
void resume();
void saveTheState();
void sayHelloToTheServer();
void introductionToServer();
void startServices();
String getChainDetail();
void handshake();
void requestForState();
void exchangeOfBasicData();
void setMin();
void setMax();
void setAsMax();
void makeMeasurement();
void cancelMeasurement();
void endMeasurement();
void readData(String payload, bool per_wifi);
void setSmart();
bool automaticSettings();
bool automaticSettings(bool temperature_changed);
void prepareRotation();
void calibration(int set, bool bypass);
void measurementRotation();
void rotation();
