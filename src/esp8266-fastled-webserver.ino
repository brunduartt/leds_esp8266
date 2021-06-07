/*
   ESP8266 FastLED WebServer: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//#define FASTLED_ALLOW_INTERRUPTS 0
//#define INTERRUPT_THRESHOLD 1
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define BUFFER_LEN 1024
#include <FastLED.h>
FASTLED_USING_NAMESPACE
extern "C" {
#include "user_interface.h"
}
#include <WiFiUdp.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
//#include <WebSocketsServer.h>
#include <FS.h>
#include <EEPROM.h>
//#include <IRremoteESP8266.h>
#include "GradientPalettes.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "Field.h"

//#define RECV_PIN D4
//IRrecv irReceiver(RECV_PIN);

//#include "Commands.h"
//248
//247 - parede
//246 - escrivaninha
IPAddress ip(192,168,0,246);  //Uncomment if apMode = false  
IPAddress gateway(192,168,0,254);   
IPAddress subnet(255,255,255,0);

ESP8266WebServer webServer(80);
//WebSocketsServer webSocketsServer = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdateServer;

WiFiUDP port;
char packetBuffer[BUFFER_LEN];
unsigned int localPort = 7777;

#include "FSBrowser.h"
//4 = d2
#define DATA_PIN      4
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
#define NUM_LEDS_PER_STRIP 300
#define NUM_STRIPS 1
#define NUM_LEDS      (NUM_LEDS_PER_STRIP * NUM_STRIPS)
#define MILLI_AMPS         2000 // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define FRAMES_PER_SECOND  120  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.
#define HAS_TWO_STRIPS  false //IF HAS TWO STRIPS IN THE SAME ESP (WILL INVERT THE POSITIONS OF THE FIRST STRIP SO THAT IT CONNECTS WITH THE SECOND ONE!)
String nameString;

const bool apMode = false;
#include "Ping.h";
#include "LedInterval.h";
#include "Secrets.h" // this file is intentionally not included in the sketch, so nobody accidentally commits their secret information.
// create a Secrets.h file with the following:

// AP mode password
// const char WiFiAPPSK[] = "your-password";

// Wi-Fi network to connect to (if not in AP mode)
// char* ssid = "your-ssid";
// char* password = "your-password";

bool loaded = false;
CRGB leds[NUM_LEDS];
LedInterval fullStrip;
uint8_t changeFullStrip = true;
uint8_t currentChangingInterval = 0;
bool changeAllIntervals = false;
bool divideLastInterval = false;
uint8_t intervalsAmount = 0;
std::vector<LedInterval> intervals;
const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
uint8_t brightnessIndex = 0;
// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
uint8_t secondsPerPalette = 10;

CRGBPalette16 WaterColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
uint8_t cooling = 49;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
uint8_t sparking = 60;

uint8_t speed = 30;

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

bool reloadPattern = true;
uint8_t currentPatternIndex = 0; // Index number of which pattern is current
uint8_t autoplay = 0;

uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB::Blue;

uint8_t soundReactiveValue = 0;
uint8_t oldSoundReactiveValue = 1;
// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(value);
  }
}

typedef void (*Pattern)(LedInterval* interval);
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

#include "Twinkles.h"
#include "TwinkleFOX.h"

// List of patterns to cycle through.  Each is defined as a separate function below.
#define solidColorPaletteIndex  0
PatternAndNameList patterns = {
  { showSolidColor,         "Solid Color" },
  { fireworks,              "Fireworks" },
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { startEndJoin,           "Start End Join" },
  { blink,                  "Blink" },
  { solidPalette,           "Solid Palette"},
  { colorsPalette,          "Colors Palette"},
  { rainbow,                "Rainbow" },
  { rainbowWithGlitter,     "Rainbow With Glitter" },
  { rainbowSolid,           "Solid Rainbow" },
  { confetti,               "Confetti" },
  { sinelon,                "Sinelon" },
  { bpm,                    "Beat" },
  { juggle,                 "Juggle" },
  { heatMap,                "Heat Map" },
  { fire,                   "Fire" },
  { water,                  "Water" },
  { soundReactiveBars,      "Sound Reactive Bars" },
  { soundReactiveBrightness,"Sound Reactive Brightness" },
  { soundReactiveBrightnessAndSpeed,"Sound Reactive Brightness And Speed" },
  // twinkle patterns
  { rainbowTwinkles,        "Rainbow Twinkles" },
  { snowTwinkles,           "Snow Twinkles" },
  { cloudTwinkles,          "Cloud Twinkles" },
  { incandescentTwinkles,   "Incandescent Twinkles" },

  // TwinkleFOX patterns
  { retroC9Twinkles,        "Retro C9 Twinkles" },
  { redWhiteTwinkles,       "Red & White Twinkles" },
  { blueWhiteTwinkles,      "Blue & White Twinkles" },
  { redGreenWhiteTwinkles,  "Red, Green & White Twinkles" },
  { fairyLightTwinkles,     "Fairy Light Twinkles" },
  { snow2Twinkles,          "Snow 2 Twinkles" },
  { hollyTwinkles,          "Holly Twinkles" },
  { iceTwinkles,            "Ice Twinkles" },
  { partyTwinkles,          "Party Twinkles" },
  { forestTwinkles,         "Forest Twinkles" },
  { lavaTwinkles,           "Lava Twinkles" },
  { fireTwinkles,           "Fire Twinkles" },
  { cloud2Twinkles,         "Cloud 2 Twinkles" },
  { oceanTwinkles,          "Ocean Twinkles" },

};

const uint8_t patternCount = ARRAY_SIZE(patterns);

typedef struct {
  CRGBPalette16 palette;
  String name;
} PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
    RainbowColors_p,
    RainbowStripeColors_p,
    CloudColors_p,
    LavaColors_p,
    OceanColors_p,
    ForestColors_p,
    PartyColors_p,
    HeatColors_p,
    Euphoria_p,
    Euphoria_2_p,
    Euphoria_3_p,
    Sunset_Real_gp,
    es_rivendell_15_gp,
    es_ocean_breeze_036_gp,
    rgi_15_gp,
    retro2_16_gp,
    Analogous_1_gp,
    es_pinksplash_08_gp,
    Coral_reef_gp,
    es_ocean_breeze_068_gp,
    es_pinksplash_07_gp,
    es_vintage_01_gp,
    departure_gp,
    es_landscape_64_gp,
    es_landscape_33_gp,
    rainbowsherbet_gp,
    gr65_hult_gp,
    gr64_hult_gp,
    GMT_drywet_gp,
    ib_jul01_gp,
    es_vintage_57_gp,
    ib15_gp,
    Fuschia_7_gp,
    es_emerald_dragon_08_gp,
    lava_gp,
    fire_gp,
    Colorfull_gp,
    Magenta_Evening_gp,
    Pink_Purple_gp,
    es_autumn_19_gp,
    BlacK_Blue_Magenta_White_gp,
    BlacK_Magenta_Red_gp,
    BlacK_Red_Magenta_Yellow_gp,
    Blue_Cyan_Yellow_gp 
};
#define rainbowPalleteIndex  0
const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
    "Rainbow",
    "Rainbow Stripe",
    "Cloud",
    "Lava",
    "Ocean",
    "Forest",
    "Party",
    "Heat",
    "Euphoria",
    "Euphoria 2",
    "Euphoria 3",
    "Sunset",
    "Rivendell",
    "Ocean Breeze 036",
    "Unicorn",
    "Retro",
    "Analogous",
    "Pinksplash 08",
    "Coral Reef",
    "Ocean Breeze 68",
    "Pinksplash 07",
    "Vintage 01",
    "Departure",
    "Landscape_64",
    "Landscape_33",
    "Rainbow Sherbet",
    "65 Hult",
    "64 Hult",
    "Dry Wet",
    "Jul",
    "Vintage 57",
    "IB15",
    "Fuschia",
    "Emerald Dragon",
    "Lava_GP",
    "Fire_GP",
    "Colorfull",
    "Magenta Evening",
    "Pink Purple",
    "Autumn",
    "BlacK_Blue_Magenta_White",
    "BlacK_Magenta_Red",
    "BlacK_Red_Magenta_Yellow",
    "Blue_Cyan_Yellow"
};

#include "Fields.h"

void setup() {
  WiFi.disconnect(); //Uncomment if apMode = false
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  Serial.begin(9600);
  delay(100);
  Serial.setDebugOutput(true);

  //FastLED.addLeds<WS2812B_PORTA,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);         // for WS2812 (Neopixel)
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS); // for APA102 (Dotstar)
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  loadSettings(); //Uncomment if apMode = false
  EEPROM.begin(512);
  //loadSettings(); //Uncomment if apMode = true
  FastLED.setBrightness(255);
  //  irReceiver.enableIRIn(); // Start the receiver
  Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
  Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
  Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
  Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println();

  SPIFFS.begin();
  {
    Serial.println("SPIFFS contents:");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  //disabled due to https://github.com/jasoncoon/esp8266-fastled-webserver/issues/62
  //initializeWiFi();

  // Do a little work to get a unique-ish name. Get the
  // last two bytes of the MAC (HEX'd)":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();

//  nameString = "W1 ESP8266 " + macID;
  nameString = "ES ESP8266 " + macID;
  char nameChar[nameString.length() + 1];
  memset(nameChar, 0, nameString.length() + 1);

  for (int i = 0; i < nameString.length(); i++)
    nameChar[i] = nameString.charAt(i);

  Serial.printf("Name: %s\n", nameChar );
  if (apMode)
  {
    WiFi.mode(WIFI_AP);

    WiFi.softAP(nameChar, WiFiAPPSK);

    Serial.printf("Connect to Wi-Fi access point: %s\n", nameChar);
    Serial.println("and open http://192.168.4.1 in your browser");
  }
  else
  {
    WiFi.config(ip, gateway, subnet);  //Uncomment if apMode = false
    WiFi.mode(WIFI_STA);
    WiFi.hostname(nameString);
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
      WiFi.begin(ssid, password);
      port.begin(localPort);
    }
    
    Serial.println(WiFi.localIP());
  }
  httpUpdateServer.setup(&webServer);

  webServer.on("/all", HTTP_GET, []() {
    String json = getFieldsJson(fields, fieldCount);
    webServer.send(200, "text/json", json);
  });

  webServer.on("/fieldValue", HTTP_GET, []() {
    String name = webServer.arg("name");
    String value = getFieldValue(name, fields, fieldCount);
    webServer.send(200, "text/json", value);
  });

  webServer.on("/fieldValue", HTTP_POST, []() {
    String name = webServer.arg("name");
    String value = webServer.arg("value");
    String newValue = setFieldValue(name, value, fields, fieldCount);
    webServer.send(200, "text/json", newValue);
  });

  webServer.on("/power", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPower(value.toInt());
    sendInt(power);
  });

  webServer.on("/cooling", HTTP_POST, []() {
    String value = webServer.arg("value");
    cooling = value.toInt();
    if(changeFullStrip) {
      fullStrip.cooling = cooling;
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].cooling = cooling;
        }
      } else {
        intervals[currentChangingInterval].cooling = cooling;
      }
    }
    broadcastInt("cooling", cooling);
    sendInt(cooling);
  });

  webServer.on("/sparking", HTTP_POST, []() {
    String value = webServer.arg("value");
    sparking = value.toInt();
    if(changeFullStrip) {
      fullStrip.sparking = sparking;
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].sparking = sparking;
        }
      } else {
        intervals[currentChangingInterval].sparking = sparking;
      }
    }
    broadcastInt("sparking", sparking);
    sendInt(sparking);
  });

  webServer.on("/speed", HTTP_POST, []() {
    String value = webServer.arg("value");
    speed = value.toInt();
    reloadPattern = true;
    if(changeFullStrip) {
      fullStrip.speed = speed;
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].speed = speed;
        }
      } else {
        intervals[currentChangingInterval].speed = speed;
      }
    }
    broadcastInt("speed", speed);
    sendInt(speed);
  });

  webServer.on("/twinkleSpeed", HTTP_POST, []() {
    String value = webServer.arg("value");
    twinkleSpeed = value.toInt();
    if (twinkleSpeed < 0) twinkleSpeed = 0;
    else if (twinkleSpeed > 8) twinkleSpeed = 8;
    if(changeFullStrip) {
      fullStrip.twinkleSpeed = twinkleSpeed;
      fullStrip.updateVars();
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].twinkleSpeed = twinkleSpeed;
          intervals[i].updateVars();
        }
      } else {
        intervals[currentChangingInterval].twinkleSpeed = twinkleSpeed;
        intervals[currentChangingInterval].updateVars();
      }
    }
    broadcastInt("twinkleSpeed", twinkleSpeed);
    sendInt(twinkleSpeed);
  });

  webServer.on("/twinkleDensity", HTTP_POST, []() {
    String value = webServer.arg("value");
    twinkleDensity = value.toInt();
    if (twinkleDensity < 0) twinkleDensity = 0;
    if(changeFullStrip) {
      fullStrip.twinkleDensity = twinkleDensity;
      fullStrip.updateVars();
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].twinkleDensity = twinkleDensity;
          intervals[i].updateVars();
        }
      } else {
        intervals[currentChangingInterval].twinkleDensity = twinkleDensity;
        intervals[currentChangingInterval].updateVars();
      }
    }
    broadcastInt("twinkleDensity", twinkleDensity);
    sendInt(twinkleDensity);
  });


webServer.on("/amountDivisions", HTTP_POST, []() {
    String value = webServer.arg("value");
    uint8_t amountDivisions = value.toInt();
    if (amountDivisions <= 0) amountDivisions = 0;
    if(changeFullStrip) {
      fullStrip.setDivisions(amountDivisions);
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].setDivisions(amountDivisions);
        }
      } else {
        intervals[currentChangingInterval].setDivisions(amountDivisions);
      }
    }
    broadcastInt("amountDivisions", amountDivisions);
    sendInt(amountDivisions);
  });


  webServer.on("/solidColor", HTTP_POST, []() {
    String r = webServer.arg("r");
    String g = webServer.arg("g");
    String b = webServer.arg("b");
    setSolidColor(r.toInt(), g.toInt(), b.toInt());
    sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
  });

  webServer.on("/pattern", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPattern(value.toInt());
    sendInt(currentPatternIndex);
  });

  webServer.on("/soundReactive", HTTP_POST, []() {
    String value = webServer.arg("value");
    soundReactiveValue = value.toInt();
    sendInt(soundReactiveValue);
  });

  webServer.on("/patternName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPatternName(value);
    sendInt(currentPatternIndex);
  });

  webServer.on("/palette", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPalette(value.toInt());
    sendInt(currentPaletteIndex);
  });

  webServer.on("/paletteName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPaletteName(value);
    sendInt(currentPaletteIndex);
  });

  webServer.on("/invertDirection", HTTP_POST, []() {
    String value = webServer.arg("value");
    setInvertDirection(value.toInt());
    sendInt(value.toInt());
  });

  webServer.on("/useSolidColor", HTTP_POST, []() {
    String value = webServer.arg("value");
    setUseSolidColor(value.toInt());
    sendInt(value.toInt());
  });

  webServer.on("/interval", HTTP_POST, []() {
    String value = webServer.arg("value");
    setChangingInterval(value.toInt());
    sendInt(currentChangingInterval);
  });

  webServer.on("/intervalOptions", HTTP_POST, []() {
    String value = webServer.arg("value");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    uint8_t option = value.toInt();
    if(option == 0) {
      addInterval();
    } else if(option == 1) {
      deleteInterval();
    } else if(option == 2) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals.at(i).updateVars();
      }
    }
    sendInt(intervalsAmount);
  });

 webServer.on("/fullStrip", HTTP_POST, []() {
    String value = webServer.arg("value");
    changeFullStrip = value.toInt();
    reloadPattern = true;
    changeAllIntervals = false;
    sendInt(changeFullStrip);
  });

 webServer.on("/changeAllIntervals", HTTP_POST, []() {
    String value = webServer.arg("value");
    changeAllIntervals = value.toInt();
    sendInt(changeAllIntervals);
  });


 webServer.on("/intervalStart", HTTP_POST, []() {
    String value = webServer.arg("value");
    fadeToBlackByRange(leds, 0, NUM_LEDS, 255);
    reloadPattern = true;
    if(changeFullStrip) {
      fullStrip.updateStart(value.toInt()-1);
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].updateStart(value.toInt()-1);
        }
      } else {
        intervals[currentChangingInterval].updateStart(value.toInt()-1);
      }
    }
    sendInt(value.toInt()-1);
  });

 webServer.on("/intervalEnd", HTTP_POST, []() {
    String value = webServer.arg("value");
    fadeToBlackByRange(leds, 0, NUM_LEDS, 255);
    reloadPattern = true;
    if(changeFullStrip) {
      fullStrip.updateEnd(value.toInt());
    } else {
      if(changeAllIntervals) {
        for(int i = 0; i < intervalsAmount; i++) {
          intervals[i].updateEnd(value.toInt());
        }
      } else {
        intervals[currentChangingInterval].updateEnd(value.toInt());
      }
    }
    sendInt(value.toInt());
  });

  webServer.on("/divideLastInterval", HTTP_POST, []() {
    String value = webServer.arg("value");
    divideLastInterval = value.toInt();
    sendInt(value.toInt());
  });

  webServer.on("/brightness", HTTP_POST, []() {
    String value = webServer.arg("value");
    setBrightness(value.toInt());
    sendInt(brightness);
  });

  webServer.on("/autoplay", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplay(value.toInt());
    sendInt(autoplay);
  });

  webServer.on("/autoplayDuration", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplayDuration(value.toInt());
    sendInt(autoplayDuration);
  });

  //list directory
  webServer.on("/list", HTTP_GET, handleFileList);
  //load editor
  webServer.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) webServer.send(404, "text/plain", "FileNotFound");
  });
  //create file
  webServer.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  webServer.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  webServer.on("/edit", HTTP_POST, []() {
        webServer.send(200, "text/plain", "");
      }, handleFileUpload);

  webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");

  webServer.begin();
  Serial.println("HTTP web server started");

  //  webSocketsServer.begin();
  //  webSocketsServer.onEvent(webSocketEvent);
  //  Serial.println("Web socket server started");

  autoPlayTimeout = millis() + (autoplayDuration * 1000);
}

void sendInt(uint8_t value)
{
  sendString(String(value));
}

void sendString(String value)
{
  webServer.send(200, "text/plain", value);
}

void broadcastInt(String name, uint8_t value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":" + String(value) + "}";
  //  webSocketsServer.broadcastTXT(json);
}

void broadcastString(String name, String value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":\"" + String(value) + "\"}";
  //  webSocketsServer.broadcastTXT(json);
}

void loop() {
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(65535));
  //ESP.wdtFeed();
  //  dnsServer.processNextRequest();
  //  webSocketsServer.loop();
  webServer.handleClient();

  //  timeClient.update();

  static bool hasConnected = false;
  EVERY_N_SECONDS(1) {
    //Serial.println(WiFi.status());
    if (WiFi.status() != WL_CONNECTED) {
      //      Serial.printf("Connecting to %s\n", ssid);
      hasConnected = false;
    }
    else if (!hasConnected) {
      hasConnected = true;
      Serial.print("Connected! Open http://");
      Serial.print(WiFi.localIP());
      Serial.println(" in your browser");
    }
  }
 // checkPingTimer();

  //  handleIrInput();
  if (power == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    yield();
    delay(1000 / FRAMES_PER_SECOND);
    return;
    //yield();
  }

  // EVERY_N_SECONDS(10) {
  //   Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  // }
  // change to a new cpt-city gradient palette
  EVERY_N_SECONDS( secondsPerPalette ) {
    //gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    //gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
    if(fullStrip.autoplay) {
      fullStrip.setPaletteIndex(addmod8( fullStrip.getPaletteIndex(), 1, gGradientPaletteCount));
      fullStrip.gTargetPalette = palettes[ fullStrip.getPaletteIndex() ];
    }
    for(int i = 0; i < intervalsAmount; i++) {
      if(intervals[i].autoplay) {
        intervals[i].setPaletteIndex(addmod8(intervals[i].getPaletteIndex(), 1, gGradientPaletteCount));
        intervals[i].gTargetPalette = palettes[ intervals[i].getPaletteIndex() ];
      }
    }
  }

  EVERY_N_MILLISECONDS(40) {
    // slowly blend the current palette to the next
    //nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 8);
    if(fullStrip.autoplay) {
      nblendPaletteTowardPalette( fullStrip.gCurrentPalette, fullStrip.gTargetPalette, 8);
    }
    for(int i = 0; i < intervalsAmount; i++) {
      if(intervals[i].autoplay) {
        nblendPaletteTowardPalette( intervals[i].gCurrentPalette, intervals[i].gTargetPalette, 8);
      }
    }
    gHue++;  // slowly cycle the "base color" through the rainbow
  }
  // Call the current pattern function once, updating the 'leds' array
  //Serial.println("isLoaded?");
  onPacket();
  if(loaded) {
    //Serial.println("YES");
    if(changeFullStrip) {
      if (fullStrip.autoplay && (millis() > fullStrip.autoPlayTimeout)) {
        adjustPattern(&fullStrip, true);
        fullStrip.autoPlayTimeout = millis() + (fullStrip.autoplayDuration * 1000);
      }
      //Serial.println(patterns[fullStrip.getPatternIndex()].name);
      patterns[fullStrip.getPatternIndex()].pattern(&(fullStrip));
      for(int i = 0; i < fullStrip.NUMLEDS; i++) {
        int ir = fullStrip.getPosition(i);
        leds[ir].fadeLightBy((255-(fullStrip.brightness)));
      }
    } else {
      for(int i = 0; i < intervalsAmount; i++) {
        if (intervals[i].autoplay && (millis() > intervals[i].autoPlayTimeout)) {
          adjustPattern(&(intervals[i]), true);
          intervals[i].autoPlayTimeout = millis() + (intervals[i].autoplayDuration * 1000);
        }
        patterns[intervals[i].getPatternIndex()].pattern(&(intervals[i]));
        for(int j = 0; j < intervals[i].NUMLEDS; j++) {
          int ir = intervals[i].getPosition(j);
          leds[ir].fadeLightBy((255-(intervals[i].brightness)));
        }
      }
    }
    //Serial.println("SET LED");
  }
  //patterns[currentPatternIndex].pattern();

  //FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

//void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
//
//  switch (type) {
//    case WStype_DISCONNECTED:
//      Serial.printf("[%u] Disconnected!\n", num);
//      break;
//
//    case WStype_CONNECTED:
//      {
//        IPAddress ip = webSocketsServer.remoteIP(num);
//        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
//
//        // send message to client
//        // webSocketsServer.sendTXT(num, "Connected");
//      }
//      break;
//
//    case WStype_TEXT:
//      Serial.printf("[%u] get Text: %s\n", num, payload);
//
//      // send message to client
//      // webSocketsServer.sendTXT(num, "message here");
//
//      // send data to all connected clients
//      // webSocketsServer.broadcastTXT("message here");
//      break;
//
//    case WStype_BIN:
//      Serial.printf("[%u] get binary length: %u\n", num, length);
//      hexdump(payload, length);
//
//      // send message to client
//      // webSocketsServer.sendBIN(num, payload, lenght);
//      break;
//  }
//}

//void handleIrInput()
//{
//  InputCommand command = readCommand();
//
//  if (command != InputCommand::None) {
//    Serial.print("command: ");
//    Serial.println((int) command);
//  }
//
//  switch (command) {
//    case InputCommand::Up: {
//        adjustPattern(true);
//        break;
//      }
//    case InputCommand::Down: {
//        adjustPattern(false);
//        break;
//      }
//    case InputCommand::Power: {
//        setPower(power == 0 ? 1 : 0);
//        break;
//      }
//    case InputCommand::BrightnessUp: {
//        adjustBrightness(true);
//        break;
//      }
//    case InputCommand::BrightnessDown: {
//        adjustBrightness(false);
//        break;
//      }
//    case InputCommand::PlayMode: { // toggle pause/play
//        setAutoplay(!autoplay);
//        break;
//      }
//
//    // pattern buttons
//
//    case InputCommand::Pattern1: {
//        setPattern(0);
//        break;
//      }
//    case InputCommand::Pattern2: {
//        setPattern(1);
//        break;
//      }
//    case InputCommand::Pattern3: {
//        setPattern(2);
//        break;
//      }
//    case InputCommand::Pattern4: {
//        setPattern(3);
//        break;
//      }
//    case InputCommand::Pattern5: {
//        setPattern(4);
//        break;
//      }
//    case InputCommand::Pattern6: {
//        setPattern(5);
//        break;
//      }
//    case InputCommand::Pattern7: {
//        setPattern(6);
//        break;
//      }
//    case InputCommand::Pattern8: {
//        setPattern(7);
//        break;
//      }
//    case InputCommand::Pattern9: {
//        setPattern(8);
//        break;
//      }
//    case InputCommand::Pattern10: {
//        setPattern(9);
//        break;
//      }
//    case InputCommand::Pattern11: {
//        setPattern(10);
//        break;
//      }
//    case InputCommand::Pattern12: {
//        setPattern(11);
//        break;
//      }
//
//    // custom color adjustment buttons
//
//    case InputCommand::RedUp: {
//        solidColor.red += 8;
//        setSolidColor(solidColor);
//        break;
//      }
//    case InputCommand::RedDown: {
//        solidColor.red -= 8;
//        setSolidColor(solidColor);
//        break;
//      }
//    case InputCommand::GreenUp: {
//        solidColor.green += 8;
//        setSolidColor(solidColor);
//        break;
//      }
//    case InputCommand::GreenDown: {
//        solidColor.green -= 8;
//        setSolidColor(solidColor);
//        break;
//      }
//    case InputCommand::BlueUp: {
//        solidColor.blue += 8;
//        setSolidColor(solidColor);
//        break;
//      }
//    case InputCommand::BlueDown: {
//        solidColor.blue -= 8;
//        setSolidColor(solidColor);
//        break;
//      }
//
//    // color buttons
//
//    case InputCommand::Red: {
//        setSolidColor(CRGB::Red);
//        break;
//      }
//    case InputCommand::RedOrange: {
//        setSolidColor(CRGB::OrangeRed);
//        break;
//      }
//    case InputCommand::Orange: {
//        setSolidColor(CRGB::Orange);
//        break;
//      }
//    case InputCommand::YellowOrange: {
//        setSolidColor(CRGB::Goldenrod);
//        break;
//      }
//    case InputCommand::Yellow: {
//        setSolidColor(CRGB::Yellow);
//        break;
//      }
//
//    case InputCommand::Green: {
//        setSolidColor(CRGB::Green);
//        break;
//      }
//    case InputCommand::Lime: {
//        setSolidColor(CRGB::Lime);
//        break;
//      }
//    case InputCommand::Aqua: {
//        setSolidColor(CRGB::Aqua);
//        break;
//      }
//    case InputCommand::Teal: {
//        setSolidColor(CRGB::Teal);
//        break;
//      }
//    case InputCommand::Navy: {
//        setSolidColor(CRGB::Navy);
//        break;
//      }
//
//    case InputCommand::Blue: {
//        setSolidColor(CRGB::Blue);
//        break;
//      }
//    case InputCommand::RoyalBlue: {
//        setSolidColor(CRGB::RoyalBlue);
//        break;
//      }
//    case InputCommand::Purple: {
//        setSolidColor(CRGB::Purple);
//        break;
//      }
//    case InputCommand::Indigo: {
//        setSolidColor(CRGB::Indigo);
//        break;
//      }
//    case InputCommand::Magenta: {
//        setSolidColor(CRGB::Magenta);
//        break;
//      }
//
//    case InputCommand::White: {
//        setSolidColor(CRGB::White);
//        break;
//      }
//    case InputCommand::Pink: {
//        setSolidColor(CRGB::Pink);
//        break;
//      }
//    case InputCommand::LightPink: {
//        setSolidColor(CRGB::LightPink);
//        break;
//      }
//    case InputCommand::BabyBlue: {
//        setSolidColor(CRGB::CornflowerBlue);
//        break;
//      }
//    case InputCommand::LightBlue: {
//        setSolidColor(CRGB::LightBlue);
//        break;
//      }
//  }
//}

void onPacket() {
  // Read data over socket
  int packetSize = port.parsePacket();
  // If packets have been received, interpret the command
  if (packetSize) {
    int len = port.read(packetBuffer, 1024);
    soundReactiveValue = (uint8_t) packetBuffer[0];
    packetBuffer[len] = 0;
  }
}

void loadSettings()
{
  loaded = false;
  currentPatternIndex = EEPROM.read(1);
  if (currentPatternIndex < 0)
    currentPatternIndex = 0;
  else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

  currentPaletteIndex = EEPROM.read(8);
  if (currentPaletteIndex < 0)
    currentPaletteIndex = 0;
  else if (currentPaletteIndex >= paletteCount)
    currentPaletteIndex = paletteCount - 1;
  Serial.println();
  Serial.println("CREATING FULL STRIP");
  fullStrip = LedInterval(0, NUM_LEDS, NUM_LEDS, 0, 0);
  fullStrip.set_gTargetPalette(gGradientPalettes[0]);
  Serial.println("CREATED FULL STRIP");
  brightness = EEPROM.read(0);
  changeFullStrip = true;
  

  byte r = EEPROM.read(2);
  byte g = EEPROM.read(3);
  byte b = EEPROM.read(4);

  if (r == 0 && g == 0 && b == 0)
  {
  }
  else
  {
    solidColor = CRGB(r, g, b);
  }

  power = EEPROM.read(5);

  autoplay = EEPROM.read(6);
  autoplayDuration = EEPROM.read(7);
  addInterval();
  loaded = true;
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;

  EEPROM.write(5, power);
  EEPROM.commit();

  broadcastInt("power", power);
}

void setAutoplay(uint8_t value)
{
  uint8_t autoplay_ = value == 0 ? 0 : 1;
  if(changeFullStrip) {
    fullStrip.autoplay = autoplay_;
  } else{
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].autoplay = autoplay_;
      }
    } else {
      intervals[currentChangingInterval].autoplay = autoplay_;
    }
  }
  autoplay = autoplay_;

  EEPROM.write(6, autoplay_);
  EEPROM.commit();

  broadcastInt("autoplay", autoplay);
}

void setAutoplayDuration(uint8_t value)
{
  long autoPlayTimeout_ = millis() + (value * 1000);
  if(changeFullStrip) {
    fullStrip.autoplayDuration = value;
    fullStrip.autoPlayTimeout = autoPlayTimeout_;
  } else{
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].autoplayDuration = value;
        intervals[i].autoPlayTimeout = autoPlayTimeout_;
      }
    } else {
      intervals[currentChangingInterval].autoplayDuration = value;
      intervals[currentChangingInterval].autoPlayTimeout = autoPlayTimeout_;
    }
  }
  autoplayDuration = value;

  EEPROM.write(7, autoplayDuration);
  EEPROM.commit();

  

  broadcastInt("autoplayDuration", autoplayDuration);
}

void setSolidColor(CRGB color)
{
  setSolidColor(color.r, color.g, color.b);
}
void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
  solidColor = CRGB(r, g, b);
  if(changeFullStrip) {
    fullStrip.solidColor = solidColor;
    if(fullStrip.useSolidColor)
      fullStrip.gCurrentPalette = CRGBPalette16(fullStrip.solidColor);
  } else{
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].solidColor = solidColor;
        if(intervals[i].useSolidColor)
          intervals[i].gCurrentPalette = CRGBPalette16(intervals[i].solidColor);
      }
    } else {
      intervals[currentChangingInterval].solidColor = solidColor;
      if(intervals[currentChangingInterval].useSolidColor)
       intervals[currentChangingInterval].gCurrentPalette = CRGBPalette16(intervals[currentChangingInterval].solidColor);
    }
  }
  EEPROM.write(2, r);
  EEPROM.write(3, g);
  EEPROM.write(4, b);
  EEPROM.commit();

  //setPattern(solidColorPaletteIndex);

  broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
}
void addInterval() {
  Serial.print("Called addInterval: { "); Serial.print(" index: "); Serial.print(intervalsAmount); Serial.println(" }");
  if(intervalsAmount > 0 && divideLastInterval) {
    if(intervals.at(intervalsAmount-1).start < intervals.at(intervalsAmount-1).end) {
      uint16_t size = intervals.at(intervalsAmount-1).NUMLEDS/2;
      uint16_t end = intervals.at(intervalsAmount-1).start + size;
      intervals.at(intervalsAmount-1).updateEnd(end);
    }
  }
  int start = 0;
  int end = NUM_LEDS;
  if(divideLastInterval && intervalsAmount > 0) {
    start = intervals.at(intervalsAmount-1).end;
  }
  ++intervalsAmount;
  LedInterval newLedInterval = LedInterval(start, end, NUM_LEDS);
  newLedInterval.setPaletteIndex(0);
  newLedInterval.gCurrentPalette = palettes[0];
  newLedInterval.set_gTargetPalette(palettes[0]);
  newLedInterval.index = intervalsAmount-1;
  newLedInterval.hasTwoStrips = HAS_TWO_STRIPS;
  intervals.push_back(newLedInterval);
  reloadPattern = true;
  Serial.println("Created interval: { " + newLedInterval.toString() +" }");
  Serial.println("Exiting addInterval: { intervalsAmount: " + String(intervalsAmount) + " }");
}

void deleteInterval() {
  Serial.print("Called deleteInterval: { "); Serial.print(" currentChangingInterval: "); Serial.print(currentChangingInterval); Serial.println(" }");
  if(intervalsAmount > 1) {
    if(!changeFullStrip) {
      intervals.erase(intervals.begin() + currentChangingInterval);
      intervalsAmount--;
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].index = i;
      }
      currentChangingInterval = intervalsAmount-1;
    }
  }
}
void setChangingInterval(uint8_t value){
  if(value >= intervalsAmount) {
    value = intervalsAmount-1;
  }
  currentChangingInterval = value;
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(LedInterval* interval, bool up)
{
  /*interval = &(fullStrip);
  if(changeAllIntervals) {
    for(int i = 0; i < intervalsAmount; i++) {
      adjustPatternIndex(&(intervals[i].patternIndex), up);
    }
  } else {
    uint16_t* currentPattern;
    if(changeFullStrip) {
      currentPattern = &(fullStrip.patternIndex);
    } else {
      interval =  &(intervals[currentChangingInterval]);
      currentPattern = &(intervals[currentChangingInterval].patternIndex);
    }
    currentPatternIndex = adjustPatternIndex(currentPattern, up);
  }*/
  currentPatternIndex = adjustPatternIndex(&(interval->patternIndex), up);
    //intervals[currentChangingInterval].setPatternIndex(currentPatternIndex, false);
  if (interval->autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}
