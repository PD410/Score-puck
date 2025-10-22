#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <time.h>
#include "mets_logo.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>

// Configuration storage
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

// Configuration variables (loaded from storage)
String configuredSSID = "";
String configuredPassword = "";
int configuredTeamId = 121;  // Default: Mets
String configuredTeamAbbr = "NYM";
String configuredTeamName = "Mets";
uint16_t configuredPrimaryColor = 0x001F;   // Default: Mets Blue
uint16_t configuredSecondaryColor = 0xFD20;  // Default: Mets Orange

// Configuration mode
bool configMode = false;
const char* configSSID = "ScorePuck-Setup";
const char* configPassword = "scorepuck123";

// Pin assignments
#define TFT_CS   4
#define TFT_DC   5  
#define TFT_RST  6
#define TFT_MOSI 7
#define TFT_SCK  8

// Initialize display
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// Colors
#define METS_BLUE    0x001F   
#define METS_ORANGE  0xFD20    
#define WHITE        0xFFFF
#define BLACK        0x0000
#define GRAY         0x7BEF
#define GREEN        0x07E0
#define RED          0xF800
#define YELLOW       0xFFE0

// Off-season dates for 2026 - CHANGE THESE WHEN MLB ANNOUNCES OFFICIAL DATES
#define SPRING_TRAINING_2026_YEAR  2026
#define SPRING_TRAINING_2026_MONTH 2     // February
#define SPRING_TRAINING_2026_DAY   18    // Pitchers & Catchers typically mid-February

#define OPENING_DAY_2026_YEAR  2026
#define OPENING_DAY_2026_MONTH 3         // March  
#define OPENING_DAY_2026_DAY   26        // Opening Day typically late March

// TEST MODE
const bool TEST_MODE = false;
const int DEBUG_SCENARIO = 2;

// FORCE CONFIG MODE - Set to true to force configuration portal on boot
const bool FORCE_CONFIG_MODE = false;

// Game data structure
struct GameInfo {
  bool hasGame;
  String opponent;
  int metsScore;
  int oppScore;
  String status;
  String detailedState;
  String inning;
  bool isLive;
  bool isFinal;
  bool isScheduled;
  bool isPostponed;
  bool isDelayed;
  String gameTime;
  String gameDate;
  String venue;
  bool metsHome;
  String awayTeam;
  String homeTeam;
  int gameId;
  time_t postponedTime;
};

// DMA buffers
uint16_t* blueGradientBuffer = nullptr;
uint16_t* greenGradientBuffer = nullptr;
const int SCREEN_BUFFER_SIZE = 240 * 240;
bool dmaBuffersInitialized = false;

// Function declarations
void initializeDMABuffers();
void drawBlueGradientBackgroundPremium();
void drawGreenGradientBackground();
uint16_t getBlueGradientColorAtXY(int x, int y);
void clearScreenWithGradient();
void showStartupScreen();
void connectToWiFi();
void showOfflineScreen();
void displayGame(GameInfo game);
String getCurrentDate();
GameInfo getGameData(String date);
GameInfo getNextGame();
GameInfo getPreviousGame();
String getFutureDate(int daysAhead);
String getPastDate(int daysBack);
String formatGameTime(String isoDateTime);
String getTeamAbbreviation(String fullName);
uint16_t interpolateColor(uint16_t color1, uint16_t color2, float factor);
GameInfo getTestGameData();
void drawMetsLogoScaled(int x, int y, uint16_t logoColor, float scale);
bool shouldShowGameTime(GameInfo game);
int getDaysUntilDate(int targetYear, int targetMonth, int targetDay);
void loadConfiguration();
void saveConfiguration();
void clearConfiguration();
void startConfigPortal();
void handleRoot();
void handleSave();
void handleStatus();
void setupOTA();
void checkConfigButton();

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("=== MLB SCOREPUCK v4.0 - MULTI-TEAM CONFIGURABLE ===");
  Serial.println();

  // Load saved configuration
  loadConfiguration();
  Serial.println();

  // Check if we should force config mode
  if (FORCE_CONFIG_MODE) {
    Serial.println("FORCE_CONFIG_MODE is enabled - entering config mode");
    configMode = true;
  }
  
  if (TEST_MODE) {
    Serial.println("*** DEBUG MODE ENABLED ***");
    Serial.print("Current DEBUG_SCENARIO: ");
    switch(DEBUG_SCENARIO) {
      case 0: Serial.println("0 - Live Game Test"); break;
      case 1: Serial.println("1 - Postponed Game Test"); break;
      case 2: Serial.println("2 - Delayed Game Test"); break;
      case 3: Serial.println("3 - Scheduled Game Test"); break;
      default: Serial.println("Unknown scenario"); break;
    }
  }
  
  // Initialize display pins
  Serial.println("Setting up display...");
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_MOSI, OUTPUT);
  pinMode(TFT_SCK, OUTPUT);
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_RST, HIGH);
  
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(200);
  
  tft.begin();
  Serial.println("✓ Display initialized");

  initializeDMABuffers();
  showStartupScreen();

  // Check if we need to enter config mode (no WiFi credentials saved)
  Serial.println("Checking configuration status...");
  if (configuredSSID == "" || configuredSSID.length() == 0) {
    Serial.println("No WiFi credentials found - entering config mode");
    configMode = true;
  }

  if (configMode || FORCE_CONFIG_MODE) {
    Serial.println("==============================================");
    Serial.println("ENTERING CONFIGURATION MODE");
    Serial.println("==============================================");
    startConfigPortal();
  } else {
    Serial.println("WiFi credentials found: " + configuredSSID);
    Serial.println("Attempting to connect...");
    connectToWiFi();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Setup OTA updates
    setupOTA();

    // Setup web server for configuration
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/status", handleStatus);
    server.begin();
    Serial.println("Web server started on http://" + WiFi.localIP().toString());

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();
    Serial.println("Time configured for Pacific Time Zone");

    delay(2000);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.printf("Current local time: %04d-%02d-%02d %02d:%02d:%02d\n",
                   timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
  }
}

// NEW FUNCTION: Calculate days until a target date
int getDaysUntilDate(int targetYear, int targetMonth, int targetDay) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return -1;
  }
  
  time_t now = mktime(&timeinfo);
  
  struct tm targetTime = {0};
  targetTime.tm_year = targetYear - 1900;
  targetTime.tm_mon = targetMonth - 1;
  targetTime.tm_mday = targetDay;
  targetTime.tm_hour = 0;
  targetTime.tm_min = 0;
  targetTime.tm_sec = 0;
  
  time_t target = mktime(&targetTime);
  
  double diffSeconds = difftime(target, now);
  int diffDays = (int)(diffSeconds / (60 * 60 * 24));
  
  return diffDays;
}

