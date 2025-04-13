#include <Arduino.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Keypad.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>

#include "driver/rtc_io.h"

// create config.h from config-template.h file
#include <config.h>

#define RESULT_OK 0
#define RESULT_ERROR 1

const byte rows = 4;
const byte cols = 3;
char keys[rows][cols] = {
    {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}};

byte rowPins[rows] = ROW_PINS;
byte colPins[cols] = COL_PINS;

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

int initiative = -1;
bool longRest = false;
int lastInitiative = -1;
bool lastLongRest = false;

int playerNumber = 0;
int resetCount = 0;

long ledInterval = PLAYER_INTERVAL;
long lastMillis = 0;
long resetMillis = 0;
int ledState = LOW;

WiFiMulti wifiMulti;
WiFiClientSecure client;

uint8_t setInitiative() {
  longRest = false;
  if (initiative < 0) {
    initiative = 0;
  }

  if (initiative > 99) {
    initiative = 99;
    longRest = true;
  }

  if (initiative == lastInitiative && longRest == lastLongRest) {
    return RESULT_ERROR;
  }

  return RESULT_OK;
}

uint8_t postInitiative() {
  Serial.print("POST... ");

  String jsonString = "{\"playerNumber\":";
  jsonString += playerNumber;
  jsonString += ",\"initiative\":";
  jsonString += initiative;
  if (longRest) {
    jsonString += ",\"longRest\":true";
  }
  jsonString += "}";

  Serial.println(jsonString);

  int contentLength = jsonString.length();

  if (!client.connect(HOST, PORT)) {
    Serial.println("CONNECTION FAILED!");
    return RESULT_ERROR;
  }

  client.print("POST ");
  client.print(URL);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(HOST);
  client.println("Accept: application/json");
  client.println("Content-Type: application/json");
  client.print("Authorization: ");
  client.println(GAME_CODE);
  client.print("Content-Length: ");
  client.println(contentLength);
  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    client.stop();
    return RESULT_ERROR;
  }

  int cIndex;
  for (cIndex = 0; cIndex < contentLength; cIndex = cIndex + 1000) {
    client.print(jsonString.substring(cIndex, cIndex + 1000));
  }

  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));

  int statusCode = 0;
  statusCode += (status[9] - '0') * 100;
  statusCode += (status[10] - '0') * 10;
  statusCode += (status[11] - '0');

  if (statusCode != 200) {
    Serial.print(F("Invalid Status Code:"));
    Serial.println(statusCode);
    client.stop();
    return RESULT_ERROR;
  }

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    client.stop();
    return RESULT_ERROR;
  }
  client.stop();
  Serial.println("Success!");
  return RESULT_OK;
}

