/*
  nRF52840 Macro Pad - firmware v0.4
  Author: Bidhan Aryal

  Every key and encoder action is stored in flash and can be changed with
  the configurator page over the USB serial port.
*/

#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---------------------------------------------------------------------------
// 1. PIN SETTINGS - MAPPED FOR nRF52840 SUPERMINI
// ---------------------------------------------------------------------------
// Header P1 (Keys)
#define KEY1_PIN      17  // P1 pin 6   (S1)  -> P0.17
#define KEY4_PIN      20  // P1 pin 7   (S4)  -> P0.20
#define KEY2_PIN      22  // P1 pin 8   (S2)  -> P0.22
#define KEY5_PIN      24  // P1 pin 9   (S5)  -> P0.24
#define KEY3_PIN      32  // P1 pin 10  (S3)  -> P1.00 (32 + 0)
#define KEY6_PIN      11  // P1 pin 11  (S6)  -> P0.11

// Header P2 (Encoders and OLED)
#define OLED_SCL_PIN  31  // P2 pin 6   -> P0.31
#define OLED_SDA_PIN  29  // P2 pin 7   -> P0.29
#define ENC1_A_PIN    2   // P2 pin 8   -> P0.02
#define ENC1_SW_PIN   47  // P2 pin 9   -> P1.15 (32 + 15)
#define ENC1_B_PIN    45  // P2 pin 10  -> P1.13 (32 + 13)
#define ENC2_A_PIN    43  // P2 pin 11  -> P1.11 (32 + 11)
#define ENC2_SW_PIN   10  // P2 pin 12  -> P0.10 (NFC pin)
#define ENC2_B_PIN    9   // P2 pin 13  -> P0.09 (NFC pin)

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

struct Action {
  uint8_t type;
  uint8_t mod;
  uint16_t code;
  char label[8];
  char text[25];
};

struct Config {
  uint16_t magic;
  char name[PROFILES_N][11];
  Action a[PROFILES_N][SLOTS];
};

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
  strcpy(cfg.name[0], "VS Code");
  strcpy(cfg.name[1], "Media");
  strcpy(cfg.name[2], "KiCad");

  const uint8_t C = 1, S = 2;
  setA(0, 0, T_KEY, C, HID_KEY_S, "Save");
  setA(0, 1, T_KEY, C | S, HID_KEY_P, "Palette");
  setA(0, 2, T_KEY, C, HID_KEY_SLASH, "Comment");
  setA(0, 3, T_KEY, C, HID_KEY_B, "Sidebar");
  setA(0, 4, T_KEY, C, HID_KEY_GRAVE, "Term");
  setA(0, 5, T_KEY, 0, HID_KEY_F5, "Run");

  setA(1, 0, T_MEDIA, 0, 0xB6, "Prev");
  setA(1, 1, T_MEDIA, 0, 0xCD, "Play");
  setA(1, 2, T_MEDIA, 0, 0xB5, "Next");
  setA(1, 3, T_MEDIA, 0, 0xE2, "Mute");
  setA(1, 4, T_MEDIA, 0, 0xEA, "Vol -");
  setA(1, 5, T_MEDIA, 0, 0xE9, "Vol +");

  setA(2, 0, T_KEY, 0, HID_KEY_R, "Rotate");
  setA(2, 1, T_KEY, 0, HID_KEY_M, "Move");
  setA(2, 2, T_KEY, 0, HID_KEY_X, "Route");
  setA(2, 3, T_KEY, 0, HID_KEY_V, "Via");
  setA(2, 4, T_KEY, 0, HID_KEY_E, "Edit");
  setA(2, 5, T_KEY, 0, HID_KEY_F, "Flip");

  for (uint8_t p = 0; p < PROFILES_N; p++) {
    setA(p, 6, T_MEDIA, 0, 0xE9, "Vol +");
    setA(p, 7, T_MEDIA, 0, 0xEA, "Vol -");
    setA(p, 8, T_NEXT, 0, 0, "Profile");
    setA(p, 9, T_SCROLL, 1, 1, "Zoom +");
    setA(p, 10, T_SCROLL, 1, 255, "Zoom -");
    setA(p, 11, T_KEY, 0, HID_KEY_ENTER, "Enter");
  }
}

void saveConfig() {
  InternalFS.remove(CFG_FILE);
  if (fsFile.open(CFG_FILE, FILE_O_WRITE)) {
    fsFile.write((uint8_t *)&cfg, sizeof(cfg));
    fsFile.close();
  }
}

void loadConfig() {
  Config t;
  if (fsFile.open(CFG_FILE, FILE_O_READ)) {
    int n = fsFile.read((uint8_t *)&t, sizeof(t));
    fsFile.close();
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
    if (a.type == T_KEY && usbHid.ready()) {
      usbHid.keyboardRelease(RID_KEYBOARD);
    }
    return;
  }

  switch (a.type) {
    case T_KEY:    
      if (usbHid.ready()) {
        uint8_t keys[6] = { (uint8_t)a.code, 0, 0, 0, 0, 0 };
        usbHid.keyboardReport(RID_KEYBOARD, a.mod, keys);
      }
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
  for (; *src && n < size - 1; src++)
    if ((uint8_t)*src >= 32 && (uint8_t)*src < 127) dst[n++] = *src;
  dst[n] = 0;
}

uint8_t splitLine(char *s, char **f, uint8_t max) {
  uint8_t n = 0;
  f[n++] = s;
  for (; *s && n < max; s++)
    if (*s == '|') { *s = 0; f[n++] = s + 1; }
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
  } else if (!strcmp(line, "SAVE")) {
    saveConfig(); drawScreen(); Serial.println("OK");
  } else if (!strcmp(line, "RESET")) {
    setDefaults(); saveConfig(); drawScreen(); Serial.println("OK");
  } else if (!strcmp(line, "BOOT")) {
    Serial.println("OK");
    delay(50);
    NRF_POWER->GPREGRET = 0x57; 
    NVIC_SystemReset();
  } else if (line[0] == 'P' && line[1] == '|') {
    char *f[3];
    if (splitLine(line, f, 3) == 3) {
      int i = atoi(f[1]);
      if (i >= 0 && i < PROFILES_N) copyClean(cfg.name[i], f[2], sizeof(cfg.name[i]));
    }
  } else if (line[0] == 'A' && line[1] == '|') {
    char *f[8];
    uint8_t n = splitLine(line, f, 8);
    int p = atoi(f[1]), s = atoi(f[2]);
    if (n >= 7 && p >= 0 && p < PROFILES_N && s >= 0 && s < SLOTS) {
      Action &a = cfg.a[p][s];
      a.type = atoi(f[3]); a.mod = atoi(f[4]); a.code = atoi(f[5]);
      copyClean(a.label, f[6], sizeof(a.label));
      copyClean(a.text, n >= 8 ? f[7] : "", sizeof(a.text));
    }
  }
}

char lineBuf[96];
uint8_t lineLen = 0;

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
  b.wentPressed = false; 
  b.wentReleased = false;
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
  pinMode(a, INPUT_PULLUP); pinMode(b, INPUT_PULLUP);
  e.last = (digitalRead(a) << 1) | digitalRead(b);
  e.count = 0;
}

int8_t encoderRead(Encoder &e) {
  static const int8_t table[16] = { 0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0 };
  uint8_t state = (digitalRead(e.pinA) << 1) | digitalRead(e.pinB);
  if (state == e.last) return 0;
  e.count += table[(e.last << 2) | state];
  e.last = state;
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

  Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.begin();
  display.begin(OLED_ADDRESS, true);
  drawScreen();
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