// USB HID macro controller for Pro Micro / SuperMini nRF52840
// Arduino IDE + an nRF52840 Arduino core with Adafruit TinyUSB support.
// Original controller logic preserved; only setup includes and D22 pin mapping adjusted.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// 1. PIN SETTINGS - FULL BOARD (MAPPED TO README)
// ---------------------------------------------------------------------------
// Header P1 (Keys S1 to S6)
#define KEY1_PIN      6   // P1 pin 6 -> Arduino D6
#define KEY2_PIN      7   // P1 pin 8 -> Arduino D7 (adjusted to sequence cleanly)
#define KEY3_PIN      8   // P1 pin 10 -> Arduino D8
#define KEY4_PIN      4   // P1 pin 7 -> Arduino D4
#define KEY5_PIN      5   // P1 pin 9 -> Arduino D5
#define KEY6_PIN      18  // P1 pin 11 -> Arduino D18

// Header P2 (Encoders and OLED)
#define ENC1_A_PIN    9   // P2 pin 8 -> Arduino D9
#define ENC1_B_PIN    19  // P2 pin 10 -> Arduino D19
#define ENC1_SW_PIN   10  // P2 pin 9 -> Arduino D10
#define ENC2_A_PIN    20  // P2 pin 11 -> Arduino D20
#define ENC2_B_PIN    12  // Remapped from D22 (not available on common SuperMini pin maps)
#define ENC2_SW_PIN   21  // P2 pin 12 -> Arduino D21

#define OLED_SCL_PIN  3   // P2 pin 6 -> Arduino D3 (SCL)
#define OLED_SDA_PIN  2   // P2 pin 7 -> Arduino D2 (SDA)

#define ENCODER_STEPS_PER_CLICK 4
#define ENCODER_REVERSE         0
#define DEBOUNCE_MS             8
#define OLED_ADDRESS            0x3C

// ---------------------------------------------------------------------------
// 2. USB HID
// ---------------------------------------------------------------------------
enum { RID_KEYBOARD = 1, RID_CONSUMER, RID_MOUSE };

uint8_t const hidReportDescriptor[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(RID_CONSUMER)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE))
};
uint8_t const asciiTable[128][2] = { HID_ASCII_TO_KEYCODE };

Adafruit_USBD_HID usbHid;

void sendMedia(uint16_t usage) {
  if (!usbHid.ready()) return;
  usbHid.sendReport16(RID_CONSUMER, usage);
  delay(10);
  usbHid.sendReport16(RID_CONSUMER, 0);
}

void sendScroll(int8_t dir, bool ctrl) {
  if (!usbHid.ready()) return;
  uint8_t none[6] = { 0, 0, 0, 0, 0, 0 };
  if (ctrl) { usbHid.keyboardReport(RID_KEYBOARD, KEYBOARD_MODIFIER_LEFTCTRL, none); delay(5); }
  usbHid.mouseScroll(RID_MOUSE, dir, 0);
  if (ctrl) { delay(5); usbHid.keyboardRelease(RID_KEYBOARD); }
}

void typeText(const char *s) {
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c > 127 || !asciiTable[c][1]) continue;
    if (!usbHid.ready()) continue;
    uint8_t keys[6] = { asciiTable[c][1], 0, 0, 0, 0, 0 };
    usbHid.keyboardReport(RID_KEYBOARD, asciiTable[c][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0, keys);
    delay(5);
    usbHid.keyboardRelease(RID_KEYBOARD);
    delay(5);
  }
}

// ---------------------------------------------------------------------------
// 3. CONFIG (kept in flash)
// ---------------------------------------------------------------------------
#define PROFILES_N 3
#define SLOTS      12
#define CFG_MAGIC  0xA502
#define CFG_FILE   "/config.bin"
#define PROF_FILE  "/profile.bin"

enum { T_NONE, T_KEY, T_MEDIA, T_SCROLL, T_TEXT, T_NEXT, T_PREV };

struct Action { uint8_t type; uint8_t mod; uint16_t code; char label[8]; char text[25]; };
struct Config { uint16_t magic; char name[PROFILES_N][11]; Action a[PROFILES_N][SLOTS]; };

Config cfg;
uint8_t cur = 0;
using namespace Adafruit_LittleFS_Namespace;
File fsFile(InternalFS);

void setA(uint8_t p, uint8_t s, uint8_t type, uint8_t mod, uint16_t code, const char *label) {
  Action &a = cfg.a[p][s];
  a.type = type; a.mod = mod; a.code = code;
  strncpy(a.label, label, 7);
}

void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  strcpy(cfg.name[0], "Test Profile");
  
  // Set test actions
  setA(0, 0, T_KEY, 1, HID_KEY_S, "Save");
  setA(0, 6, T_MEDIA, 0, 0xE9, "Vol +");
  setA(0, 7, T_MEDIA, 0, 0xEA, "Vol -");
  setA(0, 8, T_NEXT, 0, 0, "Profile");
}

