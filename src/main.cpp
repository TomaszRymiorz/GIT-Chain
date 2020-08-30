#include <c_online.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  LittleFS.begin();
  Wire.begin();

  keep_log = LittleFS.exists("/log.txt");

  note("iDom Chain " + String(version));
  Serial.print("\nDevice ID: " + WiFi.macAddress());
  offline = !LittleFS.exists("/online.txt");
  Serial.printf("\nThe device is set to %s mode", offline ? "OFFLINE" : "ONLINE");

  sprintf(host_name, "chain_%s", String(WiFi.macAddress()).c_str());
  WiFi.hostname(host_name);

  if (!readSettings(0)) {
    readSettings(1);
  }
  resume();

  RTC.begin();
  if (RTC.isrunning()) {
    start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
  }
  Serial.printf("\nRTC initialization %s", start_time != 0 ? "completed" : "failed!");

  pinMode(bipolar_enable_pin, OUTPUT);
  pinMode(bipolar_direction_pin, OUTPUT);
  pinMode(bipolar_step_pin, OUTPUT);
  setStepperOff();

  if (ssid != "" && password != "") {
    connectingToWifi();
  } else {
    initiatingWPS();
  }
}


void setStepperOff() {
  digitalWrite(bipolar_enable_pin, HIGH);
  digitalWrite(bipolar_direction_pin, LOW);
  digitalWrite(bipolar_step_pin, LOW);
}

String toPercentages(int value, int steps) {
  return String(value > 0 && steps > 0 ? value * 100 / steps : 0);
}

int toSteps(int value, int steps) {
  return value > 0 && steps > 0 ? value * steps / 100 : 0;
}


bool readSettings(bool backup) {
  File file = LittleFS.open(backup ? "/backup.txt" : "/settings.txt", "r");
  if (!file) {
    note("The " + String(backup ? "backup" : "settings") + " file cannot be read");
    return false;
  }

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());
  file.close();

  if (json_object.isNull() || json_object.size() < 5) {
    note(String(backup ? "Backup" : "Settings") + " file error");
    return false;
  }

  if (json_object.containsKey("ssid")) {
    ssid = json_object["ssid"].as<String>();
  }
  if (json_object.containsKey("password")) {
    password = json_object["password"].as<String>();
  }

  if (json_object.containsKey("smart")) {
    smart_string = json_object["smart"].as<String>();
    setSmart();
  }
  if (json_object.containsKey("uprisings")) {
    uprisings = json_object["uprisings"].as<int>() + 1;
  }
  if (json_object.containsKey("offset")) {
    offset = json_object["offset"].as<int>();
  }
  if (json_object.containsKey("dst")) {
    dst = json_object["dst"].as<bool>();
  }

  if (json_object.containsKey("steps")) {
    steps = json_object["steps"].as<int>();
  }

  if (json_object.containsKey("tilt")) {
    tilt = json_object["tilt"].as<int>();
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

  String logs;
  serializeJson(json_object, logs);
  note("Reading the " + String(backup ? "backup" : "settings") + " file:\n " + logs);

  saveSettings();

  return true;
}

void saveSettings() {
  DynamicJsonDocument json_object(1024);

  json_object["ssid"] = ssid;
  json_object["password"] = password;

  json_object["smart"] = smart_string;
  json_object["uprisings"] = uprisings;
  json_object["offset"] = offset;
  json_object["dst"] = dst;
  json_object["tilt"] = tilt;

  json_object["steps"] = steps;
  json_object["destination"] = destination;

  if (writeObjectToFile("settings", json_object)) {
    String logs;
    serializeJson(json_object, logs);
    note("Saving settings:\n " + logs);

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

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, file.readString());
  file.close();

  if (!json_object.isNull() && json_object.size() > 0) {
    String logs = "";

    if (json_object.containsKey("1")) {
      actual = json_object["1"].as<int>();
      if (actual != destination) {
        logs = "\n1 to " + String(destination - actual) + " steps to " + toPercentages(destination, steps) + "%";
      }
    }

    if (actual != destination) {
      note("Resume: " + logs);
    } else {
      if (LittleFS.exists("/resume.txt")) {
        LittleFS.remove("/resume.txt");
      }
    }
  }
}

void saveTheState() {
  DynamicJsonDocument json_object(1024);

  json_object["1"] = actual;

  writeObjectToFile("resume", json_object);
}


void sayHelloToTheServer() {
  // This function is only available with a ready-made iDom device.
}

void introductionToServer() {
  // This function is only available with a ready-made iDom device.
}