uint16_t adjustPatternIndex(uint16_t* patternIndex, bool up) {
  if (up)
      *(patternIndex)++;
    else
      *(patternIndex)--;

  // wrap around at the ends
  if (*(patternIndex) < 0)
    *(patternIndex) = patternCount - 1;
  if (*(patternIndex) >= patternCount)
    *(patternIndex) = 0;
  return *(patternIndex);
}
void setPattern(uint8_t value)
{
  reloadPattern = true;
  if (value >= patternCount)
    value = patternCount - 1;
  if(changeFullStrip) {
    fullStrip.setPatternIndex(value, true);
  } else {
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].setPatternIndex(value, true);  
      }
    } else {
      intervals[currentChangingInterval].setPatternIndex(value, true);
    }
  }
  currentPatternIndex = value;

  if (autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}

void setPatternName(String name)
{
  for (uint8_t i = 0; i < patternCount; i++) {
    if (patterns[i].name == name) {
      setPattern(i);
      break;
    }
  }
}

void setInvertDirection(bool value)
{
  if(changeFullStrip) {
    fullStrip.inverted = value;
  } else {
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        intervals[i].inverted = value;
      }
    } else {
      intervals[currentChangingInterval].inverted = value;
    }
  }
  reloadPattern = true;
}


void setUseSolidColor(bool value)
{
  if(value) {
    if(changeFullStrip) {
        fullStrip.gCurrentPalette = CRGBPalette16(fullStrip.solidColor);
        fullStrip.useSolidColor = true;
      } else {
        if(changeAllIntervals) {
          for(int i = 0; i < intervalsAmount; i++) {
            intervals[i].gCurrentPalette = CRGBPalette16(intervals[i].solidColor);
            intervals[i].useSolidColor = true;
          }
        } else {
          intervals[currentChangingInterval].useSolidColor = true;
          intervals[currentChangingInterval].gCurrentPalette = CRGBPalette16(intervals[currentChangingInterval].solidColor);
        }
      }
  } else {
    setPalette(NULL);
  }
  
}

