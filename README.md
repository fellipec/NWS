# Mini Estação Meteorológica com NodeMCU, BMP180 e DHT-22

Este projeto é uma mini estação meteorológica que utiliza um NodeMCU, um sensor BMP180 e um sensor DHT-22 para medir temperatura, umidade e pressão atmosférica. Os dados são enviados para um broker MQTT.

## Componentes Utilizados

- NodeMCU (ESP8266)
- Sensor de pressão BMP180
- Sensor de temperatura e umidade DHT-22
- Display OLED SSD1306
- Conexão Wi-Fi
- Broker MQTT

## Funcionalidades

- Medição de temperatura, umidade e pressão atmosférica.
- Envio dos dados para um broker MQTT.
- Exibição das medições em um display OLED.
- Sincronização de horário via NTP.

## Conexões

- **BMP180**: 
  - SDA -> D5 (GPIO 14)
  - SCL -> D6 (GPIO 12)
- **DHT-22**: 
  - Data -> D2 (GPIO 4)
- **Display OLED**: 
  - SDA -> D5 (GPIO 14)
  - SCL -> D6 (GPIO 12)

## Bibliotecas Necessárias

- `ESP8266WiFi`
- `NTPClient`
- `WiFiUdp`
- `Adafruit_Sensor`
- `Adafruit_BMP085_U`
- `DHT`
- `DHT_U`
- `WiFiClientSecure`
- `PubSubClient`
- `Adafruit_GFX`
- `Adafruit_SSD1306`

## Configuração

1. **Credenciais Wi-Fi**: Adicione suas redes Wi-Fi no arquivo `wifipasswd.h`.
2. **Configuração MQTT**: Configure o broker MQTT no arquivo `mqttconfig.h`.
