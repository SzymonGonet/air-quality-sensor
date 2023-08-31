#include <Arduino.h>
#include <WiFi.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include "FS.h"
#include "SD.h"
#include "PMS.h"

// microSD module
#define SD_CS 5

String dataMessage;

// PMS module
String val1; // PMS2.5 variable
String val2; // PMS10 variable

PMS pms(Serial2);
PMS::DATA data;

// GPS module
String latitude_string, longitiude_string;
float latitude, longitude;

char Time[] = "Czas: 00:00:00";
char Date[] = "Data: 00-00-2000";
byte last_second;

HardwareSerial SerialGPS(1);
TinyGPSPlus gps;

// Button and LED
const int buttonPin = 14;
const int ledPin = 27;

bool ledState = false;
unsigned long previousMillis = 0;
const long debounceInterval = 300;

// PWM input
#define PWM_PIN 34

unsigned long startTime;
unsigned long endTime;
unsigned long dutyCycle;

// WebServer
const char *ssid = "ESP32_CF3C8D";

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

String SendHTML(int val1, int val2)
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Wireless Weather Station</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: flex; text-align: center; margin: 10px 30px;}\n";
  ptr += "body{margin-top: 15px;} h2{margin-bottom: 30px;}\n";
  ptr += "p {font-size: 18px; color: #000000;}\n";
  ptr += "</style>\n";
  ptr += "<script>\n";
  ptr += "setInterval(loadDoc,1000);\n";
  ptr += "function loadDoc() {\n";
  ptr += "var xhttp = new XMLHttpRequest();\n";
  ptr += "xhttp.onreadystatechange = function() {\n";
  ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  ptr += "document.body.innerHTML =this.responseText}\n";
  ptr += "};\n";
  ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  ptr += "xhttp.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h2>Czujnik jakości powietrza:</h2>";

  if (ledState == LOW)
  {
    ptr += "<p>Aktualny tryb pracy: ";
    ptr += "<b>";
    ptr += "Tryb naziemny";
    ptr += "</b>";
    ptr += "<br>";
    ptr += "<br>";
    ptr += "<font color='#FF0000'>";
    ptr += "<p>Przed zmianą trybu sprawdź czy wyświetlają się wszystkie parametry!";
    ptr += "</font>";
    ptr += "<br>";
    ptr += "<br>";
    ptr += "<p>Szerokość geograficzna: ";
    ptr += latitude_string;
    ptr += "<p>Długość geograficzna: ";
    ptr += longitiude_string;
    ptr += "<p>PM2.5: ";
    ptr += val1;
    ptr += " ug/m3</p>";
    ptr += "<p>PM10: ";
    ptr += val2;
    ptr += " ug/m3</p>";
    ptr += "<p>";
    ptr += Time;
    ptr += "<p>";
    ptr += Date;
  }

  ptr += "</p>";
  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";

  return ptr;
}

void handleOnConnect()
{
  digitalWrite(ledPin, LOW);
  server.send(200, "text/html", SendHTML(val1.toFloat(), val2.toFloat()));
}

void handleNotFound()
{
  server.send(404, "text/html", "404: Not Found");
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void logSDCard()
{
  dataMessage = "Nowy pomiar: " + String(Date) + " " + String(Time) + "\r\n" + "Szerokość geograficzna: " + String(latitude_string) + " " + "Długość geograficzna: " + String(longitiude_string) + "\r\n" + "PM10: " + String(val1) + " (ug / m3)" + "\r\n" + "PM2.5: " + String(val2) + " (ug / m3)" + "\r\n";
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

void pmsRead()
{
  while (Serial2.available())
  {
    if (pms.read(data))
    {
      val1 = data.PM_AE_UG_2_5;
      val2 = data.PM_AE_UG_10_0;
    }
  }
}

void gpsRead()
{
  while (SerialGPS.available() > 0)
  {
    if (gps.encode(SerialGPS.read()))
    {
      if (gps.time.isValid())
      {
        Time[12] = gps.time.second() / 10 + '0';
        Time[13] = gps.time.second() % 10 + '0';
        Time[9] = gps.time.minute() / 10 + '0';
        Time[10] = gps.time.minute() % 10 + '0';
        Time[6] = (gps.time.hour() + 1) / 10 + '0';
        Time[7] = (gps.time.hour() + 1) % 10 + '0';
      }

      if (gps.date.isValid())
      {
        Date[14] = (gps.date.year() / 10) % 10 + '0';
        Date[15] = gps.date.year() % 10 + '0';
        Date[9] = gps.date.month() / 10 + '0';
        Date[10] = gps.date.month() % 10 + '0';
        Date[6] = gps.date.day() / 10 + '0';
        Date[7] = gps.date.day() % 10 + '0';
      }

      if (gps.time.hour() + 1 > 23)
      {
        Time[6] = '0';
        Time[7] = '0';
      }

      if (last_second != gps.time.second())
      {
        last_second = gps.time.second();
      }

      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        latitude_string = String(latitude, 6);
        longitude = gps.location.lng();
        longitiude_string = String(longitude, 6);
      }
    }
  }
}

void pwmInput()
{
  if (digitalRead(PWM_PIN) == HIGH)
  {
    startTime = millis();
  }
  else
  {
    endTime = millis();
    dutyCycle = endTime - startTime;
    if (dutyCycle > 1500)
    {
      logSDCard();
    }
  }
}

void toggleLED()
{
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
}

void setup()
{
  Serial.begin(9600);
  Serial2.begin(9600);
  SerialGPS.begin(9600, SERIAL_8N2, 13, 12);

  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);

  attachInterrupt(digitalPinToInterrupt(buttonPin), toggleLED, FALLING);

  server.on("/", handleOnConnect);
  server.onNotFound(handleNotFound);
  server.begin();

  attachInterrupt(digitalPinToInterrupt(PWM_PIN), pwmInput, CHANGE);

  SD.begin(SD_CS);
  if (!SD.begin(SD_CS))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS))
  {
    Serial.println("ERROR - SD card initialization failed!");
    return;
  }

  File file = SD.open("/data.txt");
  if (!file)
  {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Reading data \r\n");
  }
  else
  {
    Serial.println("File already exists");
  }
  file.close();
}

void loop()
{
  pmsRead();
  gpsRead();
  server.handleClient();

  if (digitalRead(buttonPin) == LOW)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= debounceInterval)
    {
      previousMillis = currentMillis;
      toggleLED();
    }
  }
  if (ledState == LOW)
  {
    WiFi.mode(WIFI_AP);
  }
  else if (ledState == HIGH)
  {
    WiFi.mode(WIFI_OFF);
    pwmInput();
    delay(5000);
  }
}