GameInfo getTestGameData() {
  GameInfo testGame;
  testGame.hasGame = true;
  testGame.gameDate = getCurrentDate();
  testGame.venue = "Citi Field";
  testGame.gameId = 123456;
  testGame.postponedTime = 0;
  testGame.metsHome = true;
  testGame.homeTeam = "NYM";
  testGame.awayTeam = "BAL";
  testGame.opponent = "BAL";
  testGame.gameTime = "";
  
  switch(DEBUG_SCENARIO) {
    case 0:
      testGame.isLive = true;
      testGame.isFinal = false;
      testGame.isScheduled = false;
      testGame.isPostponed = false;
      testGame.isDelayed = false;
      testGame.status = "Live";
      testGame.detailedState = "In Progress";
      testGame.metsScore = 7;
      testGame.oppScore = 6;
      testGame.inning = "B6";
      break;
      
    case 1:
      testGame.isLive = false;
      testGame.isFinal = false;
      testGame.isScheduled = false;
      testGame.isPostponed = true;
      testGame.isDelayed = false;
      testGame.status = "Postponed";
      testGame.detailedState = "Postponed";
      testGame.metsScore = 0;
      testGame.oppScore = 0;
      testGame.inning = "LAST: 3-6 vs WSN";
      testGame.postponedTime = time(nullptr);
      break;
      
    case 2:
      testGame.isLive = false;
      testGame.isFinal = false;
      testGame.isScheduled = false;
      testGame.isPostponed = false;
      testGame.isDelayed = true;
      testGame.status = "Delayed";
      testGame.detailedState = "Delayed Start";
      testGame.metsScore = 0;
      testGame.oppScore = 0;
      testGame.inning = "LAST: 8-4 vs PHI";
      break;
      
    case 3:
      testGame.isLive = false;
      testGame.isFinal = false;
      testGame.isScheduled = true;
      testGame.isPostponed = false;
      testGame.isDelayed = false;
      testGame.status = "Scheduled";
      testGame.detailedState = "Scheduled";
      testGame.metsScore = 0;
      testGame.oppScore = 0;
      testGame.inning = "LAST: 5-2 vs ATL";
      testGame.gameTime = "7:10 PM";
      break;
      
    default:
      testGame.isLive = true;
      testGame.isFinal = false;
      testGame.isScheduled = false;
      testGame.isPostponed = false;
      testGame.isDelayed = false;
      testGame.status = "Live";
      testGame.detailedState = "In Progress";
      testGame.metsScore = 7;
      testGame.oppScore = 6;
      testGame.inning = "B6";
      break;
  }
  
  return testGame;
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();

  // Handle web server requests
  server.handleClient();

  if (TEST_MODE) {
    Serial.println("=== TEST MODE ACTIVE ===");
    GameInfo testGame = getTestGameData();
    displayGame(testGame);
    delay(10000);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, showing offline screen");
    showOfflineScreen();
    delay(5000);
    return;
  }

  String currentDate = getCurrentDate();
  Serial.println("Current date: " + currentDate);

  Serial.println("Fetching today's game data...");
  GameInfo game = getGameData(currentDate);

  if (!game.hasGame) {
    Serial.println("No game today, looking for next game...");
    game = getNextGame();

    if (game.hasGame) {
      Serial.println("Found next game on: " + game.gameDate);

      GameInfo prevGame = getPreviousGame();
      if (prevGame.hasGame && prevGame.isFinal) {
        game.inning = "LAST: " + String(prevGame.metsScore) + "-" + String(prevGame.oppScore) + " vs " + prevGame.opponent;
      }
    } else {
      Serial.println("No upcoming games found - showing off-season countdown");
    }
  }

  displayGame(game);

  Serial.println("Waiting 60 seconds for next update...");

  // Instead of blocking delay, check OTA/server in smaller intervals
  for (int i = 0; i < 60; i++) {
    ArduinoOTA.handle();
    server.handleClient();
    delay(1000);
  }
}

bool shouldShowGameTime(GameInfo game) {
  if (!game.isPostponed) {
    return true;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }
  
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min < 5) {
    char currentDateStr[11];
    strftime(currentDateStr, sizeof(currentDateStr), "%Y-%m-%d", &timeinfo);
    
    if (game.gameDate != String(currentDateStr)) {
      return true;
    }
  }
  
  return false;
}

void initializeDMABuffers() {
  Serial.println("Initializing DMA buffers...");
  
  blueGradientBuffer = (uint16_t*)malloc(SCREEN_BUFFER_SIZE * sizeof(uint16_t));
  greenGradientBuffer = (uint16_t*)malloc(SCREEN_BUFFER_SIZE * sizeof(uint16_t));
  
  if (!blueGradientBuffer || !greenGradientBuffer) {
    Serial.println("✗ Failed to allocate DMA buffers");
    if (blueGradientBuffer) free(blueGradientBuffer);
    if (greenGradientBuffer) free(greenGradientBuffer);
    blueGradientBuffer = nullptr;
    greenGradientBuffer = nullptr;
    return;
  }
  
  Serial.println("Pre-rendering blue gradient...");
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      float ratio = (float)(x + y) / 478.0;
      ratio = constrain(ratio, 0.0, 1.0);
      ratio = ratio * ratio * (3.0 - 2.0 * ratio);
      
      uint8_t r = 0 + (uint8_t)(8 * ratio);
      uint8_t g = 44 + (uint8_t)(37 * ratio);
      uint8_t b = 113 + (uint8_t)(79 * ratio);
      
      blueGradientBuffer[y * 240 + x] = tft.color565(r, g, b);
    }
  }
  
  Serial.println("Pre-rendering green gradient...");
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      float ratio = (float)(x + y) / 478.0;
      ratio = constrain(ratio, 0.0, 1.0);
      ratio = ratio * ratio * (3.0 - 2.0 * ratio);
      
      uint8_t r = 4 + (uint8_t)((0 - 4) * ratio);
      uint8_t g = 106 + (uint8_t)((209 - 106) * ratio);
      uint8_t b = 0;
      
      greenGradientBuffer[y * 240 + x] = tft.color565(r, g, b);
    }
  }
  
  dmaBuffersInitialized = true;
  Serial.println("✓ DMA buffers initialized!");
}

void drawBlueGradientBackgroundPremium() {
  if (dmaBuffersInitialized && blueGradientBuffer) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, 240, 240);
    tft.writePixels(blueGradientBuffer, SCREEN_BUFFER_SIZE);
    tft.endWrite();
    return;
  }
  
  tft.fillScreen(BLACK);
  
  for (int y = 0; y < 240; y += 2) {
    for (int x = 0; x < 240; x += 2) {
      float ratio = (float)(x + y) / 478.0;
      ratio = constrain(ratio, 0.0, 1.0);
      ratio = ratio * ratio * (3.0 - 2.0 * ratio);
      
      uint8_t r = 0 + (uint8_t)(8 * ratio);
      uint8_t g = 44 + (uint8_t)(37 * ratio);
      uint8_t b = 113 + (uint8_t)(79 * ratio);
      
      uint16_t color = tft.color565(r, g, b);
      
      tft.drawPixel(x, y, color);
      tft.drawPixel(x+1, y, color);
      tft.drawPixel(x, y+1, color);
      tft.drawPixel(x+1, y+1, color);
    }
  }
}

void drawGreenGradientBackground() {
  if (dmaBuffersInitialized && greenGradientBuffer) {
    tft.startWrite();
    tft.setAddrWindow(0, 0, 240, 240);
    tft.writePixels(greenGradientBuffer, SCREEN_BUFFER_SIZE);
    tft.endWrite();
    return;
  }
  
  tft.fillScreen(BLACK);
  
  for (int y = 0; y < 240; y += 2) {
    for (int x = 0; x < 240; x += 2) {
      float ratio = (float)(x + y) / 478.0;
      ratio = constrain(ratio, 0.0, 1.0);
      ratio = ratio * ratio * (3.0 - 2.0 * ratio);
      
      uint8_t r = 4 + (uint8_t)((0 - 4) * ratio);
      uint8_t g = 106 + (uint8_t)((209 - 106) * ratio);
      uint8_t b = 0;
      
      uint16_t color = tft.color565(r, g, b);
      
      tft.drawPixel(x, y, color);
      tft.drawPixel(x+1, y, color);
      tft.drawPixel(x, y+1, color);
      tft.drawPixel(x+1, y+1, color);
    }
  }
}

uint16_t getBlueGradientColorAtXY(int x, int y) {
  if (dmaBuffersInitialized && blueGradientBuffer) {
    if (x >= 0 && x < 240 && y >= 0 && y < 240) {
      return blueGradientBuffer[y * 240 + x];
    }
  }
  
  float ratio = (float)(x + y) / 478.0;
  ratio = constrain(ratio, 0.0, 1.0);
  ratio = ratio * ratio * (3.0 - 2.0 * ratio);
  
  uint8_t r = 0 + (uint8_t)(8 * ratio);
  uint8_t g = 44 + (uint8_t)(37 * ratio);
  uint8_t b = 113 + (uint8_t)(79 * ratio);
  
  return tft.color565(r, g, b);
}

void clearScreenWithGradient() {
  drawBlueGradientBackgroundPremium();
}

void showStartupScreen() {
  Serial.println("Showing Mets Scorepuck loading screen...");
  
  drawBlueGradientBackgroundPremium();
  
  int logoX = 120 - (METS_LOGO_WIDTH / 2);   
  int logoY = 120 - (METS_LOGO_HEIGHT / 2) - 25;  
  
  drawMetsLogoWithGradientBackground(logoX, logoY, METS_ORANGE);
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(WHITE);
  
  String text = "SCOREPUCK";
  
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  int textY = 120 + 70;
  
  tft.setCursor(120 - w/2, textY);
  tft.print(text);
  
  tft.setFont();
  
  delay(3000);
}

String getCurrentDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "2025-07-15";
  }
  
  char dateString[11];
  strftime(dateString, sizeof(dateString), "%Y-%m-%d", &timeinfo);
  return String(dateString);
}

