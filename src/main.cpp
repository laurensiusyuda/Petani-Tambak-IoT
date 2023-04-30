#include <Arduino.h>
#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

#define ONE_WIRE_BUS D4
#define Analogpin A0
#define pin_Salinitas D5
#define pin_pH D6

#define VREF 3.3
#define SCOUNT 30

#define wifi_ssid "LAURENSIUS"
#define wifi_password "Alfa+Omega"

#define mqtt_server "broker.mqtt-dashboard.com"
#define mqqt_port 1883
#define salinity_topic "topicName/salt"
#define temperature_topic "topicName/temperature"
#define ph_topic "topicName/ph"

WiFiClient espClient;
OneWire oneWire(ONE_WIRE_BUS);
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DallasTemperature sensors(&oneWire);

int sensorValue = 0;
unsigned long int avgValue;
float b;
int buf[10], temp;

float suhu_rendah;
float suhu_sedang;
float suhu_tinggi;

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("changeMe"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff)
{
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

void setup()
{
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  sensors.begin();
  setup_wifi();
  client.setServer(mqtt_server, mqqt_port);
  pinMode(pin_Salinitas, OUTPUT);
  pinMode(pin_pH, OUTPUT);
}

float baca_suhu()
{
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  return tempC;
}

float baca_salinitas()
{
  digitalWrite(pin_Salinitas, HIGH);
  digitalWrite(pin_pH, LOW);
  for (int i = 0; i < 10; i++)
  {
    buf[i] = analogRead(Analogpin);
    delay(10);
  }
  for (int i = 0; i < 9; i++)
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buf[i] > buf[j])
      {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++)
    avgValue += buf[i];
  float nilaiADCSalinitas = (float)avgValue;
  float salt = (139.6803 + nilaiADCSalinitas) / 16.9394;
  return salt;
}

float baca_pH()
{
  digitalWrite(pin_pH, HIGH);
  digitalWrite(pin_Salinitas, LOW);
  for (int i = 0; i < 10; i++)
  {
    buf[i] = analogRead(Analogpin);
    delay(10);
  }
  for (int i = 0; i < 9; i++)
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buf[i] > buf[j])
      {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++)
    avgValue += buf[i];
  float pHVol = (float)avgValue * 3.3 / 1024 / 6;
  float phValue = (4.553930292 - pHVol) + 0.232093527;
  return phValue;
}

unsigned char suhuRendah()
{
  if (baca_suhu() <= 22)
  {
    suhu_rendah = 1;
  }
  else if (baca_suhu() >= 22 && baca_suhu() <= 24)
  {
    suhu_rendah = (baca_suhu() - 22) / (24 - 22);
  }
  else if (baca_suhu() > 24)
  {
    suhu_rendah = 0;
  }
  return suhu_rendah;
}

unsigned char suhuSejuk()
{
  if (baca_suhu() >= 22 && baca_suhu() <= 24)
  {
    suhu_sedang = (baca_suhu() - 22) / (24 - 22);
  }
  else if (baca_suhu() >= 24 && baca_suhu() <= 26)
  {
    suhu_sedang = 1;
  }
  else if (baca_suhu() >= 26 && baca_suhu() <= 30)
  {
    suhu_sedang = (30 - baca_suhu()) / (30 - 26);
  }
  return suhu_sedang;
}

unsigned char suhuDingin()
{
  if (baca_suhu() >= 26 && baca_suhu() <= 30)
  {
    suhu_tinggi = (baca_suhu() - 26) / (30 - 26);
  }
  else if (baca_suhu() > 30)
  {
    suhu_tinggi = 1;
  }
  return suhu_tinggi;
}

void loop()
{
  float nilai_suhu = baca_suhu();
  float nilai_salinitas = baca_salinitas();
  float nilai_pH = baca_pH();

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  client.publish(temperature_topic, String(nilai_suhu).c_str(), true);
  client.publish(salinity_topic, String(nilai_salinitas).c_str(), true);
  client.publish(ph_topic, String(nilai_pH).c_str(), true);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Suhu");
  lcd.print(nilai_suhu);
  lcd.print(" ");
  lcd.write(0xDF);
  lcd.print("C");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TDS");
  lcd.print(nilai_salinitas);
  lcd.print(" ");
  lcd.print("ppt");
  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pH");
  lcd.print(nilai_pH);
  lcd.print(" ");
  lcd.print("pH");
  delay(5000);

  Serial.println(nilai_suhu);
  Serial.println(nilai_salinitas);
  Serial.println(nilai_pH);
  delay(5000);
}