void startServices() {
  server.on("/hello", HTTP_POST, handshake);
  server.on("/set", HTTP_PUT, receivedOfflineData);
  server.on("/state", HTTP_GET, requestForState);
  server.on("/basicdata", HTTP_POST, exchangeOfBasicData);
  server.on("/priority", HTTP_POST, confirmationOfPriority);
  server.on("/measurement/start", HTTP_POST, makeMeasurement);
  server.on("/measurement/cancel", HTTP_POST, cancelMeasurement);
  server.on("/measurement/end", HTTP_POST, endMeasurement);
  server.on("/log", HTTP_GET, requestForLogs);
  server.on("/log", HTTP_DELETE, clearTheLog);
  server.on("/admin/reset", HTTP_POST, setMin);
  server.on("/admin/setmax", HTTP_POST, setMax);
  server.on("/admin/setasmax", HTTP_POST, setAsMax);
  server.on("/admin/log", HTTP_POST, activationTheLog);
  server.on("/admin/log", HTTP_DELETE, deactivationTheLog);
  server.on("/admin/wifisettings", HTTP_DELETE, deleteWiFiSettings);
  server.begin();

  note("Launch of services. " + String(host_name) + (MDNS.begin(host_name) ? " started." : " unsuccessful!"));

  MDNS.addService("idom", "tcp", 8080);

  if (!offline) {
    prime = true;
  }
  networked_devices = WiFi.macAddress();
  getOfflineData(true, true);
}

String getChainDetail() {
  return String(steps) + "," + String(tilt) + "," + RTC.isrunning() + "," + String(start_time) + "," + uprisings + "," + version;
}

void handshake() {
  readData(server.arg("plain"), true);

  String reply = "\"id\":\"" + WiFi.macAddress()
  + "\",\"value\":" + toPercentages(destination, steps)
  + ",\"steps\":" + steps
  + ",\"tilt\":" + tilt
  + ",\"version\":" + version
  + ",\"smart\":\"" + smart_string
  + "\",\"rtc\":" + RTC.isrunning()
  + ",\"dst\":" + dst
  + ",\"offset\":" + offset
  + ",\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"active\":" + String(start_time != 0 ? RTC.now().unixtime() - offset - (dst ? 3600 : 0) - start_time : 0)
  + ",\"uprisings\":" + uprisings
  + ",\"offline\":" + offline
  + ",\"prime\":" + prime
  + ",\"devices\":\"" + networked_devices + "\"";

  Serial.print("\nHandshake");
  server.send(200, "text/plain", "{" + reply + "}");
}

void requestForState() {
  String reply = "\"state\":\"" + toPercentages(destination, steps)
  + (!measurement && destination != actual ? "^" + toPercentages(actual, steps) : "")
  + "\"";

  server.send(200, "text/plain", "{" + reply + "}");
}

void exchangeOfBasicData() {
  readData(server.arg("plain"), true);

  String reply = RTC.isrunning() ? ("\"time\":" + String(RTC.now().unixtime() - offset - (dst ? 3600 : 0))
  + ",\"offset\":" + offset
  + ",\"dst\":" + String(dst)) : "";

  reply += !offline && prime ? (String(reply.length() > 0 ? "," : "") + "\"prime\":" + String(prime)) : "";

  reply += String(reply.length() > 0 ? "," : "") + "\"id\":\"" + String(WiFi.macAddress()) + "\"";

  server.send(200, "text/plain", "{" + reply + "}");
}

void setMin() {
  destination = 0;
  actual = 0;
  saveSettings();
  server.send(200, "text/plain", "Done");
  putOnlineData("detail", "val=0");
}

void setMax() {
  destination = steps;
  actual = steps;
  saveSettings();
  server.send(200, "text/plain", "Done");
  putOnlineData("detail", "val=100");
}

void setAsMax() {
  steps = actual;
  destination = actual;
  saveSettings();
  server.send(200, "text/plain", "Done");
  putOnlineData("detail", "val=100");
}

void makeMeasurement() {
  if (measurement) {
    return;
  }

  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, server.arg("plain"));

  if (json_object.isNull()) {
    server.send(200, "text/plain", "Body not received");
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
  sayHelloToTheServer();

  server.send(200, "text/plain", "Done");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
  } else {
    if (!sending_error) {
      note("Wi-Fi connection lost");
    }
    sending_error = true;
    cancelMeasurement();
  }

  server.handleClient();
  MDNS.update();

  if (measurement) {
    measurementRotation();
    return;
  }

  if (hasTimeChanged()) {
    if (destination != actual) {
      if (loop_time % 2 == 0) {
        saveTheState();
      }
    }
    if (loop_time % 2 == 0) {
      getOnlineData();
    }
    automaticSettings();
  }

  if (destination != actual) {
    rotation();
    if (destination == actual) {
      Serial.print("\nChain reached the target position");
      setStepperOff();
      if (LittleFS.exists("/resume.txt")) {
        LittleFS.remove("/resume.txt");
      }
    }
  }
}