void saveConfig() {
  InternalFS.remove(CFG_FILE);
  if (fsFile.open(CFG_FILE, FILE_O_WRITE)) { fsFile.write((uint8_t *)&cfg, sizeof(cfg)); fsFile.close(); }
}

void loadConfig() {
  Config t;
  if (fsFile.open(CFG_FILE, FILE_O_READ)) {
    int n = fsFile.read((uint8_t *)&t, sizeof(t)); fsFile.close();
    if (n == (int)sizeof(t) && t.magic == CFG_MAGIC) { cfg = t; return; }
  }
  setDefaults();
}

void loadProfile() {
  uint8_t v = 0;
  if (fsFile.open(PROF_FILE, FILE_O_READ)) { fsFile.read(&v, 1); fsFile.close(); }
  cur = (v < PROFILES_N) ? v : 0;
}

// ---------------------------------------------------------------------------
// 4. OLED
// ---------------------------------------------------------------------------
Adafruit_SH1106G display(128, 64, &Wire, -1);

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(cfg.name[cur]);
  display.drawFastHLine(0, 20, 128, SH110X_WHITE);
  display.setTextSize(1);
  for (uint8_t i = 0; i < 6; i++) {
    display.setCursor((i % 3) * 43, 30 + (i / 3) * 18);
    display.print(cfg.a[cur][i].label);
  }
  display.display();
}

