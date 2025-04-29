#include <Arduino.h>
#include <Wire.h> 
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <wifipasswd.h>  // Wi-Fi credentials
#include <apikeys.h>     // OpenWeatherMap API key
#include <mqttconfig.h>  // MQTT configuration
#include <icons.h>     // Icons for the display

// Uncomment to enable the debug serial print
#define SERIALPRINT

// I²C Addresses
// I2C device found at address 0x3C - OLED
// I2C device found at address 0x77 - BMP180


// I²C Controller
#define I2C_SDA 14
#define I2C_SCL 12

// DHT Sensor
#define DHTPIN 4       // Substitua pelo pino que você usou
#define DHTTYPE DHT22   // ou DHT11
DHT dht(DHTPIN, DHTTYPE);

// I²C Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I²C BMP180 Sensor
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// Fuso horário (UTC-3)
const long utcOffsetInSeconds = -10800;

// NTP Server List. Change to your preferred servers
const char* ntpServers[] = {
    "scarlett",                         // Local NTP Server
    "a.ntp.br","b.ntp.br","c.ntp.br",   // Official Brazilian NTP Server
    "time.nist.gov",                    // USA NTP Server
    "pool.ntp.org"                      // NTP Pool
};
const int numNTPServers = sizeof(ntpServers) / sizeof(ntpServers[0]);
int ntpSrvIndex = 0;
int numRedes = sizeof(ssids) / sizeof(ssids[0]);  // Number of Wi-Fi networks in wifipasswd.h

// Network Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServers[ntpSrvIndex], utcOffsetInSeconds, 60000);
WiFiClientSecure client;
PubSubClient mqtt(client);

/* 
 * GLOBAL VARIABLES 
 */


// General variables
int day, month, year, hour, minute, second, dayOfWeek;
unsigned long  lastRTCsync = 0;
bool wifiConnected = false, ntpConnected = false;
int lastUIScreen = 0;
unsigned long lastUIMillis = 0;
unsigned long lastBMERead = 0;
unsigned int backgroundColor = 0;
unsigned int foregroundColor = 1;

// Weather variables
float tmp, hum, pres ,qnh, tmpbmp, tmpdht;
int alt = 935;

bool tryWIFI() {
    bool connOK = false;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Limpa conexões anteriores
    delay(100);
    #ifdef SERIALPRINT
    Serial.println("Escaneando redes...");
    #endif
    int n = WiFi.scanNetworks();
    if (n == 0) {
      #ifdef SERIALPRINT
      Serial.println("Nenhuma rede encontrada.");
      #endif
      return connOK;
    }

    for (int i = 0; i < numRedes; i++) {
        #ifdef SERIALPRINT
        Serial.print("Tentando conectar em ");
        Serial.print(ssids[i]);
        #endif
        bool found = false;
        for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == ssids[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            #ifdef SERIALPRINT
            Serial.println(" - Rede não encontrada.");
            #endif
            continue;  // Skip to the next SSID if not found
        }
        WiFi.begin(ssids[i], passwords[i]);

        int tentativa = 0;
        // Retry connection up to 10 seconds (10 attempts)
        while (WiFi.status() != WL_CONNECTED && tentativa < 20) {
            delay(500);
            #ifdef SERIALPRINT            
            Serial.print(".");
            #endif
            tentativa++;
        }
        
        // If connected successfully to Wi-Fi
        if (WiFi.status() == WL_CONNECTED) {
            #ifdef SERIALPRINT
            Serial.printf("\nConectado em %s\n", ssids[i]);
            #endif
            connOK = true;
            break;  // Exit loop if connection is successful
        } else {
            #ifdef SERIALPRINT
            Serial.printf("\nFalha ao conectar no Wi-Fi: %s\n", ssids[i]);
            #endif
        }
    }
    return connOK;
}

/*
 * tryNTPServer() - Tries to connect to a list of NTP servers
 * 
 * This function attempts to establish a connection with a list of NTP (Network Time Protocol)
 * servers to synchronize the time. It loops through the predefined list of NTP server addresses
 * and tries to update the time using each server. If a successful connection is made, it returns
 * the index of the server that was successfully connected to. If all servers fail, it returns -1.
 */