void setPalette(uint8_t value)
{
  oldSoundReactiveValue = 0;
  soundReactiveValue = 1;
  if (value != NULL && value >= paletteCount)
    value = paletteCount - 1;
  if(changeFullStrip) {
    if(value == NULL) {
      value = fullStrip.paletteIndex;
    }
    fullStrip.useSolidColor = false;
    fullStrip.setPaletteIndex(value);
    fullStrip.gCurrentPalette = palettes[value];
  } else {
    if(changeAllIntervals) {
      for(int i = 0; i < intervalsAmount; i++) {
        if(value == NULL) {
          value = intervals[i].paletteIndex;
        }
        intervals[i].useSolidColor = false;
        intervals[i].setPaletteIndex(value);
        intervals[i].gCurrentPalette = palettes[value];
      }
    } else {
      if(value == NULL) {
        value = intervals[currentChangingInterval].paletteIndex;
      }
      intervals[currentChangingInterval].useSolidColor = false;
      intervals[currentChangingInterval].setPaletteIndex(value);
      intervals[currentChangingInterval].gCurrentPalette = palettes[value];
    }
  }
  currentPaletteIndex = value;

  broadcastInt("palette", currentPaletteIndex);
}

void setPaletteName(String name)
{
  for (uint8_t i = 0; i < paletteCount; i++) {
    if (paletteNames[i] == name) {
      setPalette(i);
      break;
    }
  }
}

