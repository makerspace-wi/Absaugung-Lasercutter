/*
Laser Absauge Controller - via MQTT
Controller: WEMOS D1 mini
Drucksensoren:
Motorola MP2010DP_1 (offene Version mit 2 ports) bis 10kPa -> 100mbar  https://www.nxp.com/docs/en/data-sheet/MPX2010.pdf
  ohne internen Verstärker
  - 2,5mV pro kPa - kalibriert und temperaturkompensiert

Radiallüfter erzeugt Unterdruck von max. 1100Pa - 1,1kPa -> 11 mbar
1kPa -> 0,01 bar -> 10 mbar
Gemessen werden soll die Druckdifferenz vor und nach dem Filter(n) - der ansteigt, wenn sich die Filter zusetzen
Es wird 2 Druckdifferenzkanäle geben.

MQTT-Topics:
Subscribe to topic lcfilter/stat/value_1 - value_2 - heartbeat - pwmmax - pwmact - nachlauf - ip - statusmsg
Publish to topic lcfilter/cmnd/data      tara - nachlaufN (N = Wert in Sekunden)

Cotroller SW kann über OTA-Programming geladen werden ==> IP-Adresse/update
*/

#include <Arduino.h>
#include <HX711_ADC.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <TaskScheduler.h>
#include <ESPAsyncTCP.h>
#include <ElegantOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Credentials_ms.h>

// pins:
const int HX711_dout_1 = 4;  // mcu > HX711 dout pin - D2
const int HX711_sck_1 = 5;   // mcu > HX711 sck pin - D1
const int HX711_dout_2 = 12; // mcu > HX711 dout pin - D6
const int HX711_sck_2 = 14;  // mcu > HX711 sck pin - D5

const int blower = 16;      // D0 2
const int laser_signal = 13; // D7

void refreshOffsetValueAndSaveToEEprom();
void mainloop();
void callback(char *, byte *, unsigned int);
void reconnect();
void setup_wifi();
void heartbeat();
void convert();
void check_laser();
void disable_pwm();
void publish_data();
void check_tara();


// HX711 constructor:
HX711_ADC MP2010DP_1(HX711_dout_1, HX711_sck_1);
HX711_ADC MP2010DP_2(HX711_dout_2, HX711_sck_2);
WiFiClient espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
PubSubClient client(espClient);
Scheduler ts;
AsyncWebServer server(80);

/*
// The neatest way to access variables stored in EEPROM is using a structure
struct MyEEPROMStruct {
  int     calVal_1;
  int     calVal_2;
  int     tareOffsetVal_1;
  int     tareOffsetVal_2;
  int     DelayVal;
  int     pwmMaxVal;
  boolean state;
} eepromVar1, eepromVar2;
*/

const int calVal_eepromAdress_1 = 0;
const int calVal_eepromAdress_2 = 4;
const int tareOffsetVal_eepromAdress_1 = 8;
const int tareOffsetVal_eepromAdress_2 = 12;
const int nachlaufVal_eepromAdress = 16;

unsigned long t = 0;
String temp_str_1;
String temp_str_2;
static boolean newDataReady = 0;

char msg[50];
char buffer[256];

int blower_active = 0;
int nachlauf = 30; // Nachlauf in Sekunden
float ch1;
float ch2;

const char *clientId = "FilterControl";  // bitte anpassen!

String newHostname = "LaserCutterFilter"; // bitte anpassen!

const char *PubTopicHeartbeat = "lcfilter/heartbeat"; // Topic Heartbeat
const char *PubTopicTime = "lcfilter/time";           // Topic Time
const char *PubTopicStatus = "lcfilter/statusmsg";    // Topic Status Message
const char *PubTopicSensor_1 = "lcfilter/value_1";    // Topic Status Message
const char *PubTopicSensor_2 = "lcfilter/value_2";    // Topic Status Message
const char *PubTopicPWMMAX = "lcfilter/pwmmax";       // Topic max pwm value
const char *PubTopicPWMACT = "lcfilter/blower_active";       // Topic blower status
const char *PubTopicNachlauf = "lcfilter/nachlauf";   // Topic actual nachlauf value
const char *PubTopicIP = "lcfilter/ip";               // Topic actual ip
const char *PubTopicPD = "lcfilter/PressureDifference";   // Topic actual ip
const char *PubTopicLaser = "lcfilter/laser";             // Topic Status Message
const char *SubTopicCmnd = "lcfilter/cmnd/data";

