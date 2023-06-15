#include "core.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  #ifdef physical_clock
    rtc.begin();
    note("iDom Chain " + String(version) + "." + String(core_version));
  #else
    note("iDom Chain " + String(version) + "." + String(core_version) + "wo");
  #endif

  sprintf(host_name, "chain_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  if (!readSettings(0)) {
    delay(1000);
    readSettings(1);
  }
  resume();

  if (RTCisrunning()) {
    start_u_time = rtc.now().unixtime() - offset - (dst ? 3600 : 0);
  }

  pinMode(bipolar_enable_pin, OUTPUT);
  pinMode(bipolar_direction_pin, OUTPUT);
  pinMode(bipolar_step_pin, OUTPUT);
  setStepperOff();
  setupOTA();
  connectingToWifi(false);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (destination == actual) {
      ArduinoOTA.handle();
    }
    server.handleClient();
    MDNS.update();
  } else {
    if (!auto_reconnect) {
      connectingToWifi(true);
    }
    cancelMeasurement();
  }

  if (measurement) {
    measurementRotation();
    return;
  }

  if (hasTimeChanged()) {
    if (destination != actual) {
      if (loop_u_time % 2 == 0) {
        smartAction(5, false);
      } else {
        saveTheState();
        automation();
      }
    } else {
      automation();
    }
  }

  if (destination != actual) {
    rotation();
    if (destination == actual) {
      setStepperOff();
      if (LittleFS.exists("/resume.txt")) {
        LittleFS.remove("/resume.txt");
      }
    }
  }
}


String toPercentages(int value, int steps) {
  return String(value > 0 && steps > 0 ? (int)round((value + 0.0) * 100 / steps) : 0);
}

int toSteps(int value, int steps) {
  return value > 0 && steps > 0 ? round((value + 0.0) * steps / 100) : 0;
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, file);

  if (deserialization_error) {
    note(String(backup ? "Backup" : "Settings") + " error: " + String(deserialization_error.f_str()));
    file.close();
    return false;
  }

  file.seek(0);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + file.readString());
  file.close();

  if (json_object.containsKey("log")) {
    last_accessed_log = json_object["log"].as<int>();
  }
  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  dst = json_object.containsKey("dst");
  if (json_object.containsKey("smart")) {
    if (json_object.containsKey("ver")) {
      setSmart(json_object["smart"].as<String>());
    } else {
      setSmart(oldSmart2NewSmart(json_object["smart"].as<String>()));
    }
  }
  smart_lock = json_object.containsKey("smart_lock");
  if (json_object.containsKey("location")) {
    geo_location = json_object["location"].as<String>();
    if (geo_location.length() > 2) {
      sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
    }
  }
  if (json_object.containsKey("sunset")) {
    sunset_u_time = json_object["sunset"].as<int>();
  }
  if (json_object.containsKey("sunrise")) {
    sunrise_u_time = json_object["sunrise"].as<int>();
  }
  sensor_twilight = json_object.containsKey("sensor_twilight");
  calendar_twilight = json_object.containsKey("twilight");
  if (json_object.containsKey("tilt")) {
    tilt = json_object["tilt"].as<int>();
  }
  if (json_object.containsKey("steps")) {
    steps = json_object["steps"].as<int>();
  }
  if (json_object.containsKey("destination")) {
    destination = json_object["destination"].as<int>();
    if (destination < 0) {
      destination = 0;
    }
    if (destination > steps) {
      destination = steps;
    }
    actual = destination;
  }

  saveSettings(false);

  return true;
}

void saveSettings() {
  saveSettings(true);
}