void adjustBrightness(bool up)
{
  if (up && brightnessIndex < brightnessCount - 1)
    brightnessIndex++;
  else if (!up && brightnessIndex > 0)
    brightnessIndex--;

  brightness = brightnessMap[brightnessIndex];

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void setBrightness(uint8_t value)
{
  if (value > 255)
    value = 255;
  else if (value < 0) value = 0;
  if(changeAllIntervals) {
    for(int i = 0; i < intervalsAmount; i++) {
      intervals[i].brightness = value;
      for(int j = 0; j < intervals[i].NUMLEDS; j++) {
        leds[intervals[i].getPosition(j)].maximizeBrightness();
      }
    }
  } else {
    LedInterval* interval;
    if(changeFullStrip) {
      interval = &(fullStrip);
      brightness = value;
    } else {
      interval = &(intervals[currentChangingInterval]);
    }
    for(int i = 0; i < interval->NUMLEDS; i++) {
      leds[interval->getPosition(i)].maximizeBrightness();
    }
    brightness = value;
    interval->brightness = value;
  }
  //FastLED.setBrightness(value);
  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void strandTest(LedInterval* interval)
{
  static uint8_t i = 0;

  EVERY_N_SECONDS(1)
  {
    i++;
    if (i >= NUM_LEDS)
      i = 0;
  }

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  leds[i] = solidColor;
}

void showSolidColor(LedInterval* interval)
{
  fill_solid(leds, interval->NUMLEDS, interval->solidColor, interval->start);
}

// Patterns from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino

void rainbow(LedInterval* interval)
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, interval->NUMLEDS, gHue, 255 / interval->NUMLEDS, interval->start);
}