int tryNTPServer() {
    for (int i = 0; i < numNTPServers; i++) {
        timeClient.setPoolServerName(ntpServers[i]);
        timeClient.begin();
        if (timeClient.update()) {
            #ifdef SERIALPRINT
            Serial.printf("Conexão com NTP bem-sucedida: %s\n ", ntpServers[i]);
            #endif
            return i;
        } else {
            #ifdef SERIALPRINT
            Serial.printf("Erro ao conectar no NTP: %s\n", ntpServers[i]);
            #endif
        }
    }
    // Resets the board
    ESP.restart();
    return -1;
}

/*
*  removeAccents() - Removes accents from a string
*
*  This function takes an UTF-8 string as input and removes any accents from the characters.
*  It is necessary for the correct display of characters on the LCD.
*/
void removeAccents(char* str) {
    char* src = str;
    char* dst = str;
  
    while (*src) {
      // Se encontrar caractere UTF-8 multibyte (início com 0xC3)
      if ((uint8_t)*src == 0xC3) {
        src++;  // Avança para o próximo byte
        switch ((uint8_t)*src) {
          case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4:  *dst = 'a'; break; // àáâãä
          case 0x80: case 0x81: case 0x82: case 0x83: case 0x84:  *dst = 'A'; break; // ÀÁÂÃÄ
          case 0xA7: *dst = 'c'; break; // ç
          case 0x87: *dst = 'C'; break; // Ç
          case 0xA8: case 0xA9: case 0xAA: case 0xAB: *dst = 'e'; break; // èéêë
          case 0x88: case 0x89: case 0x8A: case 0x8B: *dst = 'E'; break; // ÈÉÊË
          case 0xAC: case 0xAD: case 0xAE: case 0xAF: *dst = 'i'; break; // ìíîï
          case 0x8C: case 0x8D: case 0x8E: case 0x8F: *dst = 'I'; break; // ÌÍÎÏ
          case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: *dst = 'o'; break; // òóôõö
          case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: *dst = 'O'; break; // ÒÓÔÕÖ
          case 0xB9: case 0xBA: case 0xBB: case 0xBC: *dst = 'u'; break; // ùúûü
          case 0x99: case 0x9A: case 0x9B: case 0x9C: *dst = 'U'; break; // ÙÚÛÜ
          case 0xB1: *dst = 'n'; break; // ñ
          case 0x91: *dst = 'N'; break; // Ñ
          default: *dst = '?'; break; // desconhecido
        }
        dst++;
        src++; // Pula o segundo byte do caractere especial
      } else {
        *dst++ = *src++; // Copia byte normal
      }
    }
    *dst = '\0'; // Termina a nova string
  }

  void upperFirstLetter(char* str) {
    if (str && str[0] != '\0') {  // Checks for empty string        
      str[0] = toupper(str[0]);  // Convert the first character to uppercase
    }
  }



void getDateFromEpoch(time_t epoch, int &day, int &month, int &year) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo);

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
}

void getDayOfWeekFromEpoch(time_t epoch, int &dayOfWeek) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo); 

    dayOfWeek = timeinfo.tm_wday;
}


void getTimeFromEpoch(time_t epoch, int &hour, int &minute, int &second) {
    struct tm timeinfo;
    gmtime_r(&epoch, &timeinfo); 

    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;
}


float calculateQNH(float pressure, float temperature, float altitude) {
    const float L = 0.0065;        // Gradiente térmico padrão (°C/m)
    const float T0 = temperature + 273.15;  // Temperatura absoluta (K)
    const float g = 9.80665;
    const float M = 0.0289644;
    const float R = 8.3144598;

    float exponent = (g * M) / (R * L);
    float seaLevelPressure = pressure * pow(1 - (L * altitude) / T0, -exponent);
    return seaLevelPressure;
}

