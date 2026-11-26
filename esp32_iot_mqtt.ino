/*
 * Proyek Mini: Dashboard Node-RED untuk Kontrol Relay dan Monitoring Suhu
 *
 * - Kirim suhu DHT22 tiap 3 detik  -> iot/[NIM]/sensor/suhu
 * - Terima "ON"/"OFF" (relay GPIO 26) <- iot/[NIM]/aktuator/relay
 * - Terima 0-255 (PWM LED GPIO 2)  <- iot/[NIM]/aktuator/pwm
 *
 * Library yang dibutuhkan (Library Manager):
 *   - PubSubClient (Nick O'Leary)
 *   - DHT sensor library (Adafruit)
 *   - Adafruit Unified Sensor
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ===================== KONFIGURASI (UBAH DI SINI) =====================
const char* WIFI_SSID   = "Wokwi-GUEST";   // ganti dengan SSID WiFi Anda (jika perangkat asli)
const char* WIFI_PASS   = "";              // ganti dengan password WiFi Anda
const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* NIM         = "2410010184";

#define DHT_PIN     15      // pin data DHT22
#define DHT_TYPE    DHT22
#define RELAY_PIN   26
#define LED_PIN     2

// Jika modul relay Anda aktif-LOW, ubah menjadi: RELAY_ON = LOW, RELAY_OFF = HIGH
#define RELAY_ON    HIGH
#define RELAY_OFF   LOW

#define PWM_FREQ    5000
#define PWM_RES     8       // 8 bit -> 0-255
#define PUBLISH_INTERVAL 3000  // 3 detik
// =====================================================================

char topicSuhu[64];
char topicRelay[64];
char topicPwm[64];

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastPublish = 0;

// Kompatibel dengan ESP32 Arduino core 2.x dan 3.x
void pwmSetup() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RES);
#else
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcAttachPin(LED_PIN, 0);
#endif
}

void pwmWrite(int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

void setupWiFi() {
  Serial.print("Menyambung ke WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung, IP: " + WiFi.localIP().toString());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.print("Pesan [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (strcmp(topic, topicRelay) == 0) {
    msg.toUpperCase();
    if (msg == "ON") {
      digitalWrite(RELAY_PIN, RELAY_ON);
      Serial.println("Relay -> ON");
    } else if (msg == "OFF") {
      digitalWrite(RELAY_PIN, RELAY_OFF);
      Serial.println("Relay -> OFF");
    }
  } else if (strcmp(topic, topicPwm) == 0) {
    int duty = constrain(msg.toInt(), 0, 255);
    pwmWrite(duty);
    Serial.print("PWM LED -> ");
    Serial.println(duty);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menyambung ke MQTT broker...");
    String clientId = "esp32-" + String(NIM) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("terhubung");
      client.subscribe(topicRelay);
      client.subscribe(topicPwm);
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 3 detik");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  snprintf(topicSuhu,  sizeof(topicSuhu),  "iot/%s/sensor/suhu",     NIM);
  snprintf(topicRelay, sizeof(topicRelay), "iot/%s/aktuator/relay",  NIM);
  snprintf(topicPwm,   sizeof(topicPwm),   "iot/%s/aktuator/pwm",    NIM);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pwmSetup();
  pwmWrite(0);

  dht.begin();
  setupWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    float suhu = dht.readTemperature();
    if (isnan(suhu)) {
      Serial.println("Gagal membaca DHT22!");
      return;
    }

    char payload[16];
    dtostrf(suhu, 1, 1, payload);   // contoh: "27.5"
    client.publish(topicSuhu, payload);

    Serial.print("Suhu dikirim: ");
    Serial.print(payload);
    Serial.println(" C");
  }
}
