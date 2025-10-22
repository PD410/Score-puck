// ULTRA MINIMAL WiFi AP TEST FOR ESP32-S3
// This removes all complex features

#include <WiFi.h>

void setup() {
  // Wait for USB CDC to be ready (ESP32-S3 specific)
  delay(3000);

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("*** ESP32-S3 WiFi AP Test ***");
  Serial.println();
  Serial.println("Starting in 2 seconds...");
  delay(2000);

  Serial.println("1. Stopping WiFi...");
  WiFi.mode(WIFI_OFF);
  delay(500);

  Serial.println("2. Starting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(500);

  Serial.println("3. Creating Access Point...");
  bool ok = WiFi.softAP("TestAP");

  if (ok) {
    Serial.println("   SUCCESS!");
    Serial.print("   IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("   FAILED!");
  }

  Serial.println();
  Serial.println("Look for WiFi: TestAP");
  Serial.println("=========================");
}

void loop() {
  static unsigned long last = 0;

  if (millis() - last > 3000) {
    int n = WiFi.softAPgetStationNum();
    Serial.print("Clients: ");
    Serial.println(n);
    last = millis();
  }

  delay(100);
}
