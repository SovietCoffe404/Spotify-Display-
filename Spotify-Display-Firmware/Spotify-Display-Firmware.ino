#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

#define TFT_CS 1
#define TFT_RST 2
#define TFT_DC 3
#define TFT_SCLK 4
#define TFT_MOSI 5
#define VOL_PIN 34       // analog pin connected to potentiometer

char* SSID = "Your Wi-fi SSID";
const char* PASSWORD = "Your Wi-fi password";
const char* CLIENT_ID = "Client ID";
const char* CLIENT_SECRET = "Client Secret";

String lastArtist;
String lastTrackname;
int lastVolume = -1;     // tracks last sent volume to avoid spamming the API

Spotify sp(CLIENT_ID, CLIENT_SECRET);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);


void setup() {
    Serial.begin(115200);

    tft.initR(INITR_BLACKTAB); // the type of screen
    tft.setRotation(1); // this makes the screen landscape! remove this line for portrait
    Serial.println("TFT Initialized!");
    tft.fillScreen(ST77XX_BLACK); // make sure there is nothing in the buffer

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi...");
    while(WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }
    Serial.printf("\nConnected!\n");

    tft.setCursor(0,0); // make the cursor at the top left
    tft.write(WiFi.localIP().toString().c_str()); // print out IP on the screen

    sp.begin();
    while(!sp.is_auth()){
        sp.handle_client();
    }
    Serial.println("Authenticated");
}

void setVolume() {
    int raw = analogRead(VOL_PIN);          // 0-4095 on ESP32
    int volume = map(raw, 0, 4095, 0, 100); // convert to 0-100%

    if (abs(volume - lastVolume) >= 2) {    // deadband of 2% to reduce noise
        sp.set_volume(volume);
        lastVolume = volume;
        Serial.println("Volume set to: " + String(volume) + "%");

        // Display volume on TFT
        tft.fillRect(0, 70, 160, 16, ST77XX_BLACK); // clear previous volume line
        tft.setCursor(10, 70);
        tft.print("Vol: " + String(volume) + "%");
    }
}

void loop()
{
    setVolume();

    String currentArtist = sp.current_artist_names();
    String currentTrackname = sp.current_track_name();

    if (lastArtist != currentArtist && currentArtist != "Something went wrong" && !currentArtist.isEmpty()) {
        tft.fillScreen(ST77XX_BLACK);
        lastArtist = currentArtist;
        Serial.println("Artist: " + lastArtist);
        tft.setCursor(10,10);
        tft.write(lastArtist.c_str());
    }

    if (lastTrackname != currentTrackname && currentTrackname != "Something went wrong" && currentTrackname != "null") {
        lastTrackname = currentTrackname;
        Serial.println("Track: " + lastTrackname);
        tft.setCursor(10,40);
        tft.write(lastTrackname.c_str());
    }
    delay(2000);
}