void saveSettings(bool log) {
  DynamicJsonDocument json_object(1024);

  json_object["ver"] = String(version) + "." + String(core_version);
  if (last_accessed_log > 0) {
    json_object["log"] = last_accessed_log;
  }
  if (ssid.length() > 0) {
    json_object["ssid"] = ssid;
  }
  if (password.length() > 0) {
    json_object["password"] = password;
  }
  json_object["uprisings"] = uprisings;
  if (offset > 0) {
    json_object["offset"] = offset;
  }
  if (dst) {
    json_object["dst"] = dst;
  }
  if (smart_count > 0) {
    json_object["smart"] = getSmartString(true);
  }
  if (smart_lock) {
    json_object["smart_lock"] = smart_lock;
  }
  if (geo_location != default_location) {
    json_object["location"] = geo_location;
  }
  if (sunset_u_time > 0) {
    json_object["sunset"] = sunset_u_time;
  }
  if (sunrise_u_time > 0) {
    json_object["sunrise"] = sunrise_u_time;
  }
  if (sensor_twilight) {
    json_object["sensor_twilight"] = sensor_twilight;
  }
  if (calendar_twilight) {
    json_object["twilight"] = calendar_twilight;
  }
  if (tilt > default_tilt) {
    json_object["tilt"] = tilt;
  }
  if (steps > 0) {
    json_object["steps"] = steps;
  }
  if (destination > 0) {
    json_object["destination"] = destination;
  }

  if (writeObjectToFile("settings", json_object)) {
    if (log) {
      String log_text;
      serializeJson(json_object, log_text);
      note("Saving settings:\n " + log_text);
    }

    writeObjectToFile("backup", json_object);
  } else {
    note("Saving the settings failed!");
  }
}

void resume() {
  File file = LittleFS.open("/resume.txt", "r");
  if (!file) {
    return;
  }

  StaticJsonDocument<100> json_object;
  DeserializationError deserialization_error = deserializeJson(json_object, file);
  file.close();

  if (deserialization_error) {
    note("Resume error: " + String(deserialization_error.c_str()));
    return;
  }

  if (json_object.containsKey("actual")) {
    actual = json_object["actual"].as<int>();
  }

  if (destination != actual) {
    note("Resume: \n " + String(destination - actual) + " steps to " + toPercentages(destination, steps) + "%");
  } else {
    if (LittleFS.exists("/resume.txt")) {
      LittleFS.remove("/resume.txt");
    }
  }
}

void saveTheState() {
  StaticJsonDocument<100> json_object;

  json_object["actual"] = actual;

  writeObjectToFile("resume", json_object);
}


String getSteps() {
  return String(steps);
}

String getValue() {
  return toPercentages(destination, steps);
}