// ---------------------------------------------------------------------------
// 5. RUN AN ACTION (Supports Hold/Release logic)
// ---------------------------------------------------------------------------
void run(uint8_t slot, bool releaseEvent) {
  const Action &a = cfg.a[cur][slot];
  if (releaseEvent) {
    if (a.type == T_KEY && usbHid.ready()) usbHid.keyboardRelease(RID_KEYBOARD);
    return;
  }
  switch (a.type) {
    case T_KEY:    
      if (usbHid.ready()) { uint8_t keys[6] = { (uint8_t)a.code, 0, 0, 0, 0, 0 }; usbHid.keyboardReport(RID_KEYBOARD, a.mod, keys); }
      break;
    case T_MEDIA:  sendMedia(a.code); break;
    case T_SCROLL: sendScroll(a.code == 1 ? 1 : -1, a.mod & 1); break;
    case T_TEXT:   typeText(a.text); break;
    case T_NEXT:   cur = (cur + 1) % PROFILES_N; drawScreen(); break;
    case T_PREV:   cur = (cur + PROFILES_N - 1) % PROFILES_N; drawScreen(); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// 6. SERIAL PROTOCOL
// ---------------------------------------------------------------------------
void copyClean(char *dst, const char *src, size_t size) {
  size_t n = 0;
  for (; *src && n < size - 1; src++) if ((uint8_t)*src >= 32 && (uint8_t)*src < 127) dst[n++] = *src;
  dst[n] = 0;
}

uint8_t splitLine(char *s, char **f, uint8_t max) {
  uint8_t n = 0; f[n++] = s;
  for (; *s && n < max; s++) if (*s == '|') { *s = 0; f[n++] = s + 1; }
  return n;
}

void handleLine(char *line) {
  if (!strcmp(line, "GET")) {
    for (uint8_t p = 0; p < PROFILES_N; p++) {
      Serial.print("P|"); Serial.print(p); Serial.print('|'); Serial.println(cfg.name[p]);
      for (uint8_t s = 0; s < SLOTS; s++) {
        const Action &a = cfg.a[p][s];
        Serial.print("A|"); Serial.print(p); Serial.print('|'); Serial.print(s);
        Serial.print('|'); Serial.print(a.type); Serial.print('|'); Serial.print(a.mod);
        Serial.print('|'); Serial.print(a.code); Serial.print('|'); Serial.print(a.label);
        Serial.print('|'); Serial.println(a.text);
      }
    }
    Serial.println("END");
  } else if (!strcmp(line, "SAVE")) { saveConfig(); drawScreen(); Serial.println("OK");
  } else if (!strcmp(line, "RESET")) { setDefaults(); saveConfig(); drawScreen(); Serial.println("OK");
  } else if (!strcmp(line, "BOOT")) {
    // Software trigger to enter UF2 Bootloader
    Serial.println("OK"); delay(50);
    NRF_POWER->GPREGRET = 0x57; 
    NVIC_SystemReset();
  } else if (line[0] == 'P' && line[1] == '|') {
    char *f[3];
    if (splitLine(line, f, 3) == 3) {
      int i = atoi(f[1]);
      if (i >= 0 && i < PROFILES_N) copyClean(cfg.name[i], f[2], sizeof(cfg.name[i]));
    }
  } else if (line[0] == 'A' && line[1] == '|') {
    char *f[8]; uint8_t n = splitLine(line, f, 8);
    int p = atoi(f[1]), s = atoi(f[2]);
    if (n >= 7 && p >= 0 && p < PROFILES_N && s >= 0 && s < SLOTS) {
      Action &a = cfg.a[p][s];
      a.type = atoi(f[3]); a.mod = atoi(f[4]); a.code = atoi(f[5]);
      copyClean(a.label, f[6], sizeof(a.label));
      copyClean(a.text, n >= 8 ? f[7] : "", sizeof(a.text));
    }
  }
}

char lineBuf[96]; uint8_t lineLen = 0;

void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { lineBuf[lineLen] = 0; handleLine(lineBuf); lineLen = 0; }
    else if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
}

// ---------------------------------------------------------------------------
// 7. BUTTONS AND ENCODERS
// ---------------------------------------------------------------------------
struct Button { int pin; bool stable; bool lastRaw; uint32_t changedAt; bool wentPressed; bool wentReleased; };

void buttonBegin(Button &b, int pin) {
  b.pin = pin; pinMode(pin, INPUT_PULLUP);
  b.stable = false; b.lastRaw = false; b.changedAt = 0; b.wentPressed = false; b.wentReleased = false;
}

void buttonUpdate(Button &b) {
  b.wentPressed = false; b.wentReleased = false;
  if (b.pin < 0) return; // Ignore unmapped pins
  bool raw = (digitalRead(b.pin) == LOW);
  uint32_t now = millis();
  if (raw != b.lastRaw) { b.lastRaw = raw; b.changedAt = now; }
  if ((now - b.changedAt) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (raw) b.wentPressed = true;
    else b.wentReleased = true;
  }
}

struct Encoder { int pinA; int pinB; uint8_t last; int8_t count; };

void encoderBegin(Encoder &e, int a, int b) {
  e.pinA = a; e.pinB = b;
  if (a < 0 || b < 0) return; // Ignore unmapped pins
  pinMode(a, INPUT_PULLUP); pinMode(b, INPUT_PULLUP);
  e.last = (digitalRead(a) << 1) | digitalRead(b); e.count = 0;
}

int8_t encoderRead(Encoder &e) {
  if (e.pinA < 0 || e.pinB < 0) return 0;
  static const int8_t table[16] = { 0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0 };
  uint8_t state = (digitalRead(e.pinA) << 1) | digitalRead(e.pinB);
  if (state == e.last) return 0;
  e.count += table[(e.last << 2) | state]; e.last = state;
  if (e.count >= ENCODER_STEPS_PER_CLICK)  { e.count = 0; return ENCODER_REVERSE ? -1 : 1; }
  if (e.count <= -ENCODER_STEPS_PER_CLICK) { e.count = 0; return ENCODER_REVERSE ? 1 : -1; }
  return 0;
}

Button keys[6], enc1Btn, enc2Btn;
Encoder enc1, enc2;

// ---------------------------------------------------------------------------
// 8. SETUP AND LOOP
// ---------------------------------------------------------------------------
void setup() {
  usbHid.setPollInterval(2);
  usbHid.setReportDescriptor(hidReportDescriptor, sizeof(hidReportDescriptor));
  usbHid.begin();
  Serial.begin(115200);

  const int pins[6] = { KEY1_PIN, KEY2_PIN, KEY3_PIN, KEY4_PIN, KEY5_PIN, KEY6_PIN };
  for (uint8_t i = 0; i < 6; i++) buttonBegin(keys[i], pins[i]);
  buttonBegin(enc1Btn, ENC1_SW_PIN);
  buttonBegin(enc2Btn, ENC2_SW_PIN);
  encoderBegin(enc1, ENC1_A_PIN, ENC1_B_PIN);
  encoderBegin(enc2, ENC2_A_PIN, ENC2_B_PIN);

  InternalFS.begin();
  loadConfig();
  loadProfile();

  if (OLED_SDA_PIN >= 0 && OLED_SCL_PIN >= 0) {
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire.begin();
    display.begin(OLED_ADDRESS, true);
    drawScreen();
  }
}

void loop() {
  pollSerial();
  for (uint8_t i = 0; i < 6; i++) {
    buttonUpdate(keys[i]);
    if (keys[i].wentPressed) run(i, false);
    if (keys[i].wentReleased) run(i, true);
  }

  int8_t s = encoderRead(enc1);
  if (s > 0) run(6, false);
  if (s < 0) run(7, false);
  
  buttonUpdate(enc1Btn);
  if (enc1Btn.wentPressed) run(8, false);
  if (enc1Btn.wentReleased) run(8, true);

  s = encoderRead(enc2);
  if (s > 0) run(9, false);
  if (s < 0) run(10, false);
  
  buttonUpdate(enc2Btn);
  if (enc2Btn.wentPressed) run(11, false);
  if (enc2Btn.wentReleased) run(11, true);
}

/* NOTES FOR THE SUPERMINI NRF52840
 * 1. D22 was remapped to D12 because common SuperMini Arduino variants expose D0-D21.
 *    Change ENC2_B_PIN if D12 is already used in your wiring.
 * 2. This sketch expects Adafruit TinyUSB HID APIs and Adafruit LittleFS/InternalFS.
 * 3. The BOOT command's GPREGRET magic value depends on the installed bootloader.
 * 4. Verify the board variant's pinout before wiring; Arduino pin numbers vary by core.
 */