void rainbowWithGlitter(LedInterval* interval)
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow(interval);
  addGlitter(80, interval);
}

void rainbowSolid(LedInterval* interval)
{
  fill_solid(leds, interval->NUMLEDS, CHSV(gHue, 255, 255));
}

void confetti(LedInterval* interval)
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackByRange(leds, interval->start, interval->NUMLEDS, 10);
  int pos = random16(interval->NUMLEDS);
  // leds[pos] += CHSV( gHue + random8(64), 200, 255);
  leds[interval->getPosition(pos)] += ColorFromPalette(interval->gCurrentPalette, gHue + random8(64));
}

uint16_t getRealPosition(uint16_t pos, uint16_t start) {
  uint16_t _pos = (start+pos)%NUM_LEDS;
  if(HAS_TWO_STRIPS && _pos < 300) {
    _pos = 299 - _pos;
  }
  return _pos;
}

void fill_rainbow(CRGB * pFirstLED, int numToFill,
                  uint8_t initialhue,
                  uint8_t deltahue,
                  int start)
{
    CHSV hsv;
    hsv.hue = initialhue;
    hsv.val = 255;
    hsv.sat = 240;
    for( int i = 0; i < numToFill; i++) {
        pFirstLED[getRealPosition(i, start)] = hsv;
        hsv.hue += deltahue;
    }
}

