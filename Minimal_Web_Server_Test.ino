// ABSOLUTE MINIMAL WEB SERVER TEST FOR ESP32-S3
// NO TFT, NO DNS, JUST WIFI AP + WEB SERVER

#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

void handleRoot() {
  Serial.println(">>> Request received!");
  server.send(200, "text/plain", "IT WORKS! You reached the ESP32-S3 web server!");
}

void setup() {
  delay(3000); // Wait for USB CDC
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=== MINIMAL WEB SERVER TEST ===\n");

  // Stop WiFi
  WiFi.mode(WIFI_OFF);
  delay(1000);

  // Configure network
  IPAddress local_IP(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Set AP mode
  Serial.println("1. Setting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(2000);

  // Configure network
  Serial.println("2. Configuring network...");
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(1000);

  // Start AP
  Serial.println("3. Starting Access Point...");
  bool ok = WiFi.softAP("TestWeb");
  delay(3000);

  if (ok) {
    Serial.println("   ✓ AP Started!");
    Serial.println("   IP: " + WiFi.softAPIP().toString());
  } else {
    Serial.println("   ✗ AP FAILED!");
    while(1) delay(1000);
  }

  // Re-apply config (ESP32-S3 quirk)
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(2000);

  // Setup web server
  Serial.println("4. Setting up web server...");
  server.on("/", handleRoot);
  server.begin();
  delay(2000);

  Serial.println("\n========================================");
  Serial.println("✓✓✓ WEB SERVER IS READY ✓✓✓");
  Serial.println("========================================");
  Serial.println("Connect to WiFi: TestWeb (no password)");
  Serial.println("Set manual IP: 192.168.1.2");
  Serial.println("Gateway: 192.168.1.1");
  Serial.println("Then browse to: http://192.168.1.1");
  Serial.println("========================================\n");
}

void loop() {
  server.handleClient();

  static unsigned long last = 0;
  if (millis() - last > 3000) {
    int clients = WiFi.softAPgetStationNum();
    Serial.print("Connected devices: ");
    Serial.println(clients);
    last = millis();
  }

  delay(10);
}