// Tasks
Task t1(TASK_SECOND * 5, TASK_FOREVER, &mainloop, &ts, true);     // main task
Task t2(TASK_SECOND / 5, TASK_FOREVER, &convert, &ts, true);      // convert task
Task t3(TASK_SECOND * 10, TASK_FOREVER, &heartbeat, &ts, true);   // hartbeat task
Task t4(TASK_SECOND / 4, TASK_FOREVER, &check_laser, &ts, true);  // check STATUS of LC-Controller
Task t5(0, TASK_ONCE, &disable_pwm, &ts, false);
Task t6(0, TASK_ONCE, &publish_data, &ts, false);
Task t7(TASK_HOUR, TASK_FOREVER, &check_tara, &ts, false);

void setup()
{
  Serial.begin(115200);
  pinMode(laser_signal, INPUT_PULLUP);
  pinMode(blower, OUTPUT);
  digitalWrite(blower, HIGH);
  ts.startNow();
  setup_wifi();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  { request->send(200, "text/plain", "Hi! Ich bin der Filter Controller"); });

  EEPROM.begin(512);
  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");
  timeClient.begin();
  timeClient.setTimeOffset(3600);
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  client.connect(clientId);
  client.subscribe(SubTopicCmnd);
 
  timeClient.update();
  while (timeClient.isTimeSet() != true)
  {
    delay(500);
    timeClient.update();
  }

  Serial.println();
  Serial.println("Starting...");

  MP2010DP_1.begin(); // Default Gain is 128 - also 64 and 32 is possible
  MP2010DP_2.begin(); // Default Gain is 128 - also 64 and 32 is possible
  // MP2010DP_1.setReverseOutput(); //uncomment to turn a negative output value to positive
  // MP2010DP_2.setReverseOutput(); //uncomment to turn a negative output value to positive
  float calibrationValue_1 = 100000.0;
  float calibrationValue_2 = 100000.0;

  // read values from EEPROM
  int temp;
  EEPROM.get(nachlaufVal_eepromAdress, temp);
  if (temp != -1)
  {
    EEPROM.get(nachlaufVal_eepromAdress, nachlauf);
  }


  client.publish(PubTopicNachlauf, String(nachlauf).c_str());
  // restore the zero offset value from eeprom:
  long tare_offset = 0;
  EEPROM.get(tareOffsetVal_eepromAdress_1, tare_offset);
  MP2010DP_1.setTareOffset(tare_offset);
  EEPROM.get(tareOffsetVal_eepromAdress_2, tare_offset);
  MP2010DP_2.setTareOffset(tare_offset);
  boolean _tare = false; // set this to false as the value has been restored from eeprom

  unsigned long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;

  while ((loadcell_1_rdy + loadcell_2_rdy) < 2)
  { // run startup, stabilization and tare, both modules simultaniously
    if (!loadcell_1_rdy)
      loadcell_1_rdy = MP2010DP_1.startMultiple(stabilizingtime, _tare);
    if (!loadcell_2_rdy)
      loadcell_2_rdy = MP2010DP_2.startMultiple(stabilizingtime, _tare);
  }
  if (MP2010DP_1.getTareTimeoutFlag())
  {
    Serial.println("Timeout, check MCU>HX711 no.1 wiring and pin designations");
  }
  if (MP2010DP_2.getTareTimeoutFlag())
  {
    Serial.println("Timeout, check MCU>HX711 no.2 wiring and pin designations");
  }
  MP2010DP_1.setCalFactor(calibrationValue_1); // user set calibration value (float)
  MP2010DP_2.setCalFactor(calibrationValue_2); // user set calibration value (float)
  temp_str_1 = String(timeClient.getFormattedTime());
  temp_str_2 = "POR LaserCutterFilter - " + temp_str_1;
  Serial.println("Startup is complete");

  client.publish(PubTopicStatus, temp_str_2.c_str());
  client.publish(PubTopicIP, WiFi.localIP().toString().c_str());
  client.publish(PubTopicLaser, String(0).c_str());

  t6.restart();
  t7.restartDelayed(TASK_SECOND * 5); // start periodic tara check/adjustment
}

void loop(void)
{
  ts.execute();
  client.loop();
  ElegantOTA.loop();
}

// Functions
void convert()
{
  timeClient.update();
  newDataReady = 0;
  // check for new data/start next conversion:
  if (MP2010DP_1.update())
    newDataReady = true;
  MP2010DP_2.update();
}

