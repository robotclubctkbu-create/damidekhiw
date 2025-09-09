#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <driver/uart.h>

// ---------- WiFi + socket settings ----------
#ifndef WIFI_SSID
#define WIFI_SSID "techfusionlab"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "1234567890"
#endif
#ifndef SOCK_HOST
#define SOCK_HOST "192.168.1.50"   // your Python logic server
#endif
#ifndef SOCK_PORT
#define SOCK_PORT 5555
#endif

// ---------- UART settings ----------
#ifndef UART_TX
#define UART_TX 17
#endif
#ifndef UART_RX
#define UART_RX 16
#endif
static constexpr int UART_BUF = 1024;

// ---------- LEDs ----------
#ifndef LED1
#define LED1 4
#endif
#ifndef LED2
#define LED2 5
#endif

WiFiClient sock;
String sockLine;
char mac_id[13];

// ---------- Utility ----------
static String mac_compact() {
  uint8_t m[6]; WiFi.macAddress(m);
  char id[13];
  snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
           m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(id);
}

// ---------- Wi-Fi ----------
void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi connecting");
  int tries=0;
  while (WiFi.status() != WL_CONNECTED && tries < 60) {
    delay(250);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed, rebooting");
    ESP.restart();
  }
  String mc = mac_compact();
  mc.toCharArray(mac_id, sizeof(mac_id));
  Serial.printf("WiFi OK. IP: %s  MAC: %s\n",
                WiFi.localIP().toString().c_str(), mac_id);
  digitalWrite(LED2, HIGH);
}

// ---------- UART ----------
void uart_setup() {
  const uart_config_t cfg = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0
  };
  uart_param_config(UART_NUM_1, &cfg);
  uart_set_pin(UART_NUM_1, UART_TX, UART_RX,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_NUM_1, UART_BUF, 0, 0, NULL, 0);
}

// ---------- Socket ----------
void socket_connect() {
  if (sock.connected()) return;
  Serial.printf("Socket -> %s:%d\n", SOCK_HOST, SOCK_PORT);
  if (!sock.connect(SOCK_HOST, SOCK_PORT)) {
    Serial.println("Socket connect fail");
    return;
  }
  // send hello
  StaticJsonDocument<128> hello;
  hello["type"] = "hello";
  hello["mac_master"] = mac_id;
  String s; serializeJson(hello, s); s += '\n';
  sock.print(s);
}

// ---------- Forward UART -> socket ----------
void forward_uart_to_socket(const String& line) {
  if (!sock.connected()) return;
  StaticJsonDocument<768> out;
  out["type"] = "uart_result";
  out["mac_master"] = mac_id;
  StaticJsonDocument<512> payload;
  if (deserializeJson(payload, line) == DeserializationError::Ok) {
    out["payload"] = payload.as<JsonVariant>();
  } else {
    out["raw"] = line;
  }
  String s; serializeJson(out, s); s += '\n';
  sock.print(s);
  Serial.printf("SOCK OUT: %s\n", s.c_str());
}

void poll_uart() {
  uint8_t buf[UART_BUF];
  int n = uart_read_bytes(UART_NUM_1, buf, sizeof(buf)-1,
                          pdMS_TO_TICKS(50));
  if (n <= 0) return;
  buf[n] = 0;
  String chunk((char*)buf);
  int st=0;
  while (true) {
    int nl = chunk.indexOf('\n', st);
    if (nl < 0) {
      String last = chunk.substring(st);
      if (last.length()) forward_uart_to_socket(last);
      break;
    }
    String line = chunk.substring(st, nl);
    if (line.length()) forward_uart_to_socket(line);
    st = nl+1;
  }
}

// ---------- Handle socket -> UART ----------
void handle_socket_line(const String& line) {
  StaticJsonDocument<1024> cmd;
  if (deserializeJson(cmd, line)) return;
  if (strcmp(cmd["type"] | "", "request_data") == 0) {
    StaticJsonDocument<512> toUart;
    toUart["mode"]     = cmd["mode"]     | "normal";
    toUart["per-page"] = cmd["per_page"] | 1;
    toUart["max-page"] = cmd["max_page"] | 1;
    toUart["mac-slave"] = cmd["mac_slave_list"];
    String s; serializeJson(toUart, s); s += '\n';
    uart_write_bytes(UART_NUM_1, s.c_str(), s.length());
    Serial.printf("UART OUT: %s\n", s.c_str());
  }
}

void poll_socket() {
  if (!sock.connected()) { socket_connect(); return; }
  while (sock.available()) {
    int c = sock.read(); if (c < 0) break;
    if (c == '\n') {
      String l = sockLine; sockLine = "";
      if (l.length()) {
        Serial.printf("SOCK IN: %s\n", l.c_str());
        handle_socket_line(l);
      }
    } else if (c != '\r') {
      sockLine += (char)c;
      if (sockLine.length() > 4096) sockLine = "";
    }
  }
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  wifi_connect();
  uart_setup();
  socket_connect();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED2, LOW);
    wifi_connect();
  }
  poll_socket();
  poll_uart();

  static uint32_t t = millis();
  if (millis() - t > 500) {
    t = millis();
    digitalWrite(LED1, !digitalRead(LED1)); // blink heartbeat
  }
}