GameInfo getGameData(String date) {
  GameInfo game;
  game.hasGame = false;
  game.metsScore = 0;
  game.oppScore = 0;
  game.opponent = "";
  game.status = "No Data";
  game.detailedState = "";
  game.inning = "";
  game.isLive = false;
  game.isFinal = false;
  game.isScheduled = false;
  game.isPostponed = false;
  game.isDelayed = false;
  game.gameTime = "";
  game.gameDate = date;
  game.venue = "";
  game.metsHome = false;
  game.awayTeam = "";
  game.homeTeam = "";
  game.gameId = 0;
  game.postponedTime = 0;
  
  if (WiFi.status() != WL_CONNECTED) {
    return game;
  }
  
  WiFiClient client;
  if (!client.connect("www.google.com", 80)) {
    Serial.println("Network issue");
    return game;
  }
  client.stop();
  
  HTTPClient http;
  String url = "http://statsapi.mlb.com/api/v1/schedule/games/?sportId=1&teamId=" + String(configuredTeamId) + "&date=" + date;
  
  http.begin(url);
  http.setTimeout(20000);
  http.addHeader("User-Agent", "ESP32-Scoreboard/1.0");
  http.addHeader("Accept", "application/json");
  http.addHeader("Connection", "close");
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc.containsKey("dates") && doc["dates"].size() > 0) {
      JsonArray dates = doc["dates"];
      
      for (JsonObject dateObj : dates) {
        if (dateObj.containsKey("games") && dateObj["games"].size() > 0) {
          JsonArray games = dateObj["games"];
          
          for (JsonObject gameData : games) {
            if (gameData.containsKey("teams")) {
              JsonObject teamsObj = gameData["teams"];
              bool foundMets = false;
              
              if (teamsObj.containsKey("home") && teamsObj["home"].containsKey("team")) {
                JsonObject homeTeamObj = teamsObj["home"]["team"];
                
                if (homeTeamObj.containsKey("abbreviation")) {
                  game.homeTeam = homeTeamObj["abbreviation"].as<String>();
                } else if (homeTeamObj.containsKey("name")) {
                  game.homeTeam = getTeamAbbreviation(homeTeamObj["name"].as<String>());
                }
                
                if (homeTeamObj.containsKey("id") && homeTeamObj["id"].as<int>() == configuredTeamId) {
                  foundMets = true;
                  game.metsHome = true;
                }
              }
              
              if (teamsObj.containsKey("away") && teamsObj["away"].containsKey("team")) {
                JsonObject awayTeamObj = teamsObj["away"]["team"];
                
                if (awayTeamObj.containsKey("abbreviation")) {
                  game.awayTeam = awayTeamObj["abbreviation"].as<String>();
                } else if (awayTeamObj.containsKey("name")) {
                  game.awayTeam = getTeamAbbreviation(awayTeamObj["name"].as<String>());
                }
                
                if (awayTeamObj.containsKey("id") && awayTeamObj["id"].as<int>() == configuredTeamId) {
                  foundMets = true;
                  game.metsHome = false;
                }
              }
              
              if (foundMets) {
                game.opponent = game.metsHome ? game.awayTeam : game.homeTeam;
                game.hasGame = true;
                
                if (teamsObj.containsKey("home") && teamsObj["home"].containsKey("score") &&
                    teamsObj.containsKey("away") && teamsObj["away"].containsKey("score")) {
                  int homeScore = teamsObj["home"]["score"].as<int>();
                  int awayScore = teamsObj["away"]["score"].as<int>();
                  
                  if (game.metsHome) {
                    game.metsScore = homeScore;
                    game.oppScore = awayScore;
                  } else {
                    game.metsScore = awayScore;
                    game.oppScore = homeScore;
                  }
                }
              }
              
              if (!foundMets) continue;
            }
            
            if (gameData.containsKey("status")) {
              JsonObject status = gameData["status"];
              if (status.containsKey("abstractGameState")) {
                game.status = status["abstractGameState"].as<String>();
              }
              if (status.containsKey("detailedState")) {
                game.detailedState = status["detailedState"].as<String>();
              }
            }
            
            if (game.detailedState.indexOf("Postponed") >= 0 || game.status.indexOf("Postponed") >= 0) {
              game.isPostponed = true;
              game.postponedTime = time(nullptr);
            }
            
            if (game.detailedState.indexOf("Delayed") >= 0 || game.status.indexOf("Delayed") >= 0) {
              game.isDelayed = true;
            }
            
            if (gameData.containsKey("gamePk")) {
              game.gameId = gameData["gamePk"].as<int>();
            }
            
            if (gameData.containsKey("linescore")) {
              JsonObject linescore = gameData["linescore"];
              if (linescore.containsKey("currentInning") && linescore.containsKey("inningHalf")) {
                int inningNum = linescore["currentInning"].as<int>();
                String inningHalf = linescore["inningHalf"].as<String>();
                
                if (inningHalf == "Top") {
                  game.inning = "T" + String(inningNum);
                } else if (inningHalf == "Bottom") {
                  game.inning = "B" + String(inningNum);
                } else {
                  game.inning = String(inningNum);
                }
              }
            }
            
            game.isLive = (game.status == "Live");
            game.isFinal = (game.status == "Final");
            game.isScheduled = (game.status == "Preview" || game.status == "Scheduled");
            
            if (game.isScheduled && gameData.containsKey("gameDate")) {
              String gameDateTime = gameData["gameDate"].as<String>();
              game.gameTime = formatGameTime(gameDateTime);
            }
            
            if (gameData.containsKey("venue") && gameData["venue"].containsKey("name")) {
              game.venue = gameData["venue"]["name"].as<String>();
            }
            
            http.end();
            return game;
          }
        }
      }
    }
  }
  
  http.end();
  return game;
}

GameInfo getNextGame() {
  for (int i = 1; i <= 21; i++) {
    String futureDate = getFutureDate(i);
    GameInfo game = getGameData(futureDate);
    if (game.hasGame) {
      game.gameDate = futureDate;
      return game;
    }
    delay(200);
  }
  
  GameInfo noGame;
  noGame.hasGame = false;
  return noGame;
}

GameInfo getPreviousGame() {
  for (int i = 1; i <= 21; i++) {
    String pastDate = getPastDate(i);
    GameInfo game = getGameData(pastDate);
    if (game.hasGame && game.isFinal) {
      game.gameDate = pastDate;
      return game;
    }
    delay(200);
  }
  
  GameInfo noGame;
  noGame.hasGame = false;
  return noGame;
}

String getFutureDate(int daysAhead) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    time_t now = time(nullptr);
    now += (daysAhead * 24 * 60 * 60);
    struct tm* futureTime = localtime(&now);
    
    char dateString[11];
    strftime(dateString, sizeof(dateString), "%Y-%m-%d", futureTime);
    return String(dateString);
  }
  
  time_t now = mktime(&timeinfo);
  now += (daysAhead * 24 * 60 * 60);
  struct tm* futureTime = localtime(&now);
  
  char dateString[11];
  strftime(dateString, sizeof(dateString), "%Y-%m-%d", futureTime);
  return String(dateString);
}

String getPastDate(int daysBack) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    time_t now = time(nullptr);
    now -= (daysBack * 24 * 60 * 60);
    struct tm* pastTime = localtime(&now);
    
    char dateString[11];
    strftime(dateString, sizeof(dateString), "%Y-%m-%d", pastTime);
    return String(dateString);
  }
  
  time_t now = mktime(&timeinfo);
  now -= (daysBack * 24 * 60 * 60);
  struct tm* pastTime = localtime(&now);
  
  char dateString[11];
  strftime(dateString, sizeof(dateString), "%Y-%m-%d", pastTime);
  return String(dateString);
}

String formatGameTime(String isoDateTime) {
  if (isoDateTime.length() < 16) {
    return "TBD";
  }
  
  int year = isoDateTime.substring(0, 4).toInt();
  int month = isoDateTime.substring(5, 7).toInt();
  int day = isoDateTime.substring(8, 10).toInt();
  int hour = isoDateTime.substring(11, 13).toInt();
  int minute = isoDateTime.substring(14, 16).toInt();
  
  // Convert UTC to Pacific Time (UTC-7 for PDT, UTC-8 for PST)
  int localHour = hour - 7;
  int localDay = day;
  int localMonth = month;
  int localYear = year;
  
  if (localHour < 0) {
    localHour += 24;
    localDay--;
    if (localDay < 1) {
      localMonth--;
      if (localMonth < 1) {
        localMonth = 12;
        localYear--;
      }
      int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      if (localYear % 4 == 0 && (localYear % 100 != 0 || localYear % 400 == 0)) {
        daysInMonth[1] = 29;
      }
      localDay = daysInMonth[localMonth - 1];
    }
  } else if (localHour >= 24) {
    localHour -= 24;
    localDay++;
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (localYear % 4 == 0 && (localYear % 100 != 0 || localYear % 400 == 0)) {
      daysInMonth[1] = 29;
    }
    if (localDay > daysInMonth[localMonth - 1]) {
      localDay = 1;
      localMonth++;
      if (localMonth > 12) {
        localMonth = 1;
        localYear++;
      }
    }
  }
  
  int displayHour = localHour;
  String ampm = "AM";
  
  if (displayHour >= 12) {
    ampm = "PM";
    if (displayHour > 12) displayHour -= 12;
  }
  if (displayHour == 0) displayHour = 12;
  
  String timeStr = String(displayHour) + ":";
  if (minute < 10) timeStr += "0";
  timeStr += String(minute) + " " + ampm;
  
  String gameDate = String(localYear) + "-";
  if (localMonth < 10) gameDate += "0";
  gameDate += String(localMonth) + "-";
  if (localDay < 10) gameDate += "0";
  gameDate += String(localDay);
  
  String today = getCurrentDate();
  if (gameDate != today) {
    const char* months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    timeStr = String(months[localMonth]) + " " + String(localDay) + " " + timeStr;
  }
  
  return timeStr;
}