void fadeToBlackByRange(CRGB* leds, uint16_t start, uint16_t amount, int fadeToBlackBy) {
  for(int i = 0; i < amount; i++) {
    leds[getRealPosition(i, start)].fadeToBlackBy(fadeToBlackBy);
  }
}

void fadeToBlackByRangeWithExceptions(CRGB* leds, int start, int amount, int fadeToBlackBy, bool* doNotApplyTo) {
  for(int i = 0; i < amount; i++) {
    int realPos = getRealPosition(i, start);
    if(!doNotApplyTo[realPos]) {
      leds[realPos].fadeToBlackBy(fadeToBlackBy);
    }
  }
}


void fill_solid(CRGB * leds, int numToFill, const struct CRGB& color, int start) {
  for( int i = 0; i < numToFill; i++) {
        leds[getRealPosition(i, start)] = color;
    }
}

void fill_solid(CRGB * leds, int numToFill, const struct CHSV& color, int start) {
  for( int i = 0; i < numToFill; i++) {
        leds[getRealPosition(i, start)] = color;
    }
}

void sinelon(LedInterval* interval)
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackByRange( leds, interval->start, interval->NUMLEDS, 20);
  int pos = beatsin16(interval->speed, 0, interval->NUMLEDS-1);
  CRGB color = ColorFromPalette(interval->gCurrentPalette, gHue, 255);
  if ( pos < *(interval->prevpos) ) {
    fill_solid( leds, (*(interval->prevpos) - pos) + 1, color, interval->getPositionSameStrip(pos));
  } else {
    fill_solid( leds, (pos - *(interval->prevpos)) + 1, color, interval->getPositionSameStrip(*(interval->prevpos)));
  }
  *(interval->prevpos) = pos;
}

uint16_t easeOutCubic(uint16_t pos, uint16_t amount) {
  amount = amount-1;
  double x = pos*1.0/amount*1.0;
  return floor(amount*(1 - pow(1 - x, 4)));
}

uint16_t easeOutBounce(uint16_t pos, uint16_t amount) {
  amount = amount-1;
  double x = pos*1.0/amount*1.0;
  double n1 = 7.5625;
  double d1 = 2.75;
  double m = 0;
  if (x < 1 / d1) {
      m = n1 * x * x;
  } else if (x < 2 / d1) {
      m = n1 * (x -= 1.5 / d1) * x + 0.75;
  } else if (x < 2.5 / d1) {
      m = n1 * (x -= 2.25 / d1) * x + 0.9375;
  } else {
      m = n1 * (x -= 2.625 / d1) * x + 0.984375;
  }
  return floor(amount*m);
}

void explosions(LedInterval* interval) {

}