void blinkLED() {
  if (ledInterval > 0 && millis() - lastMillis > ledInterval) {
    lastMillis = millis();
    if (ledState == HIGH) {
      ledState = LOW;
    } else {
      ledState = HIGH;
    }
    digitalWrite(LED_PIN, ledState);
  } else if (!ledInterval) {
    ledState = LOW;
    digitalWrite(LED_PIN, ledState);
  }

  if (resetMillis > 0 && millis() > resetMillis) {
    resetMillis = 0;
    ledInterval = NORMAL_INTERVAL;
    ledState = LOW;
    digitalWrite(LED_PIN, ledState);
    if (playerNumber < 1) {
      ledInterval = PLAYER_INTERVAL;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

#ifdef DEEPSLEEP_PIN
  pinMode(DEEPSLEEP_PIN, INPUT);
  rtc_gpio_pullup_en(DEEPSLEEP_PIN);
  rtc_gpio_pulldown_dis(DEEPSLEEP_PIN);
  esp_sleep_enable_ext0_wakeup(DEEPSLEEP_PIN, HIGH);

  if (digitalRead(DEEPSLEEP_PIN) == LOW) {
    esp_deep_sleep_start();
  }
#endif

  digitalWrite(LED_PIN, HIGH);
  delay(500);
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PSWD);
#if defined(WIFI_SSID2) && defined(WIFI_PSWD2)
  wifiMulti.addAP(WIFI_SSID2, WIFI_PSWD2);
#endif
#if defined(WIFI_SSID3) && defined(WIFI_PSWD3)
  wifiMulti.addAP(WIFI_SSID3, WIFI_PSWD3);
#endif

  Serial.println("\nConnecting to Wifi");
  int counter = 0;
  while (wifiMulti.run() != WL_CONNECTED && counter < 120) {
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    Serial.print(".");
    counter += 1;
    if (counter % 20 == 0) {
      Serial.println();
    }
  }
  digitalWrite(LED_PIN, LOW);

  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("\"");
    Serial.print(WiFi.SSID());
    Serial.print("\" IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println();
  } else {
    Serial.println("\nCould not connect to Wi-Fi");
    ledState = WIFI_INTERVAL;
  }

  client.setInsecure();  // in general this is bad, but GHS data is not
                         // important and this will prevent issues with local
                         // SSL server
  client.setTimeout(5000);

#ifdef EEPROM_SIZE
  EEPROM.begin(EEPROM_SIZE);
  playerNumber = EEPROM.read(EEPROM_ADDRESS_PLAYER_NUMBER);
  if (playerNumber > 9) {
    playerNumber = 0;
    EEPROM.write(EEPROM_ADDRESS_PLAYER_NUMBER, playerNumber);
    EEPROM.commit();
  }
  Serial.print("Read playernumber: ");
  Serial.println(playerNumber);
  if (playerNumber > 0) {
    delay(FORCE_INTERVAL);
    ledInterval = PLAYER_INFO_INTERVAL;
    resetMillis = millis() + playerNumber * PLAYER_INFO_INTERVAL * 2;
  }

  lastInitiative = EEPROM.read(EEPROM_ADDRESS_LAST_INITIATIVE);
  if (lastInitiative > 99) {
    lastInitiative = -1;
    EEPROM.write(EEPROM_ADDRESS_LAST_INITIATIVE, lastInitiative);
    EEPROM.commit();
  }
  Serial.print("Read lastInitiative: ");
  Serial.println(lastInitiative);

  lastLongRest = EEPROM.read(EEPROM_ADDRESS_LAST_LONG_REST);
  if (lastLongRest != 0 && lastLongRest != 1) {
    lastLongRest = false;
    EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
    EEPROM.commit();
  }
  Serial.print("Read lastLongRest: ");
  Serial.println(lastLongRest);
#endif
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    ledInterval = WIFI_INTERVAL;
  }

  char key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.print("Pressed: ");
    Serial.println(key);
    if (key == '*') {
      initiative = -1;
      if (playerNumber > 0 && resetCount < 3) {
        resetCount += 1;
      }
      if (resetCount > 1 && playerNumber > 0) {
        resetMillis = 0;
        ledInterval = FORCE_INTERVAL;
      } else if (playerNumber > 0) {
        ledInterval = PLAYER_INFO_INTERVAL;
        resetMillis = millis() + playerNumber * PLAYER_INFO_INTERVAL * 2;
      }

      if (resetCount > 2) {
        Serial.println("Reset player");
        playerNumber = 0;
        resetCount = 0;
        lastInitiative = -1;
        lastLongRest = false;
        ledInterval = PLAYER_INTERVAL;

#ifdef EEPROM_SIZE
        EEPROM.write(EEPROM_ADDRESS_PLAYER_NUMBER, playerNumber);
        EEPROM.write(EEPROM_ADDRESS_LAST_INITIATIVE, lastInitiative);
        EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
        EEPROM.commit();
#endif
      }
    } else if (resetCount > 1) {
      resetCount = 0;
      if (playerNumber > 0) {
        ledInterval = NORMAL_INTERVAL;
      } else {
        ledInterval = PLAYER_INTERVAL;
      }
    } else if (key == '#') {
      if (playerNumber > 0) {
        ledState = HIGH;
        digitalWrite(LED_PIN, ledState);

        if (setInitiative()) {
          ledInterval = ERROR_INTERVAL;
          resetMillis = millis() + 3000;
          initiative = -1;
          longRest = false;
          lastInitiative = initiative;
          lastLongRest = longRest;

#ifdef EEPROM_SIZE
          EEPROM.write(EEPROM_ADDRESS_LAST_INITIATIVE, lastInitiative);
          EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
          EEPROM.commit();
#endif
        } else if (postInitiative()) {
          ledInterval = ERROR_INTERVAL;
          initiative = -1;
          longRest = false;
          lastInitiative = initiative;
          lastLongRest = longRest;

#ifdef EEPROM_SIZE
          EEPROM.write(EEPROM_ADDRESS_LAST_INITIATIVE, lastInitiative);
          EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
          EEPROM.commit();
#endif
        } else {
          lastInitiative = initiative;
          lastLongRest = longRest;
          initiative = -1;
          longRest = false;
          ledInterval = NORMAL_INTERVAL;

#ifdef EEPROM_SIZE
          EEPROM.write(EEPROM_ADDRESS_LAST_INITIATIVE, lastInitiative);
          EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
          EEPROM.commit();
#endif
        }
      } else {
        ledInterval = FORCE_INTERVAL;
        resetMillis = millis() + 4000;
      }
    } else {
      uint8_t value = key - '0';
      resetCount = 0;
      ledInterval = NORMAL_INTERVAL;

      if (playerNumber == 0) {
        playerNumber = value;

#ifdef EEPROM_SIZE
        EEPROM.write(EEPROM_ADDRESS_PLAYER_NUMBER, playerNumber);
        EEPROM.commit();
#endif

        Serial.print("Select player: ");
        Serial.println(playerNumber);
      } else {
        if (initiative == 0 && value == 0) {
          initiative = 100;
        } else if (initiative < 1 || initiative > 9) {
          initiative = value;
        } else {
          initiative = initiative * 10;
          initiative += value;
        }
      }
    }
  }

#ifdef DEEPSLEEP_PIN
  if (digitalRead(DEEPSLEEP_PIN) == LOW) {
    Serial.println("Sleep...");
    esp_deep_sleep_start();
  }
#endif

  blinkLED();
}