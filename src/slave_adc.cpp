#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

#ifndef ADC_PIN
#define ADC_PIN 32
#endif
#ifndef MAC_SLAVE
#define MAC_SLAVE "112233445566"
#endif

typedef struct { uint8_t dataESPNOW[250]; } packet_t;
packet_t pkt;

static float read_temp_c() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  uint32_t sum=0; for(int i=0;i<16;i++){ sum+=analogRead(ADC_PIN); delay(2); }
  float adc = sum/16.0f;             // 0..4095
  float scaled = (adc/4095.0f)*5.0f; // your scale assumption
  float t = 60.3f - (22.94f * scaled);
  return roundf(t*100.0f)/100.0f;
}

// Arduino-style ESP-NOW callback
void onDataRecv(const uint8_t *mac, const uint8_t *incoming, int len) {
  String json((const char*)incoming);
  StaticJsonDocument<256> cmd;
  if (deserializeJson(cmd, json)) return;
  if (strcmp(cmd["message"] | "", "Getdata") != 0) return;

  float t = read_temp_c();

  StaticJsonDocument<256> out;
  out["mac-slave"] = MAC_SLAVE;
  out["result"]    = t;
  String s; serializeJson(out, s);
  memcpy(pkt.dataESPNOW, s.c_str(), s.length()+1);

  // reply to "mac-uart" (compact hex → 6 bytes)
  uint8_t uartMac[6];
  String uartHex = cmd["mac-uart"] | "000000000000";
  for (int i=0;i<6;i++){ String b=uartHex.substring(i*2,i*2+2); uartMac[i]=(uint8_t)strtol(b.c_str(), nullptr, 16); }

  esp_now_send(uartMac, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("ESPNOW -> UART payload:%s\n", s.c_str());
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);            // station mode only (no AP)
  if (esp_now_init()!=ESP_OK){ Serial.println("ESP-NOW init fail"); ESP.restart(); }
  esp_now_register_recv_cb(onDataRecv);
  Serial.printf("Slave-ADC ready MAC_SLAVE=%s ADC_PIN=%d\n", MAC_SLAVE, ADC_PIN);
}

void loop(){ delay(50); }
