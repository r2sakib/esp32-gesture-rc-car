#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <esp_wifi.h>

// --- PINS ---
#define I2C_SDA 33
#define I2C_SCL 32
const int LED_PIN = 2;
const int LOCK_PIN = 25;

// --- CONFIG ---
const int speedLimit = 200;
#define WIFI_CHANNEL 1 // Must match Car AP Channel

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
Adafruit_MPU6050 mpu;

typedef struct struct_message {
  int throttle;
  int steering;
  bool armed;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

int lastLockState = HIGH;

void OnDataSent(const uint8_t * mac_addr, esp_now_send_status_t status) {
  // Optional: Add LED blink on success here if desired
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LOCK_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mpu.begin()) {
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);
    }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --- WI-FI SETUP ---
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Force Channel to match Receiver
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb((esp_now_send_cb_t) OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;

  digitalWrite(LED_PIN, HIGH);
  lastLockState = digitalRead(LOCK_PIN);
}

void loop() {
  int currentLockState = digitalRead(LOCK_PIN);

  // Visual toggle confirmation
  if (currentLockState != lastLockState) {
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(50);
    }
    lastLockState = currentLockState;
  }

  if (currentLockState == LOW) {
    // UNLOCKED
    digitalWrite(LED_PIN, HIGH);
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    int thr = (a.acceleration.x / 9.8) * 450;
    int str = (a.acceleration.y / 9.8) * -155;

    if (thr > speedLimit) thr = speedLimit;
    else if (thr < -speedLimit) thr = -speedLimit;
    if (thr > 255) thr = 255;
    else if (thr < -255) thr = -255;
    if (str > 255) str = 255;
    else if (str < -255) str = -255;
    if (abs(thr) < 50) thr = 0;
    if (abs(str) < 50) str = 0;

    myData.throttle = thr;
    myData.steering = str;
    myData.armed = true;

  } else {
    // LOCKED
    digitalWrite(LED_PIN, LOW);
    myData.throttle = 0;
    myData.steering = 0;
    myData.armed = false;
  }

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  delay(50);
}