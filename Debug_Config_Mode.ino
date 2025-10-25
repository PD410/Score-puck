// DEBUG VERSION - Config mode only, no TFT display
// This should work exactly like Minimal_Web_Server_Test.ino

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

WebServer server(80);
DNSServer dnsServer;

const char* configSSID = "ScorePuck-Setup";
bool configMode = true;

void handleRoot() {
  Serial.println(">>> handleRoot() called!");

  String html = "<!DOCTYPE html><html><body>";
  html += "<h1>ScorePuck Configuration</h1>";
  html += "<p>Web server is working!</p>";
  html += "<p>This is the DEBUG version</p>";
  html += "</body></html>";

  Serial.println("    Sending page...");
  server.send(200, "text/html", html);
  Serial.println("    ✓ Page sent!");
}

void setup() {
  delay(3000); // Wait for USB CDC
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=== DEBUG CONFIG MODE TEST ===\n");

  // Stop WiFi
  Serial.println("1. Stopping WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(2000);

  // Configure network
  Serial.println("2. Configuring network...");
  IPAddress local_IP(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  delay(3000);

  // Apply config BEFORE starting AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(2000);

  // Start AP
  Serial.println("3. Starting Access Point...");
  bool apStarted = WiFi.softAP(configSSID);
  delay(3000);

  if (apStarted) {
    Serial.println("   ✓ AP Started!");
    Serial.println("   IP: " + WiFi.softAPIP().toString());

    // Re-apply config AFTER starting AP (ESP32-S3 quirk)
    delay(1000);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    delay(2000);
  } else {
    Serial.println("   ✗ AP FAILED!");
    while(1) delay(1000);
  }

  // Setup DNS server
  Serial.println("4. Starting DNS server...");
  IPAddress dns_ip = WiFi.softAPIP();
  dnsServer.start(53, "*", dns_ip);
  delay(500);
  Serial.println("   ✓ DNS server started");

  // Setup web server
  Serial.println("5. Setting up web server...");
  server.on("/", handleRoot);
  server.onNotFound(handleRoot);
  server.begin();
  delay(2000);
  Serial.println("   ✓ Web server started");

  Serial.println("\n========================================");
  Serial.println("✓✓✓ WEB SERVER IS READY ✓✓✓");
  Serial.println("========================================");
  Serial.println("Connect to: ScorePuck-Setup");
  Serial.println("Manual IP: 192.168.1.2");
  Serial.println("Gateway: 192.168.1.1");
  Serial.println("Browse to: http://192.168.1.1");
  Serial.println("========================================\n");
}

void loop() {
  // Handle DNS and web requests
  dnsServer.processNextRequest();
  server.handleClient();

  // Status update every 3 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 3000) {
    Serial.print("Status: ");
    Serial.print(WiFi.softAPgetStationNum());
    Serial.println(" device(s) connected");
    lastStatus = millis();
  }

  delay(10);
}