String getActual() {
  return toPercentages(actual, steps);
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/measurement/start", HTTP_POST, makeMeasurement);
  server.on("/measurement/cancel", HTTP_POST, cancelMeasurement);
  server.on("/measurement/end", HTTP_POST, endMeasurement);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/test/smartdetail", HTTP_GET, getSmartDetail);
  server.on("/test/smartdetail/raw", HTTP_GET, getRawSmartDetail);
  server.on("/admin/reset", HTTP_POST, setMin);
  server.on("/admin/setmax", HTTP_POST, setMax);
  server.on("/admin/setasmax", HTTP_POST, setAsMax);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.begin();

  note(String(host_name) + (MDNS.begin(host_name) ? " started" : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  ntpClient.begin();
  ntpClient.update();
  readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);
  getOfflineData();
}

void handshake() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"id\":\"" + WiFi.macAddress() + "\"";
  reply += ",\"version\":" + String(version) + "." + String(core_version);
  reply += ",\"offline\":true";
  if (keep_log) {
    reply += ",\"last_accessed_log\":" + String(last_accessed_log);
  }
  if (start_u_time > 0) {
    reply += ",\"start\":" + String(start_u_time);
  } else {
    reply += ",\"active\":" + String(millis() / 1000);
  }
  reply += ",\"uprisings\":" + String(uprisings);
  if (offset > 0) {
    reply += ",\"offset\":" + String(offset);
  }
  if (dst) {
    reply += ",\"dst\":true";
  }
  if (RTCisrunning()) {
    #ifdef physical_clock
      reply += ",\"rtc\":true";
    #endif
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }
  if (smart_count > 0) {
    reply += ",\"smart\":\"" + getSmartString(true) + "\"";
  }
  if (smart_lock) {
    reply += ",\"smart_lock\":true";
  }
  if (geo_location.length() > 2) {
    reply += ",\"location\":\"" + geo_location + "\"";
  }
  if (last_sun_check > -1) {
    reply += ",\"sun_check\":" + String(last_sun_check);
  }
  if (next_sunset > -1) {
    reply += ",\"next_sunset\":" + String(next_sunset);
  }
  if (next_sunrise > -1) {
    reply += ",\"next_sunrise\":" + String(next_sunrise);
  }
  if (sunset_u_time > 0) {
    reply += ",\"sunset\":" + String(sunset_u_time);
  }
  if (sunrise_u_time > 0) {
    reply += ",\"sunrise\":" + String(sunrise_u_time);
  }
  if (calendar_twilight) {
    reply += ",\"twilight\":true";
  }
  if (tilt > 0) {
    reply += ",\"tilt\":" + String(tilt);
  }
  if (steps > 0) {
    reply += ",\"steps\":" + String(steps);
  }
  if (destination > 0) {
    reply += ",\"value\":" + getValue();
  }
  if (actual > 0) {
    reply += ",\"pos\":" + actual;
  }

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"value\":[" + getValue() + "]";

  if (!measurement && !(getActual() == "0" || getActual() == getValue())) {
    reply += ",\"pos\":[" + getActual() + "]";
  }

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  if (server.hasArg("plain")) {
    readData(server.arg("plain"), true);
  }

  String reply = "\"ip\":\"" + WiFi.localIP().toString() + "\"" + ",\"id\":\"" + WiFi.macAddress() + "\"";

  reply += ",\"offset\":" + String(offset) + ",\"dst\":" + String(dst);

  if (RTCisrunning()) {
    reply += ",\"time\":" + String(rtc.now().unixtime() - offset - (dst ? 3600 : 0));
  }

  server.send(200, "text/plain", "{" + reply + "}");
}

