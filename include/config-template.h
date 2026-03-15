/*
 Comment out EEPROM_SIZE to disable EEPROM
*/
#define EEPROM_SIZE 3
#define EEPROM_ADDRESS_PLAYER_NUMBER 0
#define EEPROM_ADDRESS_LAST_VALUE 1
#define EEPROM_ADDRESS_LAST_LONG_REST 2

#define NORMAL_INTERVAL 0
#define ERROR_INTERVAL 100
#define WIFI_INTERVAL 250
#define FORCE_INTERVAL 500
#define PLAYER_INFO_INTERVAL 300
#define PLAYER_INTERVAL 1000

/*
 Adjust pins to board
*/
#define LED_PIN 22

/*
 3-column keypad
*/
#define ROW_PINS {16, 19, 23, 5}
#define COL_PINS {17, 4, 18}

/* 
 Uncomment, replace above and adjust if support for 4-column keypads
*/
// #define COLUMNS 4
// #define ROW_PINS {5, 18, 23, 19}
// #define COL_PINS {0, 4, 16, 17}

/*
 Uncomment and adjust if support for on/off switch (HIGH for on, LOW for off)
*/
// #define DEEPSLEEP_PIN GPIO_NUM_13

/*
 Set Game Code, adjust to server
*/
#define HOST "gloomhaven-secretariat.de"
#define PORT 8443
#define URL "/game/command"
#define GAME_CODE ""
/*
 Comment TLS out to not use secure connection
 */
#define TLS true

/*
 Set WiFi
*/
#define WIFI_SSID ""
#define WIFI_PSWD ""