unsigned long lastBMPRead = 0;
const unsigned long BMP_READ_INTERVAL = 10000; // 10 seconds
void readBMP() {
    if (millis() - lastBMPRead > BMP_READ_INTERVAL || lastBMPRead == 0) {
        lastBMPRead = millis();
        sensors_event_t event;
        bmp.getEvent(&event);
        if (event.pressure) {
            float temperature;
            bmp.getTemperature(&temperature);

            #ifdef SERIALPRINT
            Serial.print("BMP180: Temp: ");
            Serial.print(temperature);
            Serial.print(" *C ");
            Serial.print("Pres: ");
            Serial.print(event.pressure);
            Serial.println(" hPa");
            #endif
            pres = event.pressure;
            tmpbmp = temperature;
            tmp = (temperature + tmp)/2;

          } else {
            Serial.println("Erro ao ler a pressão.");
          }



        qnh = calculateQNH(pres, tmp, alt);

    }
}

unsigned long lastDHTRead = 0;
const unsigned long DHT_READ_INTERVAL = 10000; // 10 seconds
void readDHT() {
    if (millis() - lastDHTRead > DHT_READ_INTERVAL || lastDHTRead == 0) {
        lastDHTRead = millis();
        float h = dht.readHumidity();
        float t = dht.readTemperature(); // Celsius
    
        if (isnan(h) || isnan(t)) {
        Serial.println("Falha ao ler do DHT!");
        } else {
            #ifdef SERIALPRINT
            Serial.print("DHT-22: Temp: ");
            Serial.print(t);
            Serial.print(" *C Umid: ");
            Serial.print(h);
            Serial.println("%");
            #endif
            hum = h;
            tmp = (tmp + t)/2;
            tmpdht = t;
        }
    }
}

void readNTP() {
    timeClient.update();
    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    second = timeClient.getSeconds();
    getDateFromEpoch(timeClient.getEpochTime(), day, month, year);
    dayOfWeek = timeClient.getDay();
}


bool checkConnections() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
    }
    else {
        wifiConnected = tryWIFI();
        ntpConnected = false;
    }
    if (wifiConnected) {
        if (timeClient.update()) {
            ntpConnected = true;
        }
        else {
            ntpConnected = tryNTPServer();
        }
    }
    return wifiConnected && ntpConnected;
}

long lastMQTTMillis = 0;
void sendSensorData(float tmp, float hum, float pres) {
    if (millis() - lastMQTTMillis > 30000) {
        lastMQTTMillis = millis();
        display.fillRect(0, 0, display.width(), 16, backgroundColor);
        display.setCursor(0, 0);
        display.printf(" Enviando ");
        display.display();
        // Check if connected to MQTT
        if (!mqtt.connected()) {
            #ifdef SERIALPRINT
            Serial.println("Conectando ao MQTT...");
            #endif
            if (mqtt.connect("esp2", mqtt_user, mqtt_pass)) {
                #ifdef SERIALPRINT
                Serial.println("Conectado ao MQTT");
                #endif
            } else {
                #ifdef SERIALPRINT
                Serial.print("Falha ao conectar ao MQTT, rc=");
                Serial.print(mqtt.state());
                #endif
                ESP.restart();
                return;
            }
        }

        // Messages variables
        char tmp_str[8];
        char hum_str[8];
        char pres_str[8];
        char tmpbmp_str[8];
        char tmpdht_str[8];

        // Convert to string
        dtostrf(tmp, 6, 2, tmp_str);
        dtostrf(hum, 6, 2, hum_str);
        dtostrf(pres, 6, 2, pres_str);
        dtostrf(tmpbmp, 6, 2, tmpbmp_str);
        dtostrf(tmpdht, 6, 2, tmpdht_str);

        // Publish on MQTT
        mqtt.publish("esp2/sensor/temperature", tmp_str);
        mqtt.publish("esp2/sensor/humidity", hum_str);
        mqtt.publish("esp2/sensor/pressure", pres_str);
        mqtt.publish("esp2/sensor/tmpbmp", tmpbmp_str);
        mqtt.publish("esp2/sensor/tmpdht", tmpdht_str);

        #ifdef SERIALPRINT
        Serial.println("Dados enviados ao broker:");
        Serial.print("Temp: "); Serial.println(tmp_str);
        Serial.print("Umid: "); Serial.println(hum_str);
        Serial.print("Pres: "); Serial.println(pres_str);
        Serial.print("Tbmp: "); Serial.println(tmpbmp_str);
        Serial.print("Tdht: "); Serial.println(tmpdht_str);
        #endif
        display.fillRect(0, 0, display.width(), 16, backgroundColor);
        display.display();

    }
}