void displayGame(GameInfo game) {
  Serial.println("Updating display...");
  
  clearScreenWithGradient();
  
  // NEW OFF-SEASON COUNTDOWN DISPLAY
  if (!game.hasGame) {
    // Draw Mets logo at top - smaller scale
    int logoX = 120 - (METS_LOGO_WIDTH * 0.4 / 2);   
    int logoY = 20;  
    drawMetsLogoScaled(logoX, logoY, METS_ORANGE, 0.4);
    
    // Get days until spring training and opening day
    int daysToSpringTraining = getDaysUntilDate(SPRING_TRAINING_2026_YEAR, 
                                                  SPRING_TRAINING_2026_MONTH, 
                                                  SPRING_TRAINING_2026_DAY);
    int daysToOpeningDay = getDaysUntilDate(OPENING_DAY_2026_YEAR, 
                                             OPENING_DAY_2026_MONTH, 
                                             OPENING_DAY_2026_DAY);
    
    // Title
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(METS_ORANGE);
    
    String titleText = "OFF-SEASON";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(titleText, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 95);
    tft.print(titleText);
    
    // Dividing line
    tft.drawLine(30, 106, 210, 106, WHITE);
    tft.drawLine(30, 107, 210, 107, WHITE);
    
    // Spring Training Countdown
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    
    String stLabel = "SPRING TRAINING";
    int stLabelWidth = stLabel.length() * 6;
    tft.setCursor(125 - stLabelWidth/2, 119);
    tft.print(stLabel);
    
    // Days until spring training - large number
    if (daysToSpringTraining >= 0) {
      tft.setFont(&FreeSansBold18pt7b);
      tft.setTextColor(METS_ORANGE);
      
      String daysSTStr = String(daysToSpringTraining) + " DAYS";
      tft.getTextBounds(daysSTStr, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(120 - w/2, 155);
      tft.print(daysSTStr);
      
      // "DAYS" label
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(WHITE);
      tft.setCursor(110, 165);
    } else {
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(GRAY);
      tft.setCursor(90, 145);
      tft.print("Date Passed");
    }
    
    // Dividing line
    tft.drawLine(30, 175, 210, 175, GRAY);
    
    // Opening Day Countdown
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    
    String odLabel = "OPENING DAY";
    int odLabelWidth = odLabel.length() * 6;
    tft.setCursor(120 - odLabelWidth/2, 188);
    tft.print(odLabel);
    
    // Days until opening day
    if (daysToOpeningDay >= 0) {
      tft.setFont(&FreeSansBold12pt7b);
      tft.setTextColor(WHITE);
      
      String daysODStr = String(daysToOpeningDay) + " DAYS";
      tft.getTextBounds(daysODStr, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(120 - w/2, 215);
      tft.print(daysODStr);
    } else {
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(GRAY);
      tft.setCursor(80, 205);
      tft.print("Season Started!");
    }
    
    Serial.println("✓ Off-season countdown displayed");
    Serial.printf("   Days to Spring Training: %d\n", daysToSpringTraining);
    Serial.printf("   Days to Opening Day: %d\n", daysToOpeningDay);
    return;
  }
  
  // Handle postponed games
  if (game.isPostponed && !shouldShowGameTime(game)) {
    tft.setFont(&FreeSansBold12pt7b);
    if (game.metsHome) {
      tft.setTextColor(WHITE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print(game.opponent);
      
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setTextColor(METS_ORANGE);
      tft.setCursor(startX + w1 + w2, 50);
      tft.print("NYM");
    } else {
      tft.setTextColor(METS_ORANGE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print("NYM");
      
      tft.setTextColor(WHITE);
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setCursor(startX + w1 + w2, 50);
      tft.print(game.opponent);
    }
    
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(WHITE);
    
    String postponedText = "POSTPONED";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(postponedText, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 100);
    tft.println(postponedText);
    
    tft.drawLine(40, 114, 200, 114, WHITE);
    tft.drawLine(40, 115, 200, 115, WHITE);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    String prevLabel = "PREVIOUS GAME";
    int prevLabelWidth = prevLabel.length() * 6;
    tft.setCursor(120 - prevLabelWidth/2, 125);
    tft.println(prevLabel);
    
    if (game.inning.length() > 0 && game.inning.startsWith("LAST:")) {
      String lastGameInfo = game.inning.substring(6);
      
      int vsIndex = lastGameInfo.indexOf(" vs ");
      if (vsIndex > 0) {
        String scoreInfo = lastGameInfo.substring(0, vsIndex);
        String prevOpponent = lastGameInfo.substring(vsIndex + 4);
        
        int dashIndex = scoreInfo.indexOf("-");
        if (dashIndex > 0) {
          int prevMetsScore = scoreInfo.substring(0, dashIndex).toInt();
          int prevOppScore = scoreInfo.substring(dashIndex + 1).toInt();
          
          tft.setFont(&FreeSansBold12pt7b);
          tft.setTextColor(METS_ORANGE);
          
          int16_t x1, y1;
          uint16_t w1, h1, w2, h2;
          tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(prevOpponent, 0, 0, &x1, &y1, &w2, &h2);
          
          int spacing = 50;
          int nymX = 120 - spacing/2 - w1;
          int oppX = 120 + spacing/2;
          
          tft.setCursor(nymX, 155);
          tft.print("NYM");
          
          tft.setTextColor(WHITE);
          tft.setCursor(oppX, 155);
          tft.print(prevOpponent);
          
          tft.setTextColor(WHITE);
          String metsScoreStr = String(prevMetsScore);
          String oppScoreStr = String(prevOppScore);
          
          tft.getTextBounds(metsScoreStr, 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(oppScoreStr, 0, 0, &x1, &y1, &w2, &h2);
          
          int scoreNymX = nymX + (30 - w1) / 2;
          int scoreOppX = oppX + (prevOpponent.length() * 8 - w2) / 2;
          
          tft.setCursor(scoreNymX, 180);
          tft.print(prevMetsScore);
          
          tft.setCursor(scoreOppX, 180);
          tft.print(prevOppScore);
          
          tft.setFont(&FreeSansBold18pt7b);
          if (prevMetsScore > prevOppScore) {
            tft.setTextColor(WHITE);
            tft.getTextBounds("W", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("W");
          } else {
            tft.setTextColor(WHITE);
            tft.getTextBounds("L", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("L");
          }
        }
      }
    } else {
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(GRAY);
      tft.setCursor(85, 155);
      tft.println("No recent game");
    }
    
    Serial.println("✓ Postponed game display shown");
    return;
  }
  
  // Handle delayed games
  if (game.isDelayed) {
    tft.setFont(&FreeSansBold12pt7b);
    if (game.metsHome) {
      tft.setTextColor(WHITE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print(game.opponent);
      
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setTextColor(METS_ORANGE);
      tft.setCursor(startX + w1 + w2, 50);
      tft.print("NYM");
    } else {
      tft.setTextColor(METS_ORANGE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print("NYM");
      
      tft.setTextColor(WHITE);
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setCursor(startX + w1 + w2, 50);
      tft.print(game.opponent);
    }
    
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(WHITE);
    
    String delayedText = "DELAYED";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(delayedText, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 100);
    tft.println(delayedText);
    
    tft.drawLine(40, 114, 200, 114, WHITE);
    tft.drawLine(40, 115, 200, 115, WHITE);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    String prevLabel = "PREVIOUS GAME";
    int prevLabelWidth = prevLabel.length() * 6;
    tft.setCursor(120 - prevLabelWidth/2, 125);
    tft.println(prevLabel);
    
    if (game.inning.length() > 0 && game.inning.startsWith("LAST:")) {
      String lastGameInfo = game.inning.substring(6);
      
      int vsIndex = lastGameInfo.indexOf(" vs ");
      if (vsIndex > 0) {
        String scoreInfo = lastGameInfo.substring(0, vsIndex);
        String prevOpponent = lastGameInfo.substring(vsIndex + 4);
        
        int dashIndex = scoreInfo.indexOf("-");
        if (dashIndex > 0) {
          int prevMetsScore = scoreInfo.substring(0, dashIndex).toInt();
          int prevOppScore = scoreInfo.substring(dashIndex + 1).toInt();
          
          tft.setFont(&FreeSansBold12pt7b);
          tft.setTextColor(METS_ORANGE);
          
          int16_t x1, y1;
          uint16_t w1, h1, w2, h2;
          tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(prevOpponent, 0, 0, &x1, &y1, &w2, &h2);
          
          int spacing = 50;
          int nymX = 120 - spacing/2 - w1;
          int oppX = 120 + spacing/2;
          
          tft.setCursor(nymX, 155);
          tft.print("NYM");
          
          tft.setTextColor(WHITE);
          tft.setCursor(oppX, 155);
          tft.print(prevOpponent);
          
          tft.setTextColor(WHITE);
          String metsScoreStr = String(prevMetsScore);
          String oppScoreStr = String(prevOppScore);
          
          tft.getTextBounds(metsScoreStr, 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(oppScoreStr, 0, 0, &x1, &y1, &w2, &h2);
          
          int scoreNymX = nymX + (30 - w1) / 2;
          int scoreOppX = oppX + (prevOpponent.length() * 8 - w2) / 2;
          
          tft.setCursor(scoreNymX, 180);
          tft.print(prevMetsScore);
          
          tft.setCursor(scoreOppX, 180);
          tft.print(prevOppScore);
          
          tft.setFont(&FreeSansBold18pt7b);
          if (prevMetsScore > prevOppScore) {
            tft.setTextColor(WHITE);
            tft.getTextBounds("W", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("W");
          } else {
            tft.setTextColor(WHITE);
            tft.getTextBounds("L", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("L");
          }
        }
      }
    } else {
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(GRAY);
      tft.setCursor(85, 155);
      tft.println("No recent game");
    }
    
    Serial.println("✓ Delayed game display shown");
    return;
  }
  
  // Show regular game data for all other states
  if (game.isLive) {
    int logoX = 120 - (METS_LOGO_WIDTH * 0.5 / 2);   
    int logoY = 20; 
    drawMetsLogoScaled(logoX, logoY, METS_ORANGE, 0.5);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(GREEN);

    String liveText = "LIVE";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(liveText, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 90);
    tft.print(liveText);
    
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(WHITE);
    String awayTeamDisplay = game.awayTeam;
    String homeTeamDisplay = game.homeTeam;
    
    if (game.metsHome) {
      tft.getTextBounds(awayTeamDisplay, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(65 - w/2, 115);
      tft.print(awayTeamDisplay);
      
      tft.setTextColor(METS_ORANGE);
      tft.getTextBounds(homeTeamDisplay, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(175 - w/2, 115);
      tft.print(homeTeamDisplay);
    } else {
      tft.setTextColor(METS_ORANGE);
      tft.getTextBounds(awayTeamDisplay, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(80 - w/2, 120);
      tft.print(awayTeamDisplay);
      
      tft.setTextColor(WHITE);
      tft.getTextBounds(homeTeamDisplay, 0, 0, &x1, &y1, &w, &h);
      tft.setCursor(160 - w/2, 120);
      tft.print(homeTeamDisplay);
    }
    
    tft.drawLine(20, 125, 220, 125, WHITE);
    tft.drawLine(20, 126, 220, 126, WHITE);
    
    tft.setFont(&FreeSansBold24pt7b);
    
    String awayScoreStr = String(game.metsHome ? game.oppScore : game.metsScore);
    tft.setTextColor(game.metsHome ? WHITE : METS_ORANGE);
    tft.getTextBounds(awayScoreStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(55 - w/2, 170);
    tft.print(awayScoreStr);
    
    String homeScoreStr = String(game.metsHome ? game.metsScore : game.oppScore);
    tft.setTextColor(game.metsHome ? METS_ORANGE : WHITE);
    tft.getTextBounds(homeScoreStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(185 - w/2, 170);
    tft.print(homeScoreStr);
    
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(WHITE);
    
    String inningNumber = game.inning;
    if (inningNumber.length() == 0) {
      inningNumber = "1";
    }
    
    tft.getTextBounds(inningNumber, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 150);
    tft.print(inningNumber);
    
  } else if (game.isScheduled) {
    tft.setFont(&FreeSansBold12pt7b);
    
    if (game.metsHome) {
      tft.setTextColor(WHITE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print(game.opponent);
      
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setTextColor(METS_ORANGE);
      tft.setCursor(startX + w1 + w2, 50);
      tft.print("NYM");
    } else {
      tft.setTextColor(METS_ORANGE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print("NYM");
      
      tft.setTextColor(WHITE);
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setCursor(startX + w1 + w2, 50);
      tft.print(game.opponent);
    }
    
    String today = getCurrentDate();
    if (game.gameDate != today) {
      if (game.gameDate.length() >= 10) {
        int year = game.gameDate.substring(0, 4).toInt();
        int month = game.gameDate.substring(5, 7).toInt();
        int day = game.gameDate.substring(8, 10).toInt();
        
        String formattedDate = String(month) + "/" + String(day);
        
        tft.setFont();
        tft.setTextSize(1);
        tft.setTextColor(WHITE);
        int dateWidth = formattedDate.length() * 6;
        tft.setCursor(120 - dateWidth/2, 62);
        tft.println(formattedDate);
      }
    }
    
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(WHITE);
    
    String displayTime = game.gameTime;
    
    if (displayTime.length() > 8) {
      int lastSpaceIndex = displayTime.lastIndexOf(' ');
      int secondLastSpaceIndex = displayTime.lastIndexOf(' ', lastSpaceIndex - 1);
      
      if (secondLastSpaceIndex > 0) {
        String timePart = displayTime.substring(secondLastSpaceIndex + 1);
        displayTime = timePart;
      }
    }
    
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(displayTime, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 100);
    tft.println(displayTime);
    
    tft.drawLine(40, 114, 200, 114, WHITE);
    tft.drawLine(40, 115, 200, 115, WHITE);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    String prevLabel = "PREVIOUS GAME";
    int prevLabelWidth = prevLabel.length() * 6;
    tft.setCursor(120 - prevLabelWidth/2, 125);
    tft.println(prevLabel);
    
    if (game.inning.length() > 0 && game.inning.startsWith("LAST:")) {
      String lastGameInfo = game.inning.substring(6);
      
      int vsIndex = lastGameInfo.indexOf(" vs ");
      if (vsIndex > 0) {
        String scoreInfo = lastGameInfo.substring(0, vsIndex);
        String prevOpponent = lastGameInfo.substring(vsIndex + 4);
        
        int dashIndex = scoreInfo.indexOf("-");
        if (dashIndex > 0) {
          int prevMetsScore = scoreInfo.substring(0, dashIndex).toInt();
          int prevOppScore = scoreInfo.substring(dashIndex + 1).toInt();
          
          tft.setFont(&FreeSansBold12pt7b);
          tft.setTextColor(METS_ORANGE);
          
          int16_t x1, y1;
          uint16_t w1, h1, w2, h2;
          tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(prevOpponent, 0, 0, &x1, &y1, &w2, &h2);
          
          int spacing = 50;
          int nymX = 120 - spacing/2 - w1;
          int oppX = 120 + spacing/2;
          
          tft.setCursor(nymX, 155);
          tft.print("NYM");
          
          tft.setTextColor(WHITE);
          tft.setCursor(oppX, 155);
          tft.print(prevOpponent);
          
          tft.setTextColor(WHITE);
          String metsScoreStr = String(prevMetsScore);
          String oppScoreStr = String(prevOppScore);
          
          tft.getTextBounds(metsScoreStr, 0, 0, &x1, &y1, &w1, &h1);
          tft.getTextBounds(oppScoreStr, 0, 0, &x1, &y1, &w2, &h2);
          
          int scoreNymX = nymX + (30 - w1) / 2;
          int scoreOppX = oppX + (prevOpponent.length() * 8 - w2) / 2;
          
          tft.setCursor(scoreNymX, 180);
          tft.print(prevMetsScore);
          
          tft.setCursor(scoreOppX, 180);
          tft.print(prevOppScore);
          
          tft.setFont(&FreeSansBold18pt7b);
          if (prevMetsScore > prevOppScore) {
            tft.setTextColor(WHITE);
            tft.getTextBounds("W", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("W");
          } else {
            tft.setTextColor(WHITE);
            tft.getTextBounds("L", 0, 0, &x1, &y1, &w1, &h1);
            tft.setCursor(120 - w1/2, 210);
            tft.print("L");
          }
        }
      }
    } else {
      tft.setFont();
      tft.setTextSize(1);
      tft.setTextColor(GRAY);
      tft.setCursor(85, 155);
      tft.println("No recent game");
    }
  } else {
    tft.setFont(&FreeSansBold12pt7b);
    if (game.metsHome) {
      tft.setTextColor(WHITE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print(game.opponent);
      
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setTextColor(METS_ORANGE);
      tft.setCursor(startX + w1 + w2, 50);
      tft.print("NYM");
    } else {
      tft.setTextColor(METS_ORANGE);
      
      int16_t x1, y1;
      uint16_t w1, h1, w2, h2, w3, h3;
      tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w1, &h1);
      tft.getTextBounds(" @ ", 0, 0, &x1, &y1, &w2, &h2);
      tft.getTextBounds(game.opponent, 0, 0, &x1, &y1, &w3, &h3);
      
      int totalWidth = w1 + w2 + w3;
      int startX = 120 - totalWidth/2;
      
      tft.setCursor(startX, 50);
      tft.print("NYM");
      
      tft.setTextColor(WHITE);
      tft.setCursor(startX + w1, 50);
      tft.print(" @ ");
      
      tft.setCursor(startX + w1 + w2, 50);
      tft.print(game.opponent);
    }
    
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(WHITE);
    
    String oppScoreStr = String(game.oppScore);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(oppScoreStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 75);
    tft.println(oppScoreStr);
    
    tft.setFont();
    tft.setTextSize(1);
    if (game.isLive) {
      tft.setTextColor(GREEN);
      tft.setCursor(105, 105);
      tft.println("LIVE");
      
      if (game.inning.length() > 0) {
        tft.setTextColor(WHITE);
        int inningWidth = game.inning.length() * 6;
        tft.setCursor(120 - inningWidth/2, 120);
        tft.println(game.inning);
      }
    } else if (game.isFinal) {
      tft.setTextColor(WHITE);
      tft.setCursor(105, 105);
      tft.println("FINAL");
    } else {
      tft.setTextColor(GRAY);
      String displayState = game.detailedState;
      if (displayState.length() > 12) {
        displayState = game.status;
      }
      int detailWidth = displayState.length() * 6;
      tft.setCursor(120 - detailWidth/2, 105);
      tft.println(displayState);
    }
    
    tft.setTextColor(GRAY);
    tft.setCursor(115, 135);
    tft.println("VS");
    
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(METS_ORANGE);
    
    String metsScoreStr = String(game.metsScore);
    tft.getTextBounds(metsScoreStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 165);
    tft.println(metsScoreStr);
    
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(METS_ORANGE);
    tft.getTextBounds("NYM", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 195);
    tft.println("NYM");
  }
  
  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(GRAY);
  tft.setCursor(5, 230);
  tft.println("Updated: " + String(millis()/60000) + "m");
  
  Serial.println("✓ Game display updated");
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  
  clearScreenWithGradient();
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(WHITE);
  
  String connectingText = "CONNECTING";
  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds(connectingText, 0, 0, &x1, &y1, &w1, &h1);
  
  int connectingY = 120 - 15;
  tft.setCursor(120 - w1/2, connectingY);
  tft.print(connectingText);
  
  String wifiText = "WiFi";
  uint16_t w2, h2;
  tft.getTextBounds(wifiText, 0, 0, &x1, &y1, &w2, &h2);
  
  int wifiY = connectingY + 25;
  tft.setCursor(120 - w2/2, wifiY);
  tft.print(wifiText);
  
  tft.setFont();
  
  int centerX = 120;
  int centerY = 120;
  int outerRadius = 104;
  int innerRadius = 100;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(configuredSSID.c_str(), configuredPassword.c_str());
  
  int attempts = 0;
  int maxAttempts = 30;
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    attempts++;
    Serial.print(".");
    
    float progress = (float)attempts / maxAttempts;
    int endAngle = progress * 360;
    
    for (int angle = 0; angle <= endAngle; angle += 2) {
      float radians = (angle - 90) * PI / 180;
      
      for (int r = innerRadius; r <= outerRadius; r++) {
        int x = centerX + cos(radians) * r;
        int y = centerY + sin(radians) * r;
        
        if (x >= 0 && x < 240 && y >= 0 && y < 240) {
          tft.drawPixel(x, y, WHITE);
        }
      }
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✓ WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    for (int angle = 0; angle <= 360; angle += 2) {
      float radians = (angle - 90) * PI / 180;
      for (int r = innerRadius; r <= outerRadius; r++) {
        int x = centerX + cos(radians) * r;
        int y = centerY + sin(radians) * r;
        if (x >= 0 && x < 240 && y >= 0 && y < 240) {
          tft.drawPixel(x, y, WHITE);
        }
      }
    }
    
    delay(500);
    
    clearScreenWithGradient();
    
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(GREEN);
    
    String connectedText = "CONNECTED!";
    tft.getTextBounds(connectedText, 0, 0, &x1, &y1, &w1, &h1);
    
    tft.setCursor(120 - w1/2, 120);
    tft.print(connectedText);
    
    tft.setFont();
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    String ipText = "IP: " + WiFi.localIP().toString();
    int ipWidth = ipText.length() * 6;
    tft.setCursor(120 - ipWidth/2, 140);
    tft.println(ipText);
    
    delay(800);
  } else {
    Serial.println();
    Serial.println("✗ WiFi connection failed!");
    
    clearScreenWithGradient();
    
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(RED);
    
    String notConnectedText = "NOT CONNECTED";
    tft.getTextBounds(notConnectedText, 0, 0, &x1, &y1, &w1, &h1);
    tft.setCursor(120 - w1/2, 120);
    tft.print(notConnectedText);
    
    tft.setFont();
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    String checkWifiText = "Check WiFi settings";
    int checkWifiWidth = checkWifiText.length() * 6;
    tft.setCursor(120 - checkWifiWidth/2, 140);
    tft.println(checkWifiText);
    
    delay(3000);
    showOfflineScreen();
  }
  
  delay(800);
}

void showOfflineScreen() {
  Serial.println("Showing offline screen...");
  
  clearScreenWithGradient();
  
  int logoX = 120 - (METS_LOGO_WIDTH / 2);   
  int logoY = 120 - (METS_LOGO_HEIGHT / 2) - 25;  
  
  drawMetsLogoWithGradientBackground(logoX, logoY, METS_ORANGE);
  
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(METS_ORANGE);
  
  String text = "OFFLINE";
  
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  int textY = 120 + 70;
  
  tft.setCursor(120 - w/2, textY);
  tft.print(text);
  
  tft.setFont();
}

String getTeamAbbreviation(String fullName) {
  fullName.toLowerCase();
  
  if (fullName.indexOf("orioles") >= 0) return "BAL";
  if (fullName.indexOf("red sox") >= 0) return "BOS";
  if (fullName.indexOf("yankees") >= 0) return "NYY";
  if (fullName.indexOf("rays") >= 0) return "TB";
  if (fullName.indexOf("blue jays") >= 0) return "TOR";
  if (fullName.indexOf("white sox") >= 0) return "CWS";
  if (fullName.indexOf("guardians") >= 0) return "CLE";
  if (fullName.indexOf("tigers") >= 0) return "DET";
  if (fullName.indexOf("royals") >= 0) return "KC";
  if (fullName.indexOf("twins") >= 0) return "MIN";
  if (fullName.indexOf("astros") >= 0) return "HOU";
  if (fullName.indexOf("angels") >= 0) return "LAA";
  if (fullName.indexOf("athletics") >= 0) return "OAK";
  if (fullName.indexOf("mariners") >= 0) return "SEA";
  if (fullName.indexOf("rangers") >= 0) return "TEX";
  if (fullName.indexOf("braves") >= 0) return "ATL";
  if (fullName.indexOf("marlins") >= 0) return "MIA";
  if (fullName.indexOf("mets") >= 0) return "NYM";
  if (fullName.indexOf("phillies") >= 0) return "PHI";
  if (fullName.indexOf("nationals") >= 0) return "WSN";
  if (fullName.indexOf("cubs") >= 0) return "CHC";
  if (fullName.indexOf("reds") >= 0) return "CIN";
  if (fullName.indexOf("brewers") >= 0) return "MIL";
  if (fullName.indexOf("pirates") >= 0) return "PIT";
  if (fullName.indexOf("cardinals") >= 0) return "STL";
  if (fullName.indexOf("diamondbacks") >= 0) return "ARI";
  if (fullName.indexOf("rockies") >= 0) return "COL";
  if (fullName.indexOf("dodgers") >= 0) return "LAD";
  if (fullName.indexOf("padres") >= 0) return "SD";
  if (fullName.indexOf("giants") >= 0) return "SF";
  
  String result = "";
  bool nextChar = true;
  for (int i = 0; i < fullName.length() && result.length() < 3; i++) {
    char c = fullName.charAt(i);
    if (c == ' ') {
      nextChar = true;
    } else if (nextChar && isAlpha(c)) {
      result += (char)toupper(c);
      nextChar = false;
    }
  }
  
  return result.length() > 0 ? result : "UNK";
}

uint16_t interpolateColor(uint16_t color1, uint16_t color2, float factor) {
  if (factor <= 0.0) return color1;
  if (factor >= 1.0) return color2;
  
  uint8_t r1 = (color1 >> 11) & 0x1F;
  uint8_t g1 = (color1 >> 5) & 0x3F;
  uint8_t b1 = color1 & 0x1F;
  
  uint8_t r2 = (color2 >> 11) & 0x1F;
  uint8_t g2 = (color2 >> 5) & 0x3F;
  uint8_t b2 = color2 & 0x1F;
  
  uint8_t r = r1 + (uint8_t)((r2 - r1) * factor);
  uint8_t g = g1 + (uint8_t)((g2 - g1) * factor);
  uint8_t b = b1 + (uint8_t)((b2 - b1) * factor);
  
  return (r << 11) | (g << 5) | b;
}

void drawMetsLogoScaled(int x, int y, uint16_t logoColor, float scale) {
  int scaledWidth = METS_LOGO_WIDTH * scale;
  int scaledHeight = METS_LOGO_HEIGHT * scale;

  for (int16_t j = 0; j < scaledHeight; j++) {
    for (int16_t i = 0; i < scaledWidth; i++) {
      int16_t pixelX = x + i;
      int16_t pixelY = y + j;

      if (pixelX < 0 || pixelX >= 240 || pixelY < 0 || pixelY >= 240) continue;

      int origX = i / scale;
      int origY = j / scale;

      if (origX >= METS_LOGO_WIDTH || origY >= METS_LOGO_HEIGHT) continue;

      int16_t byteWidth = (METS_LOGO_WIDTH + 7) / 8;
      uint8_t byte = pgm_read_byte(&mets_logo_bitmap[origY * byteWidth + origX / 8]);

      if (byte & (128 >> (origX & 7))) {
        tft.drawPixel(pixelX, pixelY, logoColor);
      } else {
        uint16_t gradientColor = getBlueGradientColorAtXY(pixelX, pixelY);
        tft.drawPixel(pixelX, pixelY, gradientColor);
      }
    }
  }
}

// ========== CONFIGURATION FUNCTIONS ==========

void loadConfiguration() {
  preferences.begin("scorepuck", false);

  configuredSSID = preferences.getString("wifi_ssid", "");
  configuredPassword = preferences.getString("wifi_pass", "");
  configuredTeamId = preferences.getInt("team_id", 121);
  configuredTeamAbbr = preferences.getString("team_abbr", "NYM");
  configuredTeamName = preferences.getString("team_name", "Mets");
  configuredPrimaryColor = preferences.getUShort("color_pri", 0x001F);
  configuredSecondaryColor = preferences.getUShort("color_sec", 0xFD20);

  preferences.end();

  Serial.println("Configuration loaded:");
  Serial.println("  WiFi SSID: " + configuredSSID);
  Serial.println("  Team: " + configuredTeamName + " (" + configuredTeamAbbr + ")");
  Serial.println("  Team ID: " + String(configuredTeamId));
}

void saveConfiguration() {
  preferences.begin("scorepuck", false);

  preferences.putString("wifi_ssid", configuredSSID);
  preferences.putString("wifi_pass", configuredPassword);
  preferences.putInt("team_id", configuredTeamId);
  preferences.putString("team_abbr", configuredTeamAbbr);
  preferences.putString("team_name", configuredTeamName);
  preferences.putUShort("color_pri", configuredPrimaryColor);
  preferences.putUShort("color_sec", configuredSecondaryColor);

  preferences.end();

  Serial.println("Configuration saved!");
}

void clearConfiguration() {
  preferences.begin("scorepuck", false);
  preferences.clear();
  preferences.end();

  Serial.println("Configuration cleared!");
  Serial.println("Device will restart in config mode...");

  delay(1000);
  ESP.restart();
}

void startConfigPortal() {
  configMode = true;

  Serial.println("Stopping any existing WiFi connections...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(2000);  // Longer delay for ESP32-S3

  // Configure static IP for the AP BEFORE starting AP mode
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  Serial.println("Setting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(2000);  // Longer delay for ESP32-S3

  Serial.println("Configuring AP network settings...");
  Serial.println("  IP: " + local_IP.toString());
  Serial.println("  Gateway: " + gateway.toString());
  Serial.println("  Subnet: " + subnet.toString());

  // Configure network settings
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("✗ AP Config Failed!");
  } else {
    Serial.println("✓ AP Config successful");
  }

  delay(1000);

  // Start Access Point with explicit parameters
  // For ESP32-S3: SSID, password, channel, hidden, max_connections
  Serial.println("Starting Access Point (OPEN - No Password)...");
  Serial.println("  SSID: " + String(configSSID));
  Serial.println("  Channel: 1");
  Serial.println("  Max Connections: 4");

  // Try with no password, channel 1, not hidden, max 4 connections
  bool apStarted = WiFi.softAP(configSSID, NULL, 1, 0, 4);

  delay(2000);

  if (!apStarted) {
    Serial.println("  Failed on channel 1, trying channel 6...");
    apStarted = WiFi.softAP(configSSID, NULL, 6, 0, 4);
    delay(2000);
  }

  if (!apStarted) {
    Serial.println("  Failed with parameters, trying simple mode...");
    apStarted = WiFi.softAP(configSSID);
    delay(2000);
  }

  if (apStarted) {
    Serial.println("✓ Access Point started successfully!");
  } else {
    Serial.println("✗ Access Point FAILED!");
  }

  delay(1000);

  IPAddress IP = WiFi.softAPIP();
  Serial.println();
  Serial.println("==============================================");
  Serial.println("Configuration Portal Started");
  Serial.println("==============================================");
  Serial.println("SSID: " + String(configSSID));
  Serial.println("Password: NONE (Open Network)");
  Serial.println("IP Address: " + IP.toString());
  Serial.println("URL: http://" + IP.toString());
  Serial.println("==============================================");
  Serial.println("Connect your phone to the WiFi network above");
  Serial.println("Then open a browser and go to the URL above");
  Serial.println();
  Serial.println("DHCP Range: 192.168.4.2 - 192.168.4.10");
  Serial.println("Your phone should get an IP in this range");
  Serial.println("==============================================");
  Serial.println();
  Serial.println("Waiting for connections...");

  // Display config mode on screen
  clearScreenWithGradient();
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(WHITE);

  String text1 = "CONFIG MODE";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text1, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(120 - w/2, 60);
  tft.print(text1);

  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(WHITE);

  String text2 = "Connect to WiFi:";
  int width2 = text2.length() * 6;
  tft.setCursor(120 - width2/2, 90);
  tft.print(text2);

  tft.setTextColor(METS_ORANGE);
  String text3 = String(configSSID);
  int width3 = text3.length() * 6;
  tft.setCursor(120 - width3/2, 105);
  tft.print(text3);

  tft.setTextColor(WHITE);
  String text4 = "NO PASSWORD";
  int width4 = text4.length() * 6;
  tft.setCursor(120 - width4/2, 125);
  tft.print(text4);

  tft.setTextColor(GRAY);
  String text5 = "(Open Network)";
  int width5 = text5.length() * 6;
  tft.setCursor(120 - width5/2, 140);
  tft.print(text5);

  tft.setTextColor(WHITE);
  String text6 = "Then browse to:";
  int width6 = text6.length() * 6;
  tft.setCursor(120 - width6/2, 165);
  tft.print(text6);

  tft.setTextColor(GREEN);
  String text7 = "192.168.4.1";
  int width7 = text7.length() * 6;
  tft.setCursor(120 - width7/2, 180);
  tft.print(text7);

  // Setup DNS server for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Setup web server
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);  // Redirect all requests to config page
  server.begin();

  // Stay in config mode until configured
  int lastClientCount = 0;
  unsigned long lastStatusPrint = 0;

  while (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();

    // Show connection status every 5 seconds
    if (millis() - lastStatusPrint > 5000) {
      int clientCount = WiFi.softAPgetStationNum();
      if (clientCount != lastClientCount) {
        Serial.print("Connected devices: ");
        Serial.println(clientCount);
        lastClientCount = clientCount;
      }
      lastStatusPrint = millis();
    }

    delay(10);
  }
}

void handleRoot() {
  String wifiStatus = WiFi.status() == WL_CONNECTED ? "Connected" : "Not Connected";
  String ipAddress = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "N/A";

  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ScorePuck Configuration</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #001F3F; text-align: center; }";
  html += "label { display: block; margin-top: 15px; font-weight: bold; }";
  html += "input, select { width: 100%; padding: 10px; margin-top: 5px; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }";
  html += "button { width: 100%; padding: 12px; margin-top: 20px; background: #FF6600; color: white; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; }";
  html += "button:hover { background: #FF4400; }";
  html += ".info { background: #e3f2fd; padding: 10px; border-radius: 5px; margin-top: 10px; font-size: 14px; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>&#9917; ScorePuck Setup</h1>";
  html += "<form action='/save' method='POST'>";
  html += "<h2>WiFi Settings</h2>";
  html += "<label>WiFi Network (SSID):</label>";
  html += "<input type='text' name='ssid' placeholder='Your WiFi Network' required>";
  html += "<label>WiFi Password:</label>";
  html += "<input type='password' name='password' placeholder='Your WiFi Password' required>";
  html += "<h2>Team Settings</h2>";
  html += "<label>MLB Team:</label>";
  html += "<select name='team' id='teamSelect' onchange='updateTeamInfo()'>";
  html += "<option value='109:ARI:Diamondbacks:7841:FC4C02'>Arizona Diamondbacks</option>";
  html += "<option value='144:ATL:Braves:002F5F:CE1141'>Atlanta Braves</option>";
  html += "<option value='110:BAL:Orioles:DF4601:000000'>Baltimore Orioles</option>";
  html += "<option value='111:BOS:Red Sox:BD3039:0C2340'>Boston Red Sox</option>";
  html += "<option value='112:CHC:Cubs:0E3386:CC3433'>Chicago Cubs</option>";
  html += "<option value='145:CWS:White Sox:27251F:C4CED4'>Chicago White Sox</option>";
  html += "<option value='113:CIN:Reds:C6011F:000000'>Cincinnati Reds</option>";
  html += "<option value='114:CLE:Guardians:E31937:002B5C'>Cleveland Guardians</option>";
  html += "<option value='115:COL:Rockies:33006F:C4CED4'>Colorado Rockies</option>";
  html += "<option value='116:DET:Tigers:0C2340:FA4616'>Detroit Tigers</option>";
  html += "<option value='117:HOU:Astros:EB6E1F:002D62'>Houston Astros</option>";
  html += "<option value='118:KC:Royals:004687:BD9B60'>Kansas City Royals</option>";
  html += "<option value='108:LAA:Angels:BA0021:003263'>Los Angeles Angels</option>";
  html += "<option value='119:LAD:Dodgers:005A9C:EF3E42'>Los Angeles Dodgers</option>";
  html += "<option value='146:MIA:Marlins:00A3E0:EF3340'>Miami Marlins</option>";
  html += "<option value='158:MIL:Brewers:FFC52F:12284B'>Milwaukee Brewers</option>";
  html += "<option value='142:MIN:Twins:002B5C:D31145'>Minnesota Twins</option>";
  html += "<option value='121:NYM:Mets:002D72:FF5910' selected>New York Mets</option>";
  html += "<option value='147:NYY:Yankees:003087:E4002C'>New York Yankees</option>";
  html += "<option value='133:OAK:Athletics:003831:EFB21E'>Oakland Athletics</option>";
  html += "<option value='143:PHI:Phillies:E81828:002D72'>Philadelphia Phillies</option>";
  html += "<option value='134:PIT:Pirates:FDB827:27251F'>Pittsburgh Pirates</option>";
  html += "<option value='135:SD:Padres:2F241D:FFC425'>San Diego Padres</option>";
  html += "<option value='137:SF:Giants:FD5A1E:27251F'>San Francisco Giants</option>";
  html += "<option value='136:SEA:Mariners:0C2C56:005C5C'>Seattle Mariners</option>";
  html += "<option value='138:STL:Cardinals:C41E3A:0C2340'>St. Louis Cardinals</option>";
  html += "<option value='139:TB:Rays:092C5C:8FBCE6'>Tampa Bay Rays</option>";
  html += "<option value='140:TEX:Rangers:003278:C0111F'>Texas Rangers</option>";
  html += "<option value='141:TOR:Blue Jays:134A8E:1D2D5C'>Toronto Blue Jays</option>";
  html += "<option value='120:WSH:Nationals:AB0003:14225A'>Washington Nationals</option>";
  html += "</select>";
  html += "<div class='info'>";
  html += "<strong>Selected Team:</strong> <span id='teamName'>New York Mets</span><br>";
  html += "<strong>Team ID:</strong> <span id='teamId'>121</span><br>";
  html += "<strong>Abbreviation:</strong> <span id='teamAbbr'>NYM</span>";
  html += "</div>";
  html += "<button type='submit'>Save & Restart</button>";
  html += "</form>";
  html += "<div class='info' style='margin-top: 20px;'>";
  html += "<strong>Current Status:</strong><br>";
  html += "WiFi: " + wifiStatus + "<br>";
  html += "Team: " + configuredTeamName + " (" + configuredTeamAbbr + ")<br>";
  html += "IP: " + ipAddress;
  html += "</div></div>";
  html += "<script>";
  html += "function updateTeamInfo() {";
  html += "var select = document.getElementById('teamSelect');";
  html += "var parts = select.value.split(':');";
  html += "document.getElementById('teamId').innerText = parts[0];";
  html += "document.getElementById('teamAbbr').innerText = parts[1];";
  html += "document.getElementById('teamName').innerText = select.options[select.selectedIndex].text;";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("team")) {
    configuredSSID = server.arg("ssid");
    configuredPassword = server.arg("password");

    // Parse team selection: "ID:ABBR:NAME:COLOR1:COLOR2"
    String teamData = server.arg("team");
    int idx1 = teamData.indexOf(':');
    int idx2 = teamData.indexOf(':', idx1 + 1);
    int idx3 = teamData.indexOf(':', idx2 + 1);
    int idx4 = teamData.indexOf(':', idx3 + 1);

    configuredTeamId = teamData.substring(0, idx1).toInt();
    configuredTeamAbbr = teamData.substring(idx1 + 1, idx2);
    configuredTeamName = teamData.substring(idx2 + 1, idx3);

    // Convert hex colors to RGB565
    String color1Hex = teamData.substring(idx3 + 1, idx4);
    String color2Hex = teamData.substring(idx4 + 1);

    // Simple hex to RGB565 conversion
    long c1 = strtol(color1Hex.c_str(), NULL, 16);
    long c2 = strtol(color2Hex.c_str(), NULL, 16);

    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;
    configuredPrimaryColor = ((r1 & 0xF8) << 8) | ((g1 & 0xFC) << 3) | (b1 >> 3);

    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;
    configuredSecondaryColor = ((r2 & 0xF8) << 8) | ((g2 & 0xFC) << 3) | (b2 >> 3);

    saveConfiguration();

    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Configuration Saved</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; text-align: center; }";
    html += ".container { max-width: 500px; margin: 50px auto; background: white; padding: 40px; border-radius: 10px; }";
    html += "h1 { color: #00A000; }";
    html += "p { font-size: 18px; margin: 20px 0; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>&#10004; Configuration Saved!</h1>";
    html += "<p>ScorePuck will now restart and connect to your WiFi network.</p>";
    html += "<p>The device will be accessible at the IP address shown on the display.</p>";
    html += "</div></body></html>";

    server.send(200, "text/html", html);
    delay(2000);

    configMode = false;
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing required parameters");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"team\":\"" + configuredTeamName + "\"";
  json += ",\"team_abbr\":\"" + configuredTeamAbbr + "\"";
  json += ",\"team_id\":" + String(configuredTeamId);
  json += "}";

  server.send(200, "application/json", json);
}

void setupOTA() {
  ArduinoOTA.setHostname("ScorePuck");
  ArduinoOTA.setPassword("scorepuck123");  // Set OTA password

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Starting...");
    clearScreenWithGradient();
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(YELLOW);

    String text = "UPDATING...";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 120);
    tft.print(text);
    tft.setFont();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete!");
    clearScreenWithGradient();
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(GREEN);

    String text = "UPDATE OK!";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor(120 - w/2, 120);
    tft.print(text);
    tft.setFont();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.println("Use Arduino IDE: Sketch -> Upload Using Programmer");
  Serial.println("Or use: platformio run --target upload --upload-port ScorePuck.local");
}