void mainloop()
{
  JsonDocument doc;
  // get smoothed value from the dataset:
  if (newDataReady)
  {
    ch1 = MP2010DP_1.getData();
    ch2 = MP2010DP_2.getData();
    Serial.print("Output val 1: ");
    Serial.println(ch1);
    Serial.print("Output val 2: ");
    Serial.println(ch2);
    newDataReady = 0;

    doc["Channel_1"] = ch1;
    doc["Channel_2"] = ch2;
    serializeJson(doc, buffer);
    client.publish(PubTopicPD, buffer);

   // t6.restart(); // publish values
  }
}

// zero offset value (tare), calculate and save to EEprom:
void refreshOffsetValueAndSaveToEEprom()
{
  long _offset = 0;
  Serial.println("Calculating tare offset value 1 ...");
  MP2010DP_1.tare();                                 // calculate the new tare / zero offset value (blocking)
  _offset = MP2010DP_1.getTareOffset();              // get the new tare / zero offset value
  EEPROM.put(tareOffsetVal_eepromAdress_1, _offset); // save the new tare / zero offset value to EEprom
  EEPROM.commit();

  MP2010DP_1.setTareOffset(_offset); // set value as library parameter (next restart it will be read from EEprom)
  Serial.print("New tare offset value 1:");
  Serial.print(_offset);
  Serial.print(", saved to EEprom adr:");
  Serial.println(tareOffsetVal_eepromAdress_1);

  Serial.println("Calculating tare offset value 2 ...");
  MP2010DP_2.tare();                                 // calculate the new tare / zero offset value (blocking)
  _offset = MP2010DP_2.getTareOffset();              // get the new tare / zero offset value
  EEPROM.put(tareOffsetVal_eepromAdress_2, _offset); // save the new tare / zero offset value to EEprom
  EEPROM.commit();

  MP2010DP_2.setTareOffset(_offset); // set value as library parameter (next restart it will be read from EEprom)
  Serial.print("New tare offset value 2:");
  Serial.print(_offset);
  Serial.print(", saved to EEprom adr:");
  Serial.println(tareOffsetVal_eepromAdress_2);
}

void check_tara()
{
  if ((blower_active == 0 && abs(ch1) >= 0.05) || (blower_active == 0 && abs(ch2) >= 0.05))
  {
    refreshOffsetValueAndSaveToEEprom();
  }
}

void check_WiFi()
{
  if ((WiFi.status() != WL_CONNECTED))
  {
    Serial.println(F("\nWiFi lost. Call setup_wifi()"));
    setup_wifi();
  }
}

void heartbeat()
{
  timeClient.update();
  client.publish(PubTopicHeartbeat, String(timeClient.getFormattedTime()).c_str());
}

void setup_wifi()
{
  WiFi.hostname(newHostname.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  check_WiFi();

  if (!client.connected())
  {
    if (!client.connect(clientId))
    {
      Serial.print("Reconnecting...");
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
    client.subscribe(SubTopicCmnd);
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String plo = ""; // create String Object
  Serial.print("Received message [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    plo += String((char)payload[i]);
  }
  Serial.println();
  plo.toUpperCase();

  if (plo.startsWith("TARA"))
  {
    refreshOffsetValueAndSaveToEEprom();
  }
 
  if (plo.startsWith("NACHLAUF"))
  {
    nachlauf = plo.substring(8).toInt();
    client.publish(PubTopicNachlauf, plo.substring(8).c_str());
    EEPROM.put(nachlaufVal_eepromAdress, nachlauf); // save the new nachlauf value to EEprom
    EEPROM.commit();
  }
}

void check_laser() // check if laser is active - on true turn on PWM
{
  if (!digitalRead(laser_signal))
  {
    blower_active = 1;
    //analogWrite(blower, blower_active);
    digitalWrite(blower, LOW);
    t4.setInterval(TASK_SECOND * 5);
    t5.restartDelayed(nachlauf * TASK_SECOND);
    t6.restart(); // publish data
    t7.disable(); // hourly tara check
    client.publish(PubTopicLaser, String(1).c_str());
  }
  // Nachlaufzeit mit halber Lüfterleistung
  if (digitalRead(laser_signal) && blower_active > 0)
  {
    blower_active = 1;
    //analogWrite(blower, blower_active);
    t4.setInterval(TASK_SECOND * 5);
    t6.restart(); // publish data
    client.publish(PubTopicLaser, String(0).c_str());
  }
}

void disable_pwm() // disable PWM
{
  blower_active = 0;
  digitalWrite(blower, HIGH);
  t6.restart();
  t7.restartDelayed(TASK_MINUTE);
  t4.setInterval(500);
}

void publish_data()
{
  client.publish(PubTopicPWMACT, String(blower_active).c_str());
  //client.publish(PubTopicNachlauf, String(nachlauf).c_str());
}