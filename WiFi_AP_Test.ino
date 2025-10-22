// MINIMAL WiFi AP TEST
// This is a simple test to verify WiFi AP functionality
// Upload this first to verify the ESP32 WiFi is working

#include <WiFi.h>

const char* ssid = "ScorePuck-Test";

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("WiFi AP TEST - STARTING");
  Serial.println("=================================");
  Serial.println();

  // Turn off WiFi first
  Serial.println("Step 1: Turning off WiFi...");
  WiFi.mode(WIFI_OFF);
  delay(1000);
  Serial.println("  Done");

  // Set to AP mode
  Serial.println("Step 2: Setting mode to AP...");
  WiFi.mode(WIFI_AP);
  delay(1000);
  Serial.println("  Done");

  // Configure network
  Serial.println("Step 3: Configuring network...");
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  if (WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("  ✓ Network config OK");
  } else {
    Serial.println("  ✗ Network config FAILED");
  }

  // Start AP
  Serial.println("Step 4: Starting Access Point...");
  Serial.println("  SSID: " + String(ssid));
  Serial.println("  Password: NONE (Open)");

  bool result = WiFi.softAP(ssid);

  delay(1000);

  if (result) {
    Serial.println("  ✓ AP Started Successfully!");
  } else {
    Serial.println("  ✗ AP Failed to Start!");
  }

  // Get AP info
  Serial.println();
  Serial.println("=================================");
  Serial.println("ACCESS POINT STATUS:");
  Serial.println("=================================");
  Serial.println("SSID: " + String(ssid));
  Serial.println("IP Address: " + WiFi.softAPIP().toString());
  Serial.println("MAC Address: " + WiFi.softAPmacAddress());
  Serial.println("=================================");
  Serial.println();
  Serial.println("Try connecting from your phone now!");
  Serial.println();
}

void loop() {
  static unsigned long lastCheck = 0;
  static int lastClientCount = -1;

  if (millis() - lastCheck > 2000) {
    int clients = WiFi.softAPgetStationNum();

    if (clients != lastClientCount) {
      Serial.print("Connected devices: ");
      Serial.println(clients);
      lastClientCount = clients;

      if (clients > 0) {
        Serial.println("✓ PHONE CONNECTED SUCCESSFULLY!");
      }
    }

    lastCheck = millis();
  }

  delay(100);
}