void readData(String payload, bool per_wifi) {
  DynamicJsonDocument json_object(1024);
  deserializeJson(json_object, payload);

  if (json_object.isNull()) {
    if (payload.length() > 0) {
      Serial.print("\n Parsing failed!");
    }
    return;
  }

  if (json_object.containsKey("apk")) {
    per_wifi = json_object["apk"].as<bool>();
  }

  if (json_object.containsKey("id")) {
    String new_networked_devices = json_object["id"].as<String>();
    if (!strContains(networked_devices, new_networked_devices)) {
      networked_devices +=  "," + new_networked_devices;
    }
  }

  if (json_object.containsKey("prime")) {
    prime = false;
  }

  if (json_object.containsKey("calibrate")) {
    calibration(json_object["calibrate"].as<int>(), json_object.containsKey("bypass"));
    return;
  }

  bool settings_change = false;
  bool details_change = false;
  String result = "";

  uint32_t new_time = 0;
  if (json_object.containsKey("offset")) {
    int new_offset = json_object["offset"].as<int>();
    if (offset != new_offset) {
      if (RTC.isrunning() && !json_object.containsKey("time")) {
        RTC.adjust(DateTime((RTC.now().unixtime() - offset) + new_offset));
        note("Time zone change");
      }

      offset = new_offset;
      settings_change = true;
    }
  }

  if (json_object.containsKey("dst")) {
    bool new_dst = json_object["dst"].as<bool>();
    if (dst != new_dst) {
      if (RTC.isrunning() && !json_object.containsKey("time")) {
        if (new_dst) {
          new_time = RTC.now().unixtime() + 3600;
        } else {
          new_time = RTC.now().unixtime() - 3600;
        }
        RTC.adjust(DateTime(new_time));
        note(new_dst ? "Summer time" : "Winter time");
      }

      dst = new_dst;
      settings_change = true;
    }
  }

  if (json_object.containsKey("time")) {
    new_time = json_object["time"].as<uint32_t>() + offset + (dst ? 3600 : 0);
    if (new_time > 1546304461) {
      if (RTC.isrunning()) {
        if (abs(new_time - RTC.now().unixtime()) > 60) {
          RTC.adjust(DateTime(new_time));
        }
      } else {
        RTC.adjust(DateTime(new_time));
        note("Adjust time");
        start_time = RTC.now().unixtime() - offset - (dst ? 3600 : 0);
        if (RTC.isrunning()) {
          sayHelloToTheServer();
        }
      }
    }
  }

  if (json_object.containsKey("up")) {
    uint32_t new_update_time = json_object["up"].as<uint32_t>();
    if (update_time < new_update_time) {
      update_time = new_update_time;
    }
  }

  if (json_object.containsKey("smart")) {
    String new_smart_string = json_object["smart"].as<String>();
    if (smart_string != new_smart_string) {
      smart_string = new_smart_string;
      setSmart();
      result = "smart=" + getSmartString();
      settings_change = true;
    }
  }

  if (json_object.containsKey("val")) {
    int new_value = json_object["val"].as<int>();
    int new_destination = steps > 0 ? toSteps(new_value, steps) : destination;

    if (destination != new_destination) {
      destination = new_destination;
      prepareRotation();
      result += String(result.length() > 0 ? "&" : "") + "val=" + toPercentages(destination, steps);
    }
  }

  if (json_object.containsKey("steps")) {
    int new_steps = json_object["steps"].as<int>();
    if (steps != new_steps && actual == 0) {
      steps = new_steps;
      details_change = true;
    }
  }

  if (json_object.containsKey("tilt")) {
    int new_tilt = json_object["tilt"].as<int>();
    if (tilt != new_tilt) {
      tilt = new_tilt;
      details_change = true;
    }
  }

  if (json_object.containsKey("temp")) {
    float new_temperature = json_object["temp"].as<float>();
    if (temperature != new_temperature) {
      temperature = !new_temperature;
      automaticSettings(true);
    }
  }

  if (settings_change || details_change) {
    note("Received the data:\n " + payload);
    saveSettings();
  }
  if (per_wifi && (result.length() > 0 || details_change)) {
    if (details_change) {
      result += String(result.length() > 0 ? "&" : "") + "detail=" + getChainDetail();
    }
    putOnlineData("detail", result);
  }
}

