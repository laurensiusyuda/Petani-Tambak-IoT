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

int buf[10], temp;
float minr[27];
float Rule[27];

float suhu_rendah;
float suhu_sedang;
float suhu_tinggi;

float salinitas_tawar;
float salinitas_payau;
float salinitas_asin;

float ph_asam;
float ph_netral;
float ph_basa;

// membuat fungsi untuk koneksi menuju wifi
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

// membuat fungsi untuk koneksi ulang jika wifi mengalamai ganguan
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

// membuat fungsi untuk membaca sensor suhu
float baca_suhu()
{
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  return tempC;
}

// membuat fungsi untuk membaca sensor salinitas
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

// membuat fungsi untuk membaca sensor pH
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

// fuzyfikasi sensor suhu
unsigned char suhuRendah()
{
  if (baca_suhu() <= 22)
  {
    suhu_rendah = 1;
  }
  else if (baca_suhu() >= 22 && baca_suhu() <= 24)
  {
    suhu_rendah = (24 - baca_suhu()) / (24 - 22);
  }
  else if (baca_suhu() > 24)
  {
    suhu_rendah = 0;
  }
  return suhu_rendah;
}

unsigned char suhuSejuk()
{
  if (baca_suhu() <= 22)
  {
    suhu_sedang = 0;
  }
  else if (baca_suhu() >= 22 && baca_suhu() <= 24)
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
  else if (baca_suhu() >= 30)
  {
    suhu_sedang = 0;
  }
  return suhu_sedang;
}

unsigned char suhuPanas()
{
  if (baca_suhu() <= 26)
  {
    suhu_tinggi = 0;
  }
  else if (baca_suhu() >= 26 && baca_suhu() <= 30)
  {
    suhu_tinggi = (baca_suhu() - 26) / (30 - 26);
  }
  else if (baca_suhu() >= 30)
  {
    suhu_tinggi = 1;
  }
  return suhu_tinggi;
}

// fuzzyfikasi sensor salinitas
unsigned char salinitasTawar()
{
  if (baca_salinitas() <= 25)
  {
    salinitas_tawar = 1;
  }
  else if (baca_salinitas() >= 20 && baca_salinitas() <= 25)
  {
    salinitas_tawar = (25 - baca_salinitas()) / (25 - 20);
  }
  else if (baca_salinitas() >= 25)
  {
    salinitas_tawar = 0;
  }
  return salinitas_tawar;
}

unsigned char salinitasPayau()
{
  if (baca_salinitas() <= 20)
  {
    salinitas_payau = 0;
  }
  else if (baca_salinitas() >= 20 && baca_salinitas() <= 25)
  {
    salinitas_payau = (baca_salinitas() - 20) / (25 - 20);
  }
  else if (baca_salinitas() >= 25 && baca_salinitas() <= 28)
  {
    salinitas_payau = 1;
  }
  else if (baca_salinitas() >= 25)
  {
    salinitas_payau = 0;
  }
  return salinitas_payau;
}

unsigned char salinitasAsin()
{
  if (baca_salinitas() <= 25)
  {
    salinitas_asin = 0;
  }
  else if (baca_salinitas() >= 28 && baca_salinitas() <= 30)
  {
    salinitas_asin = (baca_salinitas() - 28) / (30 - 28);
  }
  else if (baca_salinitas() >= 30)
  {
    salinitas_asin = 1;
  }
  return salinitas_asin;
}

// fuzzyfikasi sensor pH
unsigned char phAsam()
{
  if (baca_pH() <= 6.5)
  {
    ph_asam = 1;
  }
  else if (baca_pH() >= 6.5 && baca_pH() <= 7)
  {
    ph_asam = (baca_pH() - 6.5) / (25 - 20);
  }
  else if (baca_pH() >= 7)
  {
    ph_asam = 0;
  }
  return ph_asam;
}
unsigned char phNormal()
{
  if (baca_pH() <= 6.5)
  {
    ph_netral = 0;
  }
  else if (baca_pH() >= 6.5 && baca_pH() <= 7)
  {
    ph_netral = (baca_pH() - 6.5) / (7 - 6.5);
  }
  else if (baca_pH() >= 7 && baca_pH() <= 8)
  {
    ph_netral = 1;
  }
  else if (baca_pH() >= 9)
  {
    ph_netral = 0;
  }
  return ph_netral;
}
unsigned char phBasa()
{
  if (baca_pH() <= 8)
  {
    ph_asam = 0;
  }
  else if (baca_pH() >= 6.5 && baca_pH() <= 9)
  {
    ph_asam = (baca_pH() - 8) / (8 - 9);
  }
  else if (baca_pH() >= 9)
  {
    ph_asam = 1;
  }
  return ph_asam;
}
// membuat fungsi void fuzzyfikasi
void fuzzyfikasi()
{
  suhuRendah();
  suhuSejuk();
  suhuPanas();
  salinitasPayau();
  salinitasTawar();
  salinitasAsin();
  phAsam();
  phNormal();
  phBasa();
}