void readData(const String& payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  DeserializationError deserialization_error = deserializeJson(json_object, payload);

  if (deserialization_error) {
    note("Read data error: " + String(deserialization_error.c_str()) + "\n" + payload);
    return;
  }

  if (json_object.containsKey("calibrate")) {
    calibration(json_object["calibrate"].as<int>(), json_object.containsKey("positioning"));
    return;
  }

  bool settings_change = false;
  bool twilight_change = false;

  if (json_object.containsKey("ip") && json_object.containsKey("id")) {
      for (int i = 0; i < devices_count; i++) {
        if (devices_array[i].ip == json_object["ip"].as<String>()) {
          devices_array[i].mac = json_object["id"].as<String>();
        }
      }
  }

  if (json_object.containsKey("offset")) {
    if (offset != json_object["offset"].as<int>()) {
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime((rtc.now().unixtime() - offset) + json_object["offset"].as<int>()));
        note("Time zone change");
      }
      offset = json_object["offset"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    if (dst != strContains(json_object["dst"].as<String>(), 1)) {
      dst = !dst;
      settings_change = true;
      if (RTCisrunning() && !json_object.containsKey("time")) {
        rtc.adjust(DateTime(rtc.now().unixtime() + (dst ? 3600 : -3600)));
        note(dst ? "Summer time" : "Winter time");
      }
    }
  }

  if (json_object.containsKey("time")) {
    int new_u_time = json_object["time"].as<int>() + offset + (dst ? 3600 : 0);
    if (new_u_time > 1546304461) {
      if (RTCisrunning()) {
        if (abs(new_u_time - (int)rtc.now().unixtime()) > 60) {
          rtc.adjust(DateTime(new_u_time));
          note("Adjust time");
        }
      } else {
        #ifdef physical_clock
          rtc.adjust(DateTime(new_u_time));
        #else
          rtc.begin(DateTime(new_u_time));
        #endif
        note("RTC begin");
        start_u_time = (millis() / 1000) + rtc.now().unixtime() - offset - (dst ? 3600 : 0);
      }
    }
  }

  if (json_object.containsKey("smart")) {
    if (getSmartString(true) != json_object["smart"].as<String>()) {
      setSmart(json_object["smart"].as<String>());
      settings_change = true;
    }
  }

  if (json_object.containsKey("smart_lock")) {
    if (smart_lock != strContains(json_object["smart_lock"].as<String>(), 1)) {
      smart_lock = !smart_lock;
      settings_change = true;
    }
  }

  if (json_object.containsKey("location")) {
    if (geo_location != json_object["location"].as<String>()) {
      geo_location = json_object["location"].as<String>();
      if (geo_location.length() > 2) {
        sun.setPosition(geo_location.substring(0, geo_location.indexOf("x")).toDouble(), geo_location.substring(geo_location.indexOf("x") + 1).toDouble(), 0);
      } else {
        last_sun_check = -1;
        next_sunset = -1;
        next_sunrise = -1;
        sunset_u_time = 0;
        sunrise_u_time = 0;
        calendar_twilight = false;
      }
      settings_change = true;
    }
  }

  if (json_object.containsKey("tilt")) {
    if (tilt != json_object["tilt"].as<int>()) {
      tilt = json_object["tilt"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("steps") && actual == destination) {
    if (steps != json_object["steps"].as<int>()) {
      steps = json_object["steps"].as<int>();
      settings_change = true;
    }
  }

  if (json_object.containsKey("light")) {
    if (sensor_twilight != strContains(json_object["light"].as<String>(), "t")) {
      sensor_twilight = !sensor_twilight;
      twilight_change = true;
      settings_change = true;
      if (RTCisrunning()) {
        int current_time = (rtc.now().hour() * 60) + rtc.now().minute();
        if (sensor_twilight) {
          if (abs(current_time - dusk_time) > 60) {
            dusk_time = current_time;
          }
        } else {
          if (abs(current_time - dawn_time) > 60) {
            dawn_time = current_time;
          }
        }
      }
    }
    if (strContains(json_object["light"].as<String>(), "t")) {
		  light_sensor = json_object["light"].as<String>().substring(0, json_object["light"].as<String>().indexOf("t")).toInt();
    } else {
		  light_sensor = json_object["light"].as<int>();
    }
  }

  if (json_object.containsKey("val")) {
    destination = toSteps(json_object["val"].as<int>(), steps);
  }

  if (settings_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (json_object.containsKey("light")) {
    smartAction(0, twilight_change);
  }
  if (json_object.containsKey("location") && RTCisrunning()) {
    getSunriseSunset(rtc.now());
  }
  if (json_object.containsKey("val")) {
    if (destination != actual) {
      prepareRotation(per_wifi ? (json_object.containsKey("apk") ? "apk" : "local") : "cloud");
    }
  }
}

void automation() {
  if (!RTCisrunning()) {
    smartAction();
    return;
  }

  DateTime now = rtc.now();
  int current_time = (now.hour() * 60) + now.minute();

  if (now.second() == 0) {
    if (current_time == 60) {
      ntpClient.update();
      readData("{\"time\":" + String(ntpClient.getEpochTime()) + "}", false);

      if (last_accessed_log++ > 14) {
        deactivationTheLog();
      }
    }
  }

  if (current_time == 120 || current_time == 180) {
    if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
      int new_u_time = now.unixtime() + 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = true;
      note("Setting summer time");
      saveSettings();
      getSunriseSunset(now);
    }
    if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
      int new_u_time = now.unixtime() - 3600;
      rtc.adjust(DateTime(new_u_time));
      dst = false;
      note("Setting winter time");
      saveSettings();
      getSunriseSunset(now);
    }
  }

  if (geo_location.length() < 2) {
    if (current_time == 181) {
      smart_lock = false;
      saveSettings();
    }
  } else {
    if (now.second() == 0 && ((current_time > 181 && last_sun_check != now.day()) || next_sunset == -1 || next_sunrise == -1)) {
      getSunriseSunset(now);
    }

    if (next_sunset > -1 && next_sunrise > -1) {
      if ((!calendar_twilight && current_time == next_sunset) || (calendar_twilight && current_time == next_sunrise)) {
        if (current_time == next_sunset) {
          calendar_twilight = true;
          sunset_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        if (current_time == next_sunrise) {
          calendar_twilight = false;
          sunrise_u_time = now.unixtime() - offset - (dst ? 3600 : 0);
        }
        smart_lock = false;
        saveSettings();
      }
    }
  }

  smartAction();
}

void smartAction() {
  smartAction(-1, false);
}


void setMin() {
  destination = 0;
  actual = 0;
  saveSettings();
  server.send(200, "text/plain", "Done");
}

void setMax() {
  destination = steps;
  actual = steps;
  saveSettings();
  server.send(200, "text/plain", "Done");
}

void setAsMax() {
  steps = actual;
  destination = actual;
  saveSettings();
  server.send(200, "text/plain", "Done");
}

void makeMeasurement() {
  if (measurement) {
    return;
  }

  if (!(destination == 0 || actual == 0)) {
    server.send(200, "text/plain", "Cannot execute");
    return;
  }

  measurement = true;
  digitalWrite(bipolar_direction_pin, HIGH);

  server.send(200, "text/plain", "Done");
}

void cancelMeasurement() {
  if (!measurement) {
    return;
  }

  measurement = false;
  setStepperOff();

  server.send(200, "text/plain", "Done");
}

void endMeasurement() {
  if (!measurement) {
    return;
  }

  measurement = false;
  setStepperOff();

  steps = actual;
  destination = actual;

  note("Measurement completed");
  saveSettings();
  server.send(200, "text/plain", "Done");
}


void setStepperOff() {
  digitalWrite(bipolar_enable_pin, HIGH);
  digitalWrite(bipolar_direction_pin, LOW);
  digitalWrite(bipolar_step_pin, LOW);
}

void prepareRotation(String orderer) {
  String log_text = "";
  if (actual != destination) {
    note("Movement (" + orderer + "):\n " + String(destination - actual) + " steps to " + toPercentages(destination, steps) + "%");
    saveTheState();
    saveSettings();
  }
}

void calibration(int set, bool positioning) {
  if (destination != actual) {
    return;
  }

  bool settings_change = false;
  String log_text = "";

  if (actual == 0 || positioning) {
    actual -= set / 2;
    log_text += "\n " + String(set) + " steps.";
  } else {
    if (actual == steps) {
      steps += set / 2;
      destination = steps;
      settings_change = true;
      log_text += "\n " + String(set) + " steps. Steps set at " + String(steps) + ".";
    }
  }

  note("Calibration: " + log_text);
  saveTheState();

  if (settings_change) {
    saveSettings();
  }
}

void measurementRotation() {
  digitalWrite(bipolar_enable_pin, LOW);
  actual++;

  digitalWrite(bipolar_step_pin, HIGH);
  digitalWrite(bipolar_step_pin, LOW);
  delay(4);
}

void rotation() {
  if (destination != actual) {
    digitalWrite(bipolar_direction_pin, destination > actual);
    digitalWrite(bipolar_enable_pin, LOW);
    if (destination > actual) {
      actual++;
    } else {
      actual--;
    }
  } else {
    digitalWrite(bipolar_enable_pin, HIGH);
  }

  digitalWrite(bipolar_step_pin, HIGH);
  digitalWrite(bipolar_step_pin, LOW);
  delay(4);
}