void setSmart() {
  if (smart_string.length() < 2) {
    smart_count = 0;
    return;
  }

  String smart;
  bool enabled;
  String days;
  int tilting_time;
  int opening_time;
  int closing_time;

  int count = 1;
  smart_count = 1;
  for (byte b: smart_string) {
    if (b == ',') {
      count++;
    }
    if (b == 'c') {
      smart_count++;
    }
  }

  if (smart_array != 0) {
    delete [] smart_array;
  }
  smart_array = new Smart[smart_count];
  smart_count = 0;

  for (int i = 0; i < count; i++) {
    smart = get1(smart_string, i);
    if (smart.length() > 0 && strContains(smart, "c")) {
      enabled = !strContains(smart, "/");
      smart = enabled ? smart : smart.substring(1);

      opening_time = strContains(smart, "_") ? smart.substring(0, smart.indexOf("_")).toInt() : -1;
      tilting_time = strContains(smart, "p") ? smart.substring((strContains(smart, "_") ? smart.indexOf("_") + 1 : 0), smart.indexOf("p")).toInt() : -1;
      closing_time = strContains(smart, "-") ? smart.substring(smart.indexOf("-") + 1).toInt() : -1;

      smart = strContains(smart, "_") ? smart.substring(smart.indexOf("_") + 1) : smart;
      smart = strContains(smart, "p") ? smart.substring(smart.indexOf("p") + 1) : smart;
      smart = strContains(smart, "-") ? smart.substring(0, smart.indexOf("-")) : smart;

      days = strContains(smart, "w") ? "w" : "";
      days += strContains(smart, "o") ? "o" : "";
      days += strContains(smart, "u") ? "u" : "";
      days += strContains(smart, "e") ? "e" : "";
      days += strContains(smart, "h") ? "h" : "";
      days += strContains(smart, "r") ? "r" : "";
      days += strContains(smart, "a") ? "a" : "";
      days += strContains(smart, "s") ? "s" : "";

      smart_array[smart_count++] = (Smart) {enabled, days, tilting_time, opening_time, closing_time, 0};
    }
  }
}

bool automaticSettings() {
  return automaticSettings(false);
}

bool automaticSettings(bool temperature_changed) {
  bool result = false;
  DateTime now = RTC.now();
  String log = "Smart ";
  int current_time = 0;

  if (RTC.isrunning()) {
    current_time = (now.hour() * 60) + now.minute();

    if (current_time == 120 || current_time == 180) {
      if (now.month() == 3 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 120 && !dst) {
        int new_time = RTC.now().unixtime() + 3600;
        RTC.adjust(DateTime(new_time));
        dst = true;
        saveSettings();
        note("Smart set to summer time");
      }
      if (now.month() == 10 && now.day() > 24 && days_of_the_week[now.dayOfTheWeek()][0] == 's' && current_time == 180 && dst) {
        int new_time = RTC.now().unixtime() - 3600;
        RTC.adjust(DateTime(new_time));
        dst = false;
        saveSettings();
        note("Smart set to winter time");
      }
    }
  }

  int i = -1;
  while (++i < smart_count) {
    if (smart_array[i].enabled) {
      if (temperature_changed) {

      } else {
        if (RTC.isrunning() && smart_array[i].access + 60 < now.unixtime()) {
          if (smart_array[i].tilting_time == current_time && (strContains(smart_array[i].days, "w") || strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()]))) {
            smart_array[i].access = now.unixtime();
            destination = toSteps(tilt, steps);
            result = true;
            log += "tilting at time";
          }
          if (smart_array[i].opening_time == current_time && (strContains(smart_array[i].days, "w") || strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()]))) {
            smart_array[i].access = now.unixtime();
            destination = steps;
            result = true;
            log += "opening at time";
          }
          if (smart_array[i].closing_time == current_time && (strContains(smart_array[i].days, "w") || strContains(smart_array[i].days, days_of_the_week[now.dayOfTheWeek()]))) {
            smart_array[i].access = now.unixtime();
            destination = 0;
            result = true;
            log += "closing at time";
          }
        }
      }
    }
  }

  if (result && destination != actual) {
    note(log);
    putOnlineData("detail", "val=" + toPercentages(destination, steps));
    prepareRotation();

    return true;
  } else {
    if (temperature_changed) {
      note("Smart didn't activate anything.");
    }
    return false;
  }
}


void prepareRotation() {
  String logs = "";
  if (actual != destination) {
    logs = "\n 1 by " + String(destination - actual) + " steps to " + toPercentages(destination, steps) + "%";
  }
  note("Movement: " + logs);

  saveSettings();
  saveTheState();
}

void calibration(int set, bool bypass) {
  if (!bypass && destination != actual) {
    return;
  }

  bool settings_change = false;
  String logs = "";

  if (actual == 0) {
    actual -= set / 2;
  } else
    if (actual == steps) {
      steps += set / 2;
      destination = steps;
      settings_change = true;
      logs += "\n 1 by " + String(set) + " steps. Steps set at " + String(steps) + ".";
    }

  if (settings_change) {
    note("Calibration: " + logs);
    saveSettings();
    saveTheState();
  } else {
    note("Zero calibration by " + String(set) + " steps.");
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
