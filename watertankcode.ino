/*********** Blynk Details ***********/
#define BLYNK_TEMPLATE_ID "TMPL3FclfTuiI"
#define BLYNK_TEMPLATE_NAME "Water level monitoring"
#define BLYNK_AUTH_TOKEN "FR_qxx5IpkzjshAfxaRV24uq2K7R_wJq"

#define BLYNK_PRINT Serial

/*********** Libraries ***********/
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>



/*********** WiFi credentials (MULTI) ***********/
const char* ssids[] = {
  "Home network",
  "Redmi",
  "Abhijit",
  "guestwifi"
};

const char* passwords[] = {
  "Shuvojithome15",
  "Shuvojit",
  "123456789",
  "Guestwifi12345"
};

const int wifiCount = sizeof(ssids) / sizeof(ssids[0]);
char auth[] = BLYNK_AUTH_TOKEN;

/*********** Firmware (GitHub OTA) ***********/
#define CURRENT_VERSION "1.0.0"

const char* firmwareUrl = "https://github.com/shuvojit566/watertank/raw/refs/heads/main/watertankcode.ino.bin";

const char* versionUrl = "https://raw.githubusercontent.com/shuvojit566/watertank/main/version.txt";

/*********** Pins ***********/
#define trigPin   D5
#define echoPin   D6
#define BuzzerPin D7
#define LED_PIN   LED_BUILTIN   // D4

/*********** Globals ***********/
BlynkTimer timer;
unsigned long previousMillis = 0;
const long blinkInterval = 1000;
unsigned long lastUpdateCheck = 0;
const unsigned long updateInterval = 120000; // 2 min

int distance;

/*********** WiFi Connect ***********/

void connectToWiFi() {
  static unsigned long lastAttempt = 0;
  static int wifiIndex = 0;

  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastAttempt >= 10000) {   // try every 8 seconds
    lastAttempt = millis();

    Serial.print("Trying WiFi: ");
    Serial.println(ssids[wifiIndex]);

    WiFi.disconnect();
    WiFi.begin(ssids[wifiIndex], passwords[wifiIndex]);

    wifiIndex++;
    if (wifiIndex >= wifiCount) {
      wifiIndex = 0;   // loop forever
    }
  }
}



/*********** OTA Setup ***********/
void setupOTA() {
  ArduinoOTA.setHostname("ESP8266-WaterLevel");
  ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    Serial.printf("OTA Progress: %u%%\r", (p * 100) / t);
  });
  ArduinoOTA.begin();
}

/*********** Version Check ***********/
String getRemoteVersion(const char* url) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String v = http.getString();
    v.trim();
    http.end();
    return v;
  }
  http.end();
  return "";
}

bool isNewer(String remote, String current) {
  int r1, r2, r3, c1, c2, c3;
  sscanf(remote.c_str(), "%d.%d.%d", &r1, &r2, &r3);
  sscanf(current.c_str(), "%d.%d.%d", &c1, &c2, &c3);
  if (r1 > c1) return true;
  if (r1 == c1 && r2 > c2) return true;
  return (r1 == c1 && r2 == c2 && r3 > c3);
}

void checkForUpdates() {
  Serial.println("Checking for firmware update...");

  WiFiClient client;

  t_httpUpdate_return ret = ESPhttpUpdate.update(
    client,
    firmwareUrl,
    CURRENT_VERSION   // version check handled automatically
  );

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed. Error (%d): %s\n",
                    ESPhttpUpdate.getLastError(),
                    ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available.");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("Update successful. Rebooting...");
      break;
  }
}

/*********** BLYNK V2 → D7 CONTROL ***********/
BLYNK_WRITE(V2) {
  int value = param.asInt();  // 1 or 0
  digitalWrite(BuzzerPin, value ? HIGH : LOW);
  Serial.println(value ? "D7 ON (Blynk)" : "D7 OFF (Blynk)");
}


/*********** Water Level Task ***********/
void sendWaterLevel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return;

  distance = (duration / 2) / 29.1;
  int level = map(distance, 22, 51, 100, 0);
  level = constrain(level, 0, 100);

  Blynk.virtualWrite(V0, level);
  Blynk.virtualWrite(V1, distance);

  Serial.printf("Distance: %d cm | Level: %d %%\n", distance, level);

  // Buzzer alert (tank empty)
  
    if (distance > 45) {
    digitalWrite(BuzzerPin, HIGH);
    Blynk.virtualWrite(V2, 1); 
  } else if (distance < 21) {
    digitalWrite(BuzzerPin, LOW);
     Blynk.virtualWrite(V2, 0);
  }

}

/*********** Setup ***********/
void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BuzzerPin, LOW);

  WiFi.mode(WIFI_STA);

  connectToWiFi();        // start cycling through WiFi networks (non-blocking)

  Blynk.config(auth);     // safe to call before WiFi
  setupOTA();             // OTA initialized once

  timer.setInterval(1000L, sendWaterLevel);
}



/*********** Loop ***********/
void loop() {
  connectToWiFi();      // always retry if not connected

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
    ArduinoOTA.handle();

    // blink LED when connected
    if (millis() - previousMillis >= blinkInterval) {
      previousMillis = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    // periodic OTA check
    if (millis() - lastUpdateCheck >= updateInterval) {
      lastUpdateCheck = millis();
      checkForUpdates();
    }
  } else {
    digitalWrite(LED_PIN, HIGH);   // solid LED = no WiFi
  }

  timer.run();   // sensors, etc.
}