// membuat fungsi min
float Min(float a, float b, float c)
{
  if (a <= b && a <= c)
  {
    return a;
  }
  else if (b <= a && b <= c)
  {
    return b;
  }
  else
  {
    return c;
  }
}

// komposisi aturan
void rule()
{
  fuzzyfikasi();
  minr[0] = Min(suhu_rendah, salinitas_tawar, ph_basa);    //! buruk
  minr[1] = Min(suhu_rendah, salinitas_tawar, ph_netral);  //! buruk
  minr[2] = Min(suhu_rendah, salinitas_tawar, ph_asam);    //! buruk
  minr[3] = Min(suhu_rendah, salinitas_payau, ph_basa);    //! buruk
  minr[4] = Min(suhu_rendah, salinitas_payau, ph_netral);  //? cukup
  minr[5] = Min(suhu_rendah, salinitas_payau, ph_asam);    //! buruk
  minr[6] = Min(suhu_rendah, salinitas_asin, ph_basa);     //! buruk
  minr[7] = Min(suhu_rendah, salinitas_asin, ph_netral);   //! buruk
  minr[8] = Min(suhu_rendah, salinitas_asin, ph_asam);     //! buruk
  minr[9] = Min(suhu_sedang, salinitas_tawar, ph_basa);    //! buruk
  minr[10] = Min(suhu_sedang, salinitas_tawar, ph_netral); //? cukup
  minr[11] = Min(suhu_sedang, salinitas_tawar, ph_asam);   //! buruk
  minr[12] = Min(suhu_sedang, salinitas_payau, ph_basa);   //? cukup
  minr[13] = Min(suhu_sedang, salinitas_payau, ph_netral); //* bagus
  minr[14] = Min(suhu_sedang, salinitas_payau, ph_asam);   //? cukup
  minr[15] = Min(suhu_sedang, salinitas_asin, ph_basa);    //! buruk
  minr[16] = Min(suhu_sedang, salinitas_asin, ph_netral);  //! buruk
  minr[17] = Min(suhu_sedang, salinitas_asin, ph_asam);    //! buruk
  minr[18] = Min(suhu_tinggi, salinitas_tawar, ph_basa);   //! buruk
  minr[19] = Min(suhu_tinggi, salinitas_tawar, ph_netral); //! buruk
  minr[20] = Min(suhu_tinggi, salinitas_tawar, ph_asam);   //! buruk
  minr[21] = Min(suhu_tinggi, salinitas_payau, ph_basa);   //! buruk
  minr[22] = Min(suhu_tinggi, salinitas_payau, ph_netral); //? cukup
  minr[23] = Min(suhu_tinggi, salinitas_payau, ph_asam);   //! buruk
  minr[24] = Min(suhu_tinggi, salinitas_asin, ph_basa);    //! buruk
  minr[25] = Min(suhu_tinggi, salinitas_asin, ph_netral);  //! buruk
  minr[26] = Min(suhu_tinggi, salinitas_asin, ph_asam);    //! buruk
  Rule[0] = minr[0];
  Rule[1] = minr[1];
  Rule[2] = minr[2];
  Rule[3] = minr[3];
  Rule[4] = minr[4];
  Rule[5] = minr[5];
  Rule[6] = minr[6];
  Rule[7] = minr[7];
  Rule[8] = minr[8];
  Rule[9] = minr[9];
  Rule[10] = minr[10];
  Rule[11] = minr[11];
  Rule[12] = minr[12];
  Rule[13] = minr[13];
  Rule[14] = minr[14];
  Rule[15] = minr[15];
  Rule[16] = minr[16];
  Rule[17] = minr[17];
  Rule[18] = minr[18];
  Rule[19] = minr[19];
  Rule[20] = minr[20];
  Rule[21] = minr[21];
  Rule[22] = minr[22];
  Rule[23] = minr[23];
  Rule[24] = minr[24];
  Rule[25] = minr[25];
  Rule[26] = minr[26];
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