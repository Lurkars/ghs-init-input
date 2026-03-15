#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <Keypad.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "driver/rtc_io.h"

// create config.h from config-template.h file
#include <config.h>

#define RESULT_OK 0
#define RESULT_ERROR 1

#ifndef COLUMNS
#define COLUMNS 3
#endif

const byte rows = 4;
const byte cols = COLUMNS;

#if COLUMNS == 4
char keys[4][4] = {{'1', '2', '3', 'A'},
                   {'4', '5', '6', 'B'},
                   {'7', '8', '9', 'C'},
                   {'*', '0', '#', 'D'}};
#else
char keys[4][3] = {
    {'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}};
#endif

byte rowPins[rows] = ROW_PINS;
byte colPins[cols] = COL_PINS;

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

// ============================================================
// Input mode: A = Initiative (default)
// ============================================================
char currentMode = 'A';

// Mode A: Initiative
bool longRest = false;
bool lastLongRest = false;

// Mode B: Hit points, Expernience, Loot
int bModeValue = 0;
int hpValue = 0;
int xpValue = 0;
int lootValue = 0;

// Mode C: Conditions
bool conditionSubmitted = false;

// Mode D: Identities, WIP

// Persistent values
int value = -1;
int lastValue = -1;
int playerNumber = 0;
int resetCount = 0;

long ledInterval = PLAYER_INTERVAL;
long lastMillis = 0;
long resetMillis = 0;
int ledState = LOW;

WiFiMulti wifiMulti;

#if defined(TLS)
#include <WiFiClientSecure.h>
WiFiClientSecure client;
#else
#include <WiFiClient.h>
WiFiClient client;
#endif

// ============================================================
// Mode A: Initiative
// ============================================================

uint8_t setInitiative() {
  longRest = false;
  if (value < 0) {
    value = 0;
  }

  if (value > 99) {
    value = 99;
    longRest = true;
  }

  if (value == lastValue && longRest == lastLongRest) {
    return RESULT_ERROR;
  }

  return RESULT_OK;
}

uint8_t postCommand(JsonDocument command) {
  String commandJson;
  serializeJson(command, commandJson);
  int contentLength = commandJson.length();

  Serial.print("POST... ");
  Serial.println(commandJson);

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
    client.print(commandJson.substring(cIndex, cIndex + 1000));
  }

  char status[32] = {0};
  int bytesRead = client.readBytesUntil('\r', status, sizeof(status) - 1);

  if (bytesRead < 12 || !isdigit(status[9]) || !isdigit(status[10]) ||
      !isdigit(status[11])) {
    Serial.print(F("Invalid HTTP response (bytes read: "));
    Serial.print(bytesRead);
    Serial.print(F("): "));
    Serial.println(status);
    client.stop();
    return RESULT_ERROR;
  }

  int statusCode =
      (status[9] - '0') * 100 + (status[10] - '0') * 10 + (status[11] - '0');

  if (statusCode != 200) {
    Serial.print(F("Invalid Status Code: "));
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

uint8_t postInitiative() {
  JsonDocument initiativeCommand;
  initiativeCommand["id"] = "character.initiative";
  initiativeCommand["parameters"][0] = playerNumber;
  initiativeCommand["parameters"][1] = value;
  if (longRest) {
    initiativeCommand["parameters"][2] = true;
  }
  return postCommand(initiativeCommand);
}

void modeA_onClear() { value = -1; }

void modeA_onDigit(uint8_t digit) {
  if (value == 0 && digit == 0) {
    value = 100;
  } else if (value < 1 || value > 9) {
    value = digit;
  } else {
    value = value * 10;
    value += digit;
  }
}

void modeA_onSubmit() {
  if (setInitiative() != RESULT_OK) {
    Serial.println("Initiative unchanged or invalid");
    ledInterval = ERROR_INTERVAL;
    resetMillis = millis() + 3000;
    value = -1;
    longRest = false;
    lastValue = value;
    lastLongRest = longRest;

#ifdef EEPROM_SIZE
    EEPROM.write(EEPROM_ADDRESS_LAST_VALUE, lastValue);
    EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
    EEPROM.commit();
#endif
    return;
  }

  if (postInitiative() != RESULT_OK) {
    Serial.println("Post initiative failed");
    ledInterval = ERROR_INTERVAL;
    value = -1;
    longRest = false;
    lastValue = value;
    lastLongRest = longRest;

#ifdef EEPROM_SIZE
    EEPROM.write(EEPROM_ADDRESS_LAST_VALUE, lastValue);
    EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
    EEPROM.commit();
#endif
    return;
  }

  lastValue = value;
  lastLongRest = longRest;
  value = -1;
  longRest = false;
  ledInterval = NORMAL_INTERVAL;

#ifdef EEPROM_SIZE
  EEPROM.write(EEPROM_ADDRESS_LAST_VALUE, lastValue);
  EEPROM.write(EEPROM_ADDRESS_LAST_LONG_REST, lastLongRest);
  EEPROM.commit();
#endif
}

// ============================================================
// Mode B:  Hit points, Expernience, Loot
// ============================================================

uint8_t postHp() {
  JsonDocument hpCommand;
  hpCommand["id"] = "character.hp";
  hpCommand["parameters"][0] = playerNumber;
  hpCommand["parameters"][1] = hpValue;
  return postCommand(hpCommand);
}

uint8_t postXp() {
  JsonDocument xpCommand;
  xpCommand["id"] = "character.xp";
  xpCommand["parameters"][0] = playerNumber;
  xpCommand["parameters"][1] = xpValue;
  return postCommand(xpCommand);
}

uint8_t postLoot() {
  JsonDocument lootCommand;
  lootCommand["id"] = "character.loot";
  lootCommand["parameters"][0] = playerNumber;
  lootCommand["parameters"][1] = lootValue;
  return postCommand(lootCommand);
}

uint8_t postLootDraw() {
  JsonDocument lootDrawCommand;
  lootDrawCommand["id"] = "character.loot.draw";
  lootDrawCommand["parameters"][0] = playerNumber;
  return postCommand(lootDrawCommand);
}

void modeB_onClear() {
  bModeValue = 0;
  hpValue = 0;
  xpValue = 0;
  lootValue = 0;
}

void modeB_onDigit(uint8_t digit) {
  switch (digit) {
    case 1:
      bModeValue = 1;
      hpValue--;
      xpValue = 0;
      lootValue = 0;
      break;
    case 2:
      bModeValue = 1;
      hpValue = 0;
      xpValue = 0;
      lootValue = 0;
      break;
    case 3:
      bModeValue = 1;
      hpValue++;
      xpValue = 0;
      lootValue = 0;
      break;
    case 4:
      bModeValue = 2;
      xpValue--;
      hpValue = 0;
      lootValue = 0;
      break;
    case 5:
      bModeValue = 2;
      xpValue = 0;
      hpValue = 0;
      lootValue = 0;
      break;
    case 6:
      bModeValue = 2;
      xpValue++;
      hpValue = 0;
      lootValue = 0;
      break;
    case 7:
      bModeValue = 3;
      lootValue--; 
      hpValue = 0;
      xpValue = 0; 
      break;
    case 8:
      bModeValue = 3;
      lootValue = 0;
      hpValue = 0;
      xpValue = 0;
      break;
    case 9:
      bModeValue = 3;
      lootValue++;
      hpValue = 0;
      xpValue = 0;
      break;
    case 0:
      bModeValue = 4;
      hpValue = 0;
      xpValue = 0;
      lootValue = 0;
      break;
  }
}
void modeB_onSubmit() {
  if (bModeValue == 1 && hpValue != 0 && postHp() != RESULT_OK) {
    Serial.println("Post HP failed");
    ledInterval = ERROR_INTERVAL;
    return;
  } else if (bModeValue == 2 && xpValue != 0 && postXp() != RESULT_OK) {
    Serial.println("Post XP failed");
    ledInterval = ERROR_INTERVAL;
    return;
  } else if (bModeValue == 3 && lootValue != 0 &&
             postLoot() != RESULT_OK) {
    Serial.println("Post Loot failed");
    ledInterval = ERROR_INTERVAL;
    return;
  } else if (bModeValue == 4 && postLootDraw() != RESULT_OK) {
    Serial.println("Post Loot Draw failed");
    ledInterval = ERROR_INTERVAL;
    return;
  }
  modeB_onClear();
}

// ============================================================
// Mode C: Conditions
// ============================================================

uint8_t postCondition() {
  JsonDocument conditionCommand;
  conditionCommand["id"] = "character.condition";
  conditionCommand["parameters"][0] = playerNumber;
  if (value == 0) {
    conditionCommand["parameters"][1] = 9;
  } else {
    conditionCommand["parameters"][1] = value - 1;
  }
  return postCommand(conditionCommand);
}

void modeC_onClear() { value = -1; }

void modeC_onDigit(uint8_t digit) {
  if (value < 1 || value > 9 || conditionSubmitted) {
    value = digit;
  } else {
    value = value * 10;
    value += digit;
  }
  conditionSubmitted = false;
}
void modeC_onSubmit() {
  if (postCondition() != RESULT_OK) {
    Serial.println("Post Condition failed");
    ledInterval = ERROR_INTERVAL;
    return;
  }
  conditionSubmitted = true;
}

// ============================================================
// Mode D: Identities, WIP
// ============================================================

uint8_t postIdentity(uint8_t value) {
  JsonDocument identityCommand;
  identityCommand["id"] = "character.identity";
  identityCommand["parameters"][0] = playerNumber;
  identityCommand["parameters"][1] = value - 1;
  return postCommand(identityCommand);
}

void modeD_onClear() {}
void modeD_onDigit(uint8_t digit) {
  if (postIdentity(digit) != RESULT_OK) {
    Serial.println("Post initiative failed");
    ledInterval = ERROR_INTERVAL;
    return;
  }
}
void modeD_onSubmit() {}

// ============================================================
// Mode dispatch helpers
// ============================================================

void onClear() {
  switch (currentMode) {
    case 'B':
      modeB_onClear();
      break;
    case 'C':
      modeC_onClear();
      break;
    case 'D':
      modeD_onClear();
      break;
    default:
      modeA_onClear();
      break;  // 'A' and 3-col keypads
  }
}

void onDigit(uint8_t value) {
  switch (currentMode) {
    case 'B':
      modeB_onDigit(value);
      break;
    case 'C':
      modeC_onDigit(value);
      break;
    case 'D':
      modeD_onDigit(value);
      break;
    default:
      modeA_onDigit(value);
      break;
  }
}

void onSubmit() {
  switch (currentMode) {
    case 'B':
      modeB_onSubmit();
      break;
    case 'C':
      modeC_onSubmit();
      break;
    case 'D':
      modeD_onSubmit();
      break;
    default:
      modeA_onSubmit();
      break;
  }
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

#if defined(TLS)
  client.setInsecure();  // in general this is bad, but GHS data is not
                         // important and this will prevent issues with local
                         // SSL server
#endif
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

  lastValue = EEPROM.read(EEPROM_ADDRESS_LAST_VALUE);
  if (lastValue > 99) {
    lastValue = -1;
    EEPROM.write(EEPROM_ADDRESS_LAST_VALUE, lastValue);
    EEPROM.commit();
  }
  Serial.print("Read lastValue: ");
  Serial.println(lastValue);

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
      onClear();
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
        lastValue = -1;
        lastLongRest = false;
        ledInterval = PLAYER_INTERVAL;

#ifdef EEPROM_SIZE
        EEPROM.write(EEPROM_ADDRESS_PLAYER_NUMBER, playerNumber);
        EEPROM.write(EEPROM_ADDRESS_LAST_VALUE, lastValue);
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
        onSubmit();
      } else {
        ledInterval = FORCE_INTERVAL;
        resetMillis = millis() + 4000;
      }
    } else if (key >= 'A' && key <= 'D') {
      // Mode switch (4-column keypads only)
      resetCount = 0;
      ledInterval = NORMAL_INTERVAL;
      onClear();  // reset previous mode input
      currentMode = key;
      Serial.print("Mode: ");
      Serial.println(currentMode);
    } else {
      // Digit input
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
        onDigit(value);
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