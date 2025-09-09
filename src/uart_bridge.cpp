#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <driver/uart.h>
#include <string.h>

// UART config
#ifndef UART_TX
#define UART_TX 17
#endif
#ifndef UART_RX
#define UART_RX 16
#endif
static constexpr int UART_BUF = 1024;
uint8_t uartBuffer[UART_BUF];

// ESPNOW packet
typedef struct { uint8_t dataESPNOW[250]; } struct_message;
struct_message dataToSendESPNOW;

volatile bool responseReceived = false;
String jsonStringRecv;
char clientId[25];
char macSlaveUart[18];  // hex string

// ---------- ESP-NOW receive callback (Arduino signature) ----------
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  responseReceived = true;
  jsonStringRecv = String((const char*)incomingData);

  // (Optional) log sender
  Serial.print("ESPNOW From: ");
  for (int i=0;i<6;i++){ Serial.printf("%02X", mac[i]); if(i<5) Serial.print(":"); }
  Serial.println();
}

static bool ensure_peer(const uint8_t addr[6]) {
  if (esp_now_is_peer_exist(addr)) return true;
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, addr, 6);
  p.channel = 0;
  p.encrypt = false;
  p.ifidx = WIFI_IF_STA;
  return esp_now_add_peer(&p) == ESP_OK;
}

void setupESPNOW() {
  // ESP-NOW needs station mode, but no AP connection
  WiFi.mode(WIFI_STA);

  // our compact MAC hex for "mac-uart"
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(clientId, sizeof(clientId), "%02X%02X%02X%02X%02X%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.printf("ESP-NOW ready, clientId=%s\n", clientId);
}

void setupUART() {
  const uart_config_t cfg = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0
  };
  uart_param_config(UART_NUM_1, &cfg);
  uart_set_pin(UART_NUM_1, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_NUM_1, UART_BUF, 0, 0, NULL, 0);
}

static void request_one(const String& slaveHex) {
  // Send {"mac-uart":<our mac>,"message":"Getdata"} via ESP-NOW
  StaticJsonDocument<128> cmd;
  cmd["mac-uart"] = clientId;
  cmd["message"]  = "Getdata";
  String cmdStr; serializeJson(cmd, cmdStr);

  struct_message pkt{};
  memcpy(pkt.dataESPNOW, cmdStr.c_str(), cmdStr.length()+1);

  uint8_t peer[6];
  for (int i=0;i<6;i++){ String b=slaveHex.substring(i*2,i*2+2); peer[i]=(uint8_t)strtol(b.c_str(), nullptr, 16); }

  if (!ensure_peer(peer)) Serial.println("esp_now_add_peer failed");

  responseReceived = false;
  jsonStringRecv = "";

  Serial.printf("ESPNOW send -> %s : %s\n", slaveHex.c_str(), cmdStr.c_str());
  esp_now_send(peer, (uint8_t*)&pkt, sizeof(pkt));

  // brief wait for reply (simple demo)
  uint32_t t0 = millis();
  while (!responseReceived && millis()-t0 < 200) delay(10);

  // Build UART JSON line
  StaticJsonDocument<256> out;
  if (responseReceived) {
    if (deserializeJson(out, jsonStringRecv)) {
      out.clear(); out["mac-slave"]=slaveHex; out["result"]="null";
    }
  } else {
    out["mac-slave"]=slaveHex; out["result"]="null";
  }
  String line; serializeJson(out, line); line += '\n';
  uart_write_bytes(UART_NUM_1, line.c_str(), line.length());
  Serial.printf("UART OUT: %s", line.c_str());
}

static void handle_uart_line(const String& line) {
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, line)) { Serial.println("Bad UART JSON"); return; }
  JsonArray arr = doc["mac-slave"].as<JsonArray>();
  for (JsonVariant v : arr) {
    String slaveHex = v.as<String>(); // 12 hex chars, no colons
    strncpy(macSlaveUart, slaveHex.c_str(), sizeof(macSlaveUart)-1);
    macSlaveUart[sizeof(macSlaveUart)-1] = '\0';
    request_one(slaveHex);
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);
  setupUART();
  setupESPNOW();
  Serial.println("UART-Bridge ready");
}

void loop() {
  int n = uart_read_bytes(UART_NUM_1, uartBuffer, UART_BUF-1, pdMS_TO_TICKS(50));
  if (n > 0) {
    uartBuffer[n] = 0;
    String chunk((char*)uartBuffer);
    int st=0;
    while (true) {
      int nl = chunk.indexOf('\n', st);
      if (nl < 0) { String last = chunk.substring(st); if (last.length()) handle_uart_line(last); break; }
      String line = chunk.substring(st, nl); if (line.length()) handle_uart_line(line);
      st = nl + 1;
    }
  }
}
