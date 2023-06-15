#include <Arduino.h>

#define physical_clock
#define chain

const char device[7] = "chain";
const char smart_prefix = 'c';
const uint8_t version = 3;

const int bipolar_enable_pin = D6;
const int bipolar_direction_pin = D5;
const int bipolar_step_pin = D3;

const int default_tilt = 10;
int tilt = default_tilt;

int steps = 0;
int destination = 0;
int actual = 0;

bool measurement = false;

String toPercentages(int value, int steps);
int toSteps(int value, int steps);
bool readSettings(bool backup);
void saveSettings();
void saveSettings(bool log);
void resume();
void saveTheState();
String getSteps();
String getValue();
String getActual();
void startServices();
void handshake();
void requestForState();
void exchangeOfBasicData();
void readData(const String& payload, bool per_wifi);
void automation();
void smartAction();
void setMin();
void setMax();
void setAsMax();
void makeMeasurement();
void cancelMeasurement();
void endMeasurement();
void setStepperOff();
void prepareRotation(String orderer);
void calibration(int set, bool positioning);
void measurementRotation();
void rotation();