void fireworks(LedInterval* interval) {
  random16_add_entropy(random(256));
  uint8_t glareSize = 4;
  uint8_t fadingScale = 255/glareSize;
  uint16_t speedValue = beat16(interval->speed, 0);
  //CRGB color = ColorFromPalette(palettes[interval->getPaletteIndex()], gHue, 255);
  CRGB color = CRGB::White;
  uint16_t amount = interval->NUMLEDS;
  uint16_t middle = interval->start + amount/2;
  if((interval->launching) && speedValue > interval->sLastMillis) {
    amount = amount/2;
    uint16_t pos = scale16(speedValue, amount - 1);
    pos = easeOutCubic(pos, amount);
    uint16_t pos_ = pos+glareSize;
    //Serial.println("lauching fade 0");
    fadeToBlackByRange( leds, interval->start, interval->NUMLEDS, 100);
    //Serial.println("lauching fade 1");
    if(interval->sPseudotime > 0) {
      //Serial.println("skipping");
      interval->sPseudotime = interval->sPseudotime - 1;
    } else if(pos_ < interval->end) {
      //Serial.println("launching");
      //Serial.print("middle:");
      //Serial.println(pos_);
      uint8_t fadingRandom = random8();
      bool fades = 0;
      if(fadingRandom < 50) {
        fades = 1;
      } else {
        fades = 0;
      }
      setColorToPosition(pos_, interval->start, interval->end, amount, false, color, interval->inverted);
      if(fades) {
         interval->sPseudotime = 1;
      }
      for(int i = 0; i < glareSize; i++) {
        pos_ = pos+i;
        //Serial.print(" start:");
        //Serial.print(pos_);
        setColorToPosition(pos_, interval->start, interval->end, amount, false, color, interval->inverted);
        if(pos_ < middle) leds[interval->getPosition(pos_)].fadeToBlackBy(((glareSize-i-1)*fadingScale));
        pos_ = i+glareSize+1+pos;
        //Serial.print(" end:");
        //Serial.println(pos_);
        setColorToPosition(pos_, interval->start, interval->end, amount+glareSize+1, false, color, interval->inverted);
        if(pos_ < middle) leds[interval->getPosition(pos_)].fadeToBlackBy(((1+i)*fadingScale));
      }
    }
  } else {
    //Serial.println("reached to explosion");
    uint8_t amountParticles = interval->twinkleDensity;
    //Serial.println("got density");
    //Serial.println("got palette");
    //fadeToBlackByRange( leds, interval->start, interval->NUMLEDS, 35 + (220 * (amountParticles*1.0/255)));
    fadeToBlackByRange( leds, interval->start, interval->NUMLEDS, 35);
    if(!(interval->launching)) {
      //Serial.println("is not launching launching");
      if(speedValue > interval->sLastMillis) {
        //Serial.println("explosion fade");
        //Serial.println("explosion start");
        for(int i = 0; i < amountParticles; i++) {
          uint16_t direction = interval->pos[i];
          uint16_t distance = interval->helper[i];
          uint16_t pos = scale16(speedValue, distance - 1);
          pos = easeOutCubic(pos, distance);
          color = ColorFromPalette(interval->gCurrentPalette, gHue + scale16by8(pos, 68), 255);
          uint16_t prevpos = interval->prevpos[i];
          uint16_t range = pos-prevpos;
          if(range > interval->sparking) {
            range = interval->sparking;
          }
          if(direction == 0) {
            fill_solid( leds, range, color, interval->getPositionSameStrip(amount/2 + prevpos));
            leds[interval->getPosition(amount/2 + pos)] = color;
          } else {
            fill_solid( leds, range, color, interval->getPositionSameStrip(amount/2 - pos));
            leds[interval->getPosition(amount/2 - pos)] = color;
          }
          if(prevpos != pos) {
            interval->prevpos[i] = pos;
          }
        }
        //Serial.println("explosion end");
      } else {
        interval->launching = true;
      }
    } else {
      //Serial.println("creating new array");
      interval-> launching = false;
      free(interval->pos);
      interval-> pos = new uint16_t[amountParticles]; //pos - stores the actual direction of each particle
      free(interval->helper);
      interval-> helper = new uint16_t[amountParticles]; //helper - stores the travel distance of each particle
      free(interval->prevpos);
      interval-> prevpos = new uint16_t[amountParticles]; //prevpos - stores the previous position of each particle;
      //Serial.println("created new array");
      for(int i = 0; i < amountParticles; i++) {
        interval->pos[i] = i%2 + (i < amountParticles/2 ? 0 : random8(0, 1));
        interval->helper[i] = random16(amount/6, amount/2);
        interval->prevpos[i] = 0;
      }
      //Serial.println("set values to new array");
    }
  }
  interval->sLastMillis = speedValue;
}

void setColorToPosition(uint16_t pos, uint16_t start, uint16_t end, uint16_t amount, bool loop, CRGB& color, bool inverted) {
  uint16 pos_;
  if(inverted){
    pos_ = end - 1 - pos;
  } else {
    pos_ = start + pos;
  }
  if(loop) {
      pos_ = (pos_)%amount;
  } else if(!loop && (pos_ >= end || pos_ >= NUM_LEDS)) {
      Serial.print("OVERFLOW: ");
      Serial.print("pos: ");
      Serial.print(pos);
      Serial.print(" - end: ");
      Serial.println(end);
      return;
  }
  if(HAS_TWO_STRIPS && pos_ < 300) {
    pos_ = 299 - pos_;
  }
  leds[pos_ % NUM_LEDS] = color;
}

