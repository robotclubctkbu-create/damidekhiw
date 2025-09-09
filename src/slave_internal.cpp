#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

extern "C" uint8_t temprature_sens_read(); // ROM function (demo only)

#ifndef MAC_SLAVE
#define MAC_SLAVE "AABBCCDDEEFF"
#endif

typedef struct { uint8_t dataESPNOW[250]; } packet_t;
packet_t pkt;

static float internal_temp_c(){
  float f = (float)temprature_sens_read();
  float c = (f - 32.0f) / 1.8f;
  if (c >= 30.0f) c -= 18.0f;        // your correction
  return roundf(c*100.0f)/100.0f;
}

void onDataRecv(const uint8_t *mac, const uint8_t *incoming, int len) {
  String json((const char*)incoming);
  StaticJsonDocument<256> cmd;
  if (deserializeJson(cmd, json)) return;
  if (strcmp(cmd["message"] | "", "Getdata") != 0) return;

  float t = internal_temp_c();

  StaticJsonDocument<256> out;
  out["mac-slave"] = MAC_SLAVE;
  out["result"]    = t;
  String s; serializeJson(out, s);
  memcpy(pkt.dataESPNOW, s.c_str(), s.length()+1);

  uint8_t uartMac[6];
  String uartHex = cmd["mac-uart"] | "000000000000";
  for (int i=0;i<6;i++){ String b=uartHex.substring(i*2,i*2+2); uartMac[i]=(uint8_t)strtol(b.c_str(), nullptr, 16); }

  esp_now_send(uartMac, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("ESPNOW -> UART payload:%s\n", s.c_str());
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);            // station only
  if (esp_now_init()!=ESP_OK){ Serial.println("ESP-NOW init fail"); ESP.restart(); }
  esp_now_register_recv_cb(onDataRecv);
  Serial.printf("Slave-Internal ready MAC_SLAVE=%s\n", MAC_SLAVE);
}

void loop(){ delay(50); }
