#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

// ── TFT Pins ──────────────────────────────────────────────
#define TFT_CS   1
#define TFT_RST  2
#define TFT_DC   3
#define TFT_SCLK 4
#define TFT_MOSI 5

// ── Button & Potentiometer Pins ───────────────────────────
#define VOL_PIN      34   // Potentiometer (analog)
#define LIKE_BTN     25   // Momentary push button - Like/Unlike
#define SHUFFLE_BTN  26   // Momentary push button - Shuffle on/off

// ── Credentials ───────────────────────────────────────────
char*       SSID          = "Your Wi-fi SSID";
const char* PASSWORD      = "Your Wi-fi password";
const char* CLIENT_ID     = "Client ID";
const char* CLIENT_SECRET = "Client Secret";

// ── State ─────────────────────────────────────────────────
String lastArtist;
String lastTrackname;
int    lastVolume   = -1;
bool   shuffleOn    = false;
bool   liked        = false;

// ── Hardware ──────────────────────────────────────────────
Spotify         sp(CLIENT_ID, CLIENT_SECRET);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);


// ─────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(LIKE_BTN,    INPUT_PULLUP);
    pinMode(SHUFFLE_BTN, INPUT_PULLUP);

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);           // landscape
    tft.fillScreen(ST77XX_BLACK);
    Serial.println("TFT Initialized!");

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.printf("\nConnected!\n");

    tft.setCursor(0, 0);
    tft.write(WiFi.localIP().toString().c_str());

    sp.begin();
    while (!sp.is_auth()) {
        sp.handle_client();
    }
    Serial.println("Authenticated");

    drawStatusBar(); // draw initial shuffle/like icons
}


// ─────────────────────────────────────────────────────────
//  DRAW STATUS BAR  (bottom row of the screen)
//  Layout at y=100:  [Heart Liked/Unliked]   [Shuffle ON/OFF]
// ─────────────────────────────────────────────────────────
void drawStatusBar() {
    // Clear the bar area
    tft.fillRect(0, 100, 160, 28, ST77XX_BLACK);

    // ── Like indicator ──
    tft.setCursor(10, 100);
    if (liked) {
        tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
        tft.print("<3 Liked");
    } else {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.print("   Like");
    }

    // ── Shuffle indicator ──
    tft.setCursor(90, 100);
    if (shuffleOn) {
        tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
        tft.print("Shuf ON");
    } else {
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.print("Shuf OFF");
    }

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); // reset colour
}


// ─────────────────────────────────────────────────────────
//  VOLUME  - potentiometer on VOL_PIN
// ─────────────────────────────────────────────────────────
void handleVolume() {
    int raw    = analogRead(VOL_PIN);
    int volume = map(raw, 0, 4095, 0, 100);

    if (abs(volume - lastVolume) >= 2) {
        sp.set_volume(volume);
        lastVolume = volume;
        Serial.println("Volume: " + String(volume) + "%");

        tft.fillRect(0, 84, 160, 14, ST77XX_BLACK);
        tft.setCursor(10, 84);
        tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        tft.print("Vol: " + String(volume) + "%");
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    }
}


// ─────────────────────────────────────────────────────────
//  LIKE / UNLIKE  - toggle on button press
// ─────────────────────────────────────────────────────────
void handleLike() {
    static bool lastState = HIGH;
    bool currentState = digitalRead(LIKE_BTN);

    if (lastState == HIGH && currentState == LOW) {   // falling edge = press
        liked = !liked;
        if (liked) {
            sp.save_track();          // like / save current track
            Serial.println("Track liked!");
        } else {
            sp.remove_track();        // unlike / remove from library
            Serial.println("Track unliked.");
        }
        drawStatusBar();
        delay(50); // simple debounce
    }
    lastState = currentState;
}


// ─────────────────────────────────────────────────────────
//  SHUFFLE  - toggle on button press
// ─────────────────────────────────────────────────────────
void handleShuffle() {
    static bool lastState = HIGH;
    bool currentState = digitalRead(SHUFFLE_BTN);

    if (lastState == HIGH && currentState == LOW) {   // falling edge = press
        shuffleOn = !shuffleOn;
        sp.toggle_shuffle(shuffleOn);
        Serial.println("Shuffle: " + String(shuffleOn ? "ON" : "OFF"));
        drawStatusBar();
        delay(50); // simple debounce
    }
    lastState = currentState;
}


// ─────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────
void loop() {
    handleVolume();
    handleLike();
    handleShuffle();

    String currentArtist    = sp.current_artist_names();
    String currentTrackname = sp.current_track_name();

    // When the song changes, reset the like state and refresh the screen
    if (lastTrackname != currentTrackname &&
        currentTrackname != "Something went wrong" &&
        currentTrackname != "null") {

        liked = false;   // new song starts unliked (from device perspective)
        tft.fillScreen(ST77XX_BLACK);
        lastTrackname = currentTrackname;
        Serial.println("Track: " + lastTrackname);
        tft.setCursor(10, 40);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.write(lastTrackname.c_str());
        drawStatusBar();
    }

    if (lastArtist != currentArtist &&
        currentArtist != "Something went wrong" &&
        !currentArtist.isEmpty()) {

        lastArtist = currentArtist;
        Serial.println("Artist: " + lastArtist);
        tft.fillRect(0, 10, 160, 14, ST77XX_BLACK);
        tft.setCursor(10, 10);
        tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        tft.write(lastArtist.c_str());
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    }

    delay(2000);
}