void setup() {
    Wire.begin(I2C_SDA,I2C_SCL);
    Serial.begin(115200);
    Serial.println("");
    Serial.println("Iniciando...");
    client.setInsecure(); // Usa a verificação de certificado SSL sem precisar armazená-lo

     // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    

    // Clear the buffer
    display.clearDisplay();

    // Setup display for text
    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font
    display.printf("Iniciando\n");
    
    if (!bmp.begin()) {
        Serial.println("Não foi possível encontrar o sensor BMP180.");
      } else {
      Serial.println("BMP180 encontrado!");
      }

    // Show the display buffer on the screen. You MUST call display() after
    // drawing commands to make them visible on screen!
    display.display();
 

    wifiConnected = tryWIFI();

    if (wifiConnected) {
        display.setTextSize(1);
        display.setCursor(0, 16);
        display.printf("Rede: %s\n", WiFi.SSID().c_str());
        display.setCursor(0, 24);
        display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        display.display();
        ntpSrvIndex = tryNTPServer();
        display.setCursor(0,32);
        display.printf("NTP: %s\n", ntpServers[ntpSrvIndex]);
        display.setTextSize(2);
        display.display();
    } 
    else {
        ntpSrvIndex = -1;
    }

    

    // Configure the MQTT server
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setKeepAlive(60);

    dht.begin();
    


}

void testdrawchar(void) {

  
    // Not all the characters will fit on the display. This is normal.
    // Library will draw what it can and the rest will be clipped.
    for(int16_t i=0; i<256; i++) {
      if(i == '\n') display.write(' ');
      else          display.write(i);
    }
  
    display.display();
    delay(2000);
  }

void printClock() {
    char timestring[9];
    int16_t x1, y1;
    uint16_t w, h;
    sprintf(timestring, "%02d:%02d:%02d", hour, minute, second);
    display.fillRect(0, 0, SCREEN_WIDTH, 16, backgroundColor); // limpa a parte amarela
    display.setTextSize(2);
    display.getTextBounds(timestring, 0, 0, &x1, &y1, &w, &h);
    int centerX = (display.width() - w) / 2;
    display.setCursor(centerX, 0);
    display.print(timestring);
    #ifdef SERIALPRINT
    Serial.println(timestring);
    #endif
    display.display();
}

void printWeather() {
    display.fillRect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, backgroundColor); // limpa a parte azul
    display.setTextSize(2);
    display.setCursor(2, 16);
    display.printf("T: %.1f C\n", tmp);
    display.setCursor(2, 32);
    display.printf("U: %.1f %%\n", hum); 
    display.setCursor(2, 48);
    display.printf("P: %.0fhPa\n", qnh);
    display.display();
}

void printNetwork() {
    display.fillRect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, backgroundColor); // limpa a parte azul
    display.setTextSize(1);
    display.setCursor(2, 16);
    display.printf("Rede: %s\n", WiFi.SSID().c_str());
    display.setCursor(2, 32);
    display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    display.setCursor(2, 48);
    display.printf("NTP: %s\n", ntpServers[ntpSrvIndex]);
    display.display();
}

unsigned long lastMillis = 0;
void loop() {
    if (millis() - lastMillis > 500) {
        lastMillis = millis();
        readBMP();
        readDHT();
        readNTP();
        sendSensorData(tmp, hum, qnh);
        if (second <11) {
            printNetwork();
        } else {
            printWeather();
        }
        printClock();
        
        if (hour > 5 && hour < 18) {
            foregroundColor = SSD1306_WHITE;
            backgroundColor = SSD1306_BLACK;
        } else {
            
            foregroundColor = SSD1306_BLACK;
            backgroundColor = SSD1306_WHITE;}
        }
        display.setTextColor(foregroundColor);
}