void bpm(LedInterval* interval)
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8( interval->speed, 64, 255);
  for ( int i = 0; i < interval->NUMLEDS; i++) {
    leds[interval->getPosition(i)] = ColorFromPalette(interval->gCurrentPalette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle(LedInterval* interval)
{
  static uint8_t    numdots =   4; // Number of dots in use.
  static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
  static uint8_t     hueinc =  255 / numdots - 1; // Incremental change in hue between each dot.
  static uint8_t    thishue =   0; // Starting hue.
  static uint8_t     curhue =   0; // The current hue
  static uint8_t    thissat = 255; // Saturation of the colour.
  static uint8_t thisbright = 255; // How bright should the LED/display be.
  static uint8_t   basebeat =   5; // Higher = faster movement.

 //static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (interval->lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    interval->lastSecond = secondHand;
    switch (secondHand) {
      case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
      case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
      case 20: numdots = 8; basebeat =  3; hueinc =  0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
  curhue = thishue; // Reset the hue values.
  fadeToBlackByRange(leds, interval->start, interval->NUMLEDS, faderate);
  for ( int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[interval->getPosition(beatsin16(basebeat + i + numdots, 0, interval->NUMLEDS))] += CHSV(gHue + curhue, thissat, thisbright);
    curhue += hueinc;
  }
}

void fire(LedInterval* interval)
{
  heatMap(HeatColors_p, true, interval);
}

void water(LedInterval* interval)
{
  heatMap(WaterColors_p, true, interval);
}

void heatMap(LedInterval* interval)
{
  CRGBPalette16 palette = interval->gCurrentPalette;
  palette.entries[0] = CRGB::Black;
  heatMap(palette, true, interval);
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride(LedInterval* interval)
{
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = interval->sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - interval->sLastMillis ;
  interval->sLastMillis  = ms;
  interval->sPseudotime += deltams * msmultiplier;
  interval->sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = interval->sPseudotime;

  for ( uint16_t i = 0 ; i < interval->NUMLEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (interval->NUMLEDS - 1) - pixelnumber;

    nblend( leds[interval->getPosition(pixelnumber)], newcolor, 64);
  }
}

void radialPaletteShift(LedInterval* interval)
{
  for (uint16_t i = 0; i < interval->NUMLEDS; i++) {
    // leds[i] = ColorFromPalette( gCurrentPalette, gHue + sin8(i*16), brightness);
    leds[interval->getPosition(i)] = ColorFromPalette(interval->gCurrentPalette, i + gHue, 255, LINEARBLEND);
  }
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void heatMap(CRGBPalette16 palette, bool up, LedInterval* interval)
{
  fill_solid(leds, interval->NUMLEDS, CRGB::Black, interval->start);

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(256));

  // Array of temperature readings at each simulation cell
  byte* heat = interval->heat;

  byte colorindex;

  // Step 1.  Cool down every cell a little
  for ( uint16_t i = 0; i < interval->NUMLEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((cooling * 10) / interval->NUMLEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( uint16_t k = interval->NUMLEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < sparking ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( uint16_t j = 0; j < interval->NUMLEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    colorindex = scale8(heat[j], 190);

    CRGB color = ColorFromPalette(palette, colorindex);

    if (up) {
      leds[interval->getPosition(j)] = color;
    }
    else {
      leds[interval->getPosition((interval->NUMLEDS - 1) - j)] = color;
    }
  }
}

void addGlitter( uint8_t chanceOfGlitter, LedInterval* interval)
{
  if ( random8() < chanceOfGlitter) {
    leds[ interval->getPosition(random16(interval->NUMLEDS)) ] += CRGB::White;
  }
}


void blink(LedInterval* interval) {
  uint8_t msmultiplier = random8(23, 60);

  uint16_t hue16 = interval->sHue16;//gHue * 256;
  uint16_t hueinc16 = random(200, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - interval->sLastMillis ;
  if(deltams/2 >= 255-speed) {
    interval->sLastMillis = ms;
    interval->sHue16 += deltams * random(0, 1000);
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    CRGB newcolor;
    if(interval->getPaletteIndex() == rainbowPalleteIndex) {
      newcolor = CHSV( hue8, 255, 255);
    } else {
      newcolor = ColorFromPalette(interval->gCurrentPalette, gHue + random8(64));
    }
    for ( uint16_t i = 0 ; i < interval->NUMLEDS; i++) {
      nblend( leds[interval->getPosition(i)], newcolor, 255);
    }
  }
}
///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

uint8_t beatsaw8( accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                  uint32_t timebase = 0, uint8_t phase_offset = 0)
{
  uint8_t beat = beat8( beats_per_minute, timebase);
  uint8_t beatsaw = beat + phase_offset;
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8( beatsaw, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

void colorWaves(LedInterval* interval)
{
  colorwaves( leds, interval);
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, LedInterval* interval)
{
  CRGBPalette16 palette = interval->gCurrentPalette;
  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = interval->sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms -  interval->sLastMillis ;
  interval->sLastMillis  = ms;
  interval->sPseudotime += deltams * msmultiplier;
  interval->sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = interval->sPseudotime;

  for ( uint16_t i = 0 ; i <  interval->NUMLEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (interval->NUMLEDS - 1) - pixelnumber;

    nblend( ledarray[interval->getPosition(pixelnumber)], newcolor, 128);
  }
}

void blendRange( CRGB* ledarray, int start, int amount, CRGB color, fract8 amountOfOverlay, int fadeToBlackBy) {
  for(int i = 0; i <= amount; i++) {
    nblend(ledarray[getRealPosition(i, start)], color, amountOfOverlay);
    ledarray[getRealPosition(i, start)].fadeToBlackBy(fadeToBlackBy);
  }
}

void startEndJoin(LedInterval* interval) {
  if(interval->clear) {
    interval->setDivisions(interval->divisions);
  }
  for(int i = 0; i < interval->divisions; i++) {
    startendjoin(leds, interval->start + interval->amountPerDivision*i, interval, i);
  }
  startendjoin(leds, interval->start + (interval->amountPerDivision)*((interval->divisions)), interval, interval->divisions);
}

void startendjoin( CRGB* ledarray, uint16_t start, LedInterval* interval, uint8_t section) {
  CRGBPalette16 palette = interval->gCurrentPalette;
  uint16_t amountLeds = interval->amountPerDivision;
  int end = start + amountLeds;
  int middle = amountLeds/2;
  static double speedMultiplier = 0.1;
  uint16_t* pos = &(interval->pos[section]);
  uint16_t* prevpos = &(interval->prevpos[section]);
  uint16_t* count = &(interval->helper[section]);
  int actualSpeed = (int)ceil((amountLeds * ((interval->speed*1.0)/(255*1.0)))*speedMultiplier);
  CRGB color1 = ColorFromPalette(palette, gHue + actualSpeed, 255);
  CRGB color2 = ColorFromPalette(palette, gHue + actualSpeed, 255);
  CRGB color3 = ColorFromPalette(palette, gHue + 60 + actualSpeed*6, 255);
  if(*(pos) >= amountLeds) {
    *(pos) = actualSpeed;
    *(prevpos) = 0;
    *(count) += 1;
    *(count) = *(count)%4;
  } else {
    *(prevpos) = *(pos);
    *(pos) += actualSpeed;
  }
  for(int i = 0; i < amountLeds; i++) {
    ledarray[interval->getPosition(start + i)].fadeToBlackBy(20);
  }
  if(*(count) % 2 == 0) {
    bool end0 = false;
    if(*(pos) >= middle) {
      *(pos) = middle;
      end0 = true;
    }
    if(*(count) == 0) {
      blendRange(ledarray, start, *(pos), color1, 255, 0);
      blendRange(ledarray, end - 1 - *(pos), *(pos), color2, 255, 0);
    } else {
      blendRange(ledarray, start, *(pos), color1, 255, 0);
      blendRange(ledarray, end - 1 - *(pos), *(pos), color2, 255, 0);
    }
    if(end0) {
      *(pos) = amountLeds;
    }
  }
  if(*(count) % 2 == 1) {
      bool end0 = false;
      bool end1 = false;
      if(middle + *(pos) < amountLeds) {
        blendRange(ledarray, start + middle, *(pos), color3, 255, 0);
      } else {
        blendRange(ledarray, start + middle, amountLeds - middle, color3, 255, 0);
        end0 = true;
      }
      if(middle - *(pos) >= 0) {
        blendRange(ledarray, start + middle - *(pos), *(pos), color3, 255, 0);
      } else {
        blendRange(ledarray, start,  *(prevpos), color3, 255, 0);
        end1 = true;
      }
      if(end0 && end1) {
        *(pos) = amountLeds;
      }
  }
  
}

void solidPalette(LedInterval* interval) {
  uint8_t beat = beat8( interval->speed, 0);
  CRGB color = ColorFromPalette(interval->gCurrentPalette, beat, 255);
  for(int i = 0; i < interval->NUMLEDS; i++) {
    nblend(leds[interval->getPosition(i)], color, 255);
  }
}
void colorsPalette(LedInterval* interval) {
  uint8_t beat = beat8( interval->speed, 0);
  for(int i = 0; i < interval->NUMLEDS; i++) {
    nblend(leds[interval->getPosition(i)], ColorFromPalette(interval->gCurrentPalette, beat + i, 255), 255);
  }
}


void soundReactiveBars(LedInterval* interval) {
    random16_add_entropy(random(256));
    uint8_t beat = beat8( interval->speed, 0);
    CRGB color = ColorFromPalette(interval->gCurrentPalette, beat, 255);
    double value =  (soundReactiveValue*((interval->sparking*1.0)/255.0))/255;
    int position = int(value * (interval->NUMLEDS));
    for(int i = 0; i < interval->NUMLEDS; i++) {
      CRGB posColor = CRGB::Black;
      if(i < position && value >= interval->cooling) {
        posColor = color;
      }
      nblend(leds[interval->getPosition(i)], posColor, 255);
    }
}


void soundReactiveBrightness(LedInterval* interval) {
    random16_add_entropy(random(256));
    uint8_t beat = beat8( interval->speed, 0);
    CRGB color;
    double value = soundReactiveValue*((interval->sparking*1.0)/255.0);
    for(int i = 0; i < interval->NUMLEDS; i++) {
      if(value < interval->cooling) {
        color = CRGB::Black;
      } else {
        color = ColorFromPalette(interval->gCurrentPalette, beat, int(value));
      }
      nblend(leds[interval->getPosition(i)], color, 255);
    }
}


void soundReactiveBrightnessAndSpeed(LedInterval* interval) {
    random16_add_entropy(random(256));
    double value = soundReactiveValue*((interval->sparking*1.0)/255.0);
    uint8_t beat = beat8((soundReactiveValue * (interval->speed/511.0)), 0);
    CRGB color;
    for(int i = 0; i < interval->NUMLEDS; i++) {
      if(value < interval->cooling) {
        color = CRGB::Black;
      } else {
        color = ColorFromPalette(interval->gCurrentPalette, beat, int(value));
      }
      nblend(leds[interval->getPosition(i)], color, 255);
    }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}
