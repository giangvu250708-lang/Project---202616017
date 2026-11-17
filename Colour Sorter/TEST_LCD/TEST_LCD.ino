#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ============================================================
// HARDWARE
// ============================================================

#define LCD_ADDR 0x27

#define BUTTON_LEFT  2
#define BUTTON_RIGHT 3
#define BUTTON_OK    4

#define PHOTO_PIN A0

#define TCS_S0  7
#define TCS_S1  8
#define TCS_S2  9
#define TCS_S3  10
#define TCS_OUT 11

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);

// ============================================================
// BUTTON PARAMETERS
// LOW  = RELEASED
// HIGH = PRESSED
// ============================================================

#define DEBOUNCE_MS       30
#define LONG_PRESS_MS     500
#define AUTO_REPEAT_MS    300

// ============================================================
// PRODUCT / COLOR PARAMETERS
// ============================================================

#define PRODUCT_THRESHOLD    200
#define PRODUCT_SAMPLES      3

#define COLOR_STABLE_ERROR   20
#define COLOR_RANGE          30
#define COLOR_SAMPLES        3

#define PULSE_TIMEOUT_US     100000UL

// ============================================================
// COLOR DATABASE
// ============================================================

#define COLOR_COUNT 13

const char* COLOR_NAMES[COLOR_COUNT] = {
  "RED",
  "BLUE",
  "GREEN",
  "WHITE",
  "BLACK",
  "YELLOW",
  "PURPLE",
  "BROWN",
  "PINK",
  "NEW1",
  "NEW2",
  "NEW3",
  "NEW4"
};

// ============================================================
// EEPROM
// ============================================================

// Đổi MAGIC để bản EEPROM cũ của code 14 màu không bị đọc nhầm.
#define EEPROM_MAGIC 0x5634

struct ColorData {
  uint8_t valid;

  uint32_t redMin;
  uint32_t redMax;

  uint32_t blueMin;
  uint32_t blueMax;

  uint32_t greenMin;
  uint32_t greenMax;
};

struct EEPROMData {
  uint16_t magic;

  ColorData colors[COLOR_COUNT];

  // -1 = chưa gán
  // 0..12 = index của màu
  int8_t positionColor[5];
};

EEPROMData db;

// ============================================================
// COLOR SAMPLE
// ============================================================

struct ColorSample {
  uint32_t red;
  uint32_t blue;
  uint32_t green;
};

// ============================================================
// BUTTON STATE
// ============================================================

struct ButtonState {
  bool rawState;
  bool stableState;

  unsigned long rawChangedAt;
  unsigned long pressedAt;
  unsigned long lastRepeatAt;

  bool autoMode;

  bool pressEvent;
  bool repeatEvent;
};

ButtonState btnLeft;
ButtonState btnRight;
ButtonState btnOK;

// ============================================================
// UI STATE
// ============================================================

enum Screen {
  SCREEN_MAIN,
  SCREEN_COLOR_MENU,
  SCREEN_POSITION_MENU,
  SCREEN_POSITION_COLOR_MENU
};

Screen currentScreen = SCREEN_MAIN;

int mainCursor = 0;
int colorCursor = 0;
int positionCursor = 0;

int selectedPosition = -1;
int positionColorCursor = 0;

bool uiDirty = true;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

// ---------- Buttons ----------
void initButton(ButtonState &b, bool initialState);
void updateButton(ButtonState &b, bool rawState);
void updateButtons();

bool buttonLeftEvent();
bool buttonRightEvent();
bool buttonOKEvent();

// ---------- EEPROM ----------
void initializeDatabase();
void saveEEPROM();
void loadEEPROM();

// ---------- LCD ----------
void renderMain();
void renderColorMenu();
void renderPositionMenu();
void renderPositionColorMenu();

void printMenuCell(
  uint8_t row,
  uint8_t col,
  const char* text,
  bool selected
);

void showMessage(
  const char* line1,
  const char* line2,
  unsigned long ms
);

// ---------- Cursor ----------
void moveCursor(
  int &cursor,
  int count,
  int direction
);

// ---------- Product ----------
bool detectProductStable();

// ---------- TCS3200 ----------
void setTCSColorFilter(bool s2, bool s3);
uint32_t readTCSPeriod();

bool readColorSample(ColorSample &sample);
bool measureColorStable(ColorSample &average);

// ---------- Calibration ----------
void calibrateColor(int colorIndex);

bool colorUsedByOtherPosition(
  int colorIndex,
  int exceptPosition
);

int buildPositionCandidates(
  int positionIndex,
  int candidateList[]
);

// ---------- Debug ----------
void printColorData(int index);

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(9600);

  Wire.begin();

  // ---------- LCD ----------
  lcd.init();
  lcd.backlight();

  // ---------- Buttons ----------
  // LOW = released
  // HIGH = pressed
  //
  // Arduino UNO không có INPUT_PULLDOWN.
  // Vì vậy mạch ngoài phải có pull-down resistor.
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  pinMode(BUTTON_OK, INPUT);

  // ---------- Photoresistor ----------
  pinMode(PHOTO_PIN, INPUT);

  // ---------- TCS3200 ----------
  pinMode(TCS_S0, OUTPUT);
  pinMode(TCS_S1, OUTPUT);
  pinMode(TCS_S2, OUTPUT);
  pinMode(TCS_S3, OUTPUT);
  pinMode(TCS_OUT, INPUT);

  // Frequency scaling = 20%
  // S0 = HIGH
  // S1 = LOW
  digitalWrite(TCS_S0, HIGH);
  digitalWrite(TCS_S1, LOW);

  // ---------- EEPROM ----------
  loadEEPROM();

  // ---------- Initial button state ----------
  initButton(
    btnLeft,
    digitalRead(BUTTON_LEFT)
  );

  initButton(
    btnRight,
    digitalRead(BUTTON_RIGHT)
  );

  initButton(
    btnOK,
    digitalRead(BUTTON_OK)
  );

  // ---------- Serial ----------
  Serial.println(F("================================"));
  Serial.println(F(" LCD 2004 COLOR CALIBRATION"));
  Serial.println(F(" Arduino UNO R3"));
  Serial.println(F("================================"));

  Serial.println(F("[SYSTEM] Direct button input"));
  Serial.println(F("[SYSTEM] LEFT  = D2"));
  Serial.println(F("[SYSTEM] RIGHT = D3"));
  Serial.println(F("[SYSTEM] OK    = D4"));

  Serial.println(F("[SYSTEM] Photoresistor = A0"));

  Serial.println(F("[SYSTEM] TCS3200:"));
  Serial.println(F("         S0  = D7"));
  Serial.println(F("         S1  = D8"));
  Serial.println(F("         S2  = D9"));
  Serial.println(F("         S3  = D10"));
  Serial.println(F("         OUT = D11"));

  Serial.println(F("[SYSTEM] Scale = 20%"));
  Serial.println(F("[SYSTEM] Product threshold > 200"));
  Serial.println(F("[SYSTEM] Color stability <= 20"));
  Serial.println(F("[SYSTEM] Final range = AVG +/- 30"));

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("LCD 2004 TEST"));

  lcd.setCursor(0, 1);
  lcd.print(F("SYSTEM READY"));

  delay(800);

  uiDirty = true;
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  updateButtons();

  switch (currentScreen) {

    // ========================================================
    // MAIN MENU
    // ========================================================

    case SCREEN_MAIN:

      if (uiDirty) {
        renderMain();
        uiDirty = false;
      }

      // LEFT
      if (buttonLeftEvent()) {
        moveCursor(
          mainCursor,
          3,
          -1
        );

        uiDirty = true;
      }

      // RIGHT
      if (buttonRightEvent()) {
        moveCursor(
          mainCursor,
          3,
          +1
        );

        uiDirty = true;
      }

      // OK
      if (buttonOKEvent()) {

        Serial.print(F("[MAIN] Selected: "));

        if (mainCursor == 0) {

          Serial.println(F("START"));

          showMessage(
            "START",
            "MAIN CODE NOT READY",
            1000
          );

          currentScreen = SCREEN_MAIN;
          uiDirty = true;
        }

        else if (mainCursor == 1) {

          Serial.println(F("SET NEW COLOUR"));

          currentScreen = SCREEN_COLOR_MENU;
          colorCursor = 0;
          uiDirty = true;
        }

        else {

          Serial.println(F("CALIBRATE POSITION"));

          currentScreen = SCREEN_POSITION_MENU;
          positionCursor = 0;
          uiDirty = true;
        }
      }

      break;

    // ========================================================
    // SET NEW COLOUR
    // ========================================================

    case SCREEN_COLOR_MENU:

      if (uiDirty) {
        renderColorMenu();
        uiDirty = false;
      }

      if (buttonLeftEvent()) {

        moveCursor(
          colorCursor,
          COLOR_COUNT,
          -1
        );

        uiDirty = true;
      }

      if (buttonRightEvent()) {

        moveCursor(
          colorCursor,
          COLOR_COUNT,
          +1
        );

        uiDirty = true;
      }

      if (buttonOKEvent()) {

        Serial.print(F("[COLOR MENU] Selected: "));
        Serial.println(
          COLOR_NAMES[colorCursor]
        );

        calibrateColor(colorCursor);

        // Sau khi calibration xong
        // luôn quay về MAIN MENU.
        currentScreen = SCREEN_MAIN;
        mainCursor = 0;

        uiDirty = true;
      }

      break;

    // ========================================================
    // CALIBRATE POSITION
    // ========================================================

    case SCREEN_POSITION_MENU:

      if (uiDirty) {
        renderPositionMenu();
        uiDirty = false;
      }

      if (buttonLeftEvent()) {

        moveCursor(
          positionCursor,
          6,
          -1
        );

        uiDirty = true;
      }

      if (buttonRightEvent()) {

        moveCursor(
          positionCursor,
          6,
          +1
        );

        uiDirty = true;
      }

      if (buttonOKEvent()) {

        // POS1 -> POS5
        if (positionCursor < 5) {

          selectedPosition = positionCursor;

          int candidates[COLOR_COUNT];

          int count =
            buildPositionCandidates(
              selectedPosition,
              candidates
            );

          if (count == 0) {

            Serial.println(
              F("[POSITION] No available colour.")
            );

            showMessage(
              "NO FREE COLOUR",
              "CALIBRATE FIRST",
              1200
            );

            uiDirty = true;
          }

          else {

            positionColorCursor = 0;

            currentScreen =
              SCREEN_POSITION_COLOR_MENU;

            uiDirty = true;
          }
        }

        // DONE
        else {

          Serial.println(
            F("[POSITION] DONE")
          );

          saveEEPROM();

          showMessage(
            "POSITION SAVED",
            "RETURN MAIN MENU",
            800
          );

          currentScreen = SCREEN_MAIN;
          mainCursor = 0;

          uiDirty = true;
        }
      }

      break;

    // ========================================================
    // SELECT COLOUR FOR POSITION
    // ========================================================

    case SCREEN_POSITION_COLOR_MENU:
    {

      if (uiDirty) {
        renderPositionColorMenu();
        uiDirty = false;
      }

      int candidates[COLOR_COUNT];

      int count =
        buildPositionCandidates(
          selectedPosition,
          candidates
        );

      if (count == 0) {
        currentScreen = SCREEN_POSITION_MENU;
        uiDirty = true;
        break;
      }

      if (buttonLeftEvent()) {

        moveCursor(
          positionColorCursor,
          count,
          -1
        );

        uiDirty = true;
      }

      if (buttonRightEvent()) {

        moveCursor(
          positionColorCursor,
          count,
          +1
        );

        uiDirty = true;
      }

      if (buttonOKEvent()) {

        int colorIndex =
          candidates[positionColorCursor];

        db.positionColor[selectedPosition] =
          colorIndex;

        Serial.print(F("[POSITION] POS"));
        Serial.print(selectedPosition + 1);
        Serial.print(F(" <- "));
        Serial.println(
          COLOR_NAMES[colorIndex]
        );

        saveEEPROM();

        showMessage(
          "POSITION ASSIGNED",
          COLOR_NAMES[colorIndex],
          800
        );

        // Quay lại position menu
        currentScreen = SCREEN_POSITION_MENU;

        // Tự động đưa cursor sang position kế tiếp
        if (selectedPosition < 4) {
          positionCursor =
            selectedPosition + 1;
        }
        else {
          positionCursor = 5; // DONE
        }

        uiDirty = true;
      }

      break;
    }
  }
}

// ============================================================
// BUTTON INITIALIZATION
// ============================================================

void initButton(
  ButtonState &b,
  bool initialState
) {

  b.rawState = initialState;
  b.stableState = initialState;

  b.rawChangedAt = millis();
  b.pressedAt = 0;
  b.lastRepeatAt = 0;

  b.autoMode = false;

  b.pressEvent = false;
  b.repeatEvent = false;
}

// ============================================================
// BUTTON UPDATE
// ============================================================

void updateButton(
  ButtonState &b,
  bool rawState
) {

  unsigned long now = millis();

  // Reset event mỗi vòng loop.
  b.pressEvent = false;
  b.repeatEvent = false;

  // ----------------------------------------------------------
  // RAW STATE CHANGED
  // ----------------------------------------------------------

  if (rawState != b.rawState) {

    b.rawState = rawState;
    b.rawChangedAt = now;
  }

  // ----------------------------------------------------------
  // DEBOUNCE
  // ----------------------------------------------------------

  if (
    (now - b.rawChangedAt) >= DEBOUNCE_MS &&
    b.stableState != b.rawState
  ) {

    b.stableState = b.rawState;

    // LOW -> HIGH
    // button vừa được nhấn
    if (b.stableState == HIGH) {

      b.pressedAt = now;
      b.lastRepeatAt = now;

      b.autoMode = false;

      b.pressEvent = true;
    }

    // HIGH -> LOW
    // button vừa được thả
    else {

      b.autoMode = false;
    }
  }

  // ----------------------------------------------------------
  // LONG PRESS
  // ----------------------------------------------------------

  if (b.stableState == HIGH) {

    if (
      !b.autoMode &&
      (now - b.pressedAt) >= LONG_PRESS_MS
    ) {

      b.autoMode = true;
      b.lastRepeatAt = now;

      Serial.println(
        F("[BUTTON] AUTO MODE")
      );
    }

    // --------------------------------------------------------
    // AUTO REPEAT
    // --------------------------------------------------------

    if (
      b.autoMode &&
      (now - b.lastRepeatAt) >= AUTO_REPEAT_MS
    ) {

      b.lastRepeatAt = now;

      b.repeatEvent = true;
    }
  }
}

// ============================================================
// READ ALL BUTTONS
// ============================================================

void updateButtons() {

  bool leftState =
    digitalRead(BUTTON_LEFT);

  bool rightState =
    digitalRead(BUTTON_RIGHT);

  bool okState =
    digitalRead(BUTTON_OK);

  updateButton(
    btnLeft,
    leftState
  );

  updateButton(
    btnRight,
    rightState
  );

  updateButton(
    btnOK,
    okState
  );
}

// ============================================================
// BUTTON EVENTS
// ============================================================

bool buttonLeftEvent() {
  return
    btnLeft.pressEvent ||
    btnLeft.repeatEvent;
}

bool buttonRightEvent() {
  return
    btnRight.pressEvent ||
    btnRight.repeatEvent;
}

bool buttonOKEvent() {
  return btnOK.pressEvent;
}

// ============================================================
// EEPROM INITIALIZATION
// ============================================================

void initializeDatabase() {

  memset(
    &db,
    0,
    sizeof(db)
  );

  db.magic = EEPROM_MAGIC;

  // Tất cả màu chưa calibration
  for (int i = 0; i < COLOR_COUNT; i++) {
    db.colors[i].valid = 0;
  }

  // Tất cả position chưa gán
  for (int i = 0; i < 5; i++) {
    db.positionColor[i] = -1;
  }
}

// ============================================================
// SAVE EEPROM
// ============================================================

void saveEEPROM() {

  EEPROM.put(
    0,
    db
  );

  Serial.println(
    F("[EEPROM] Data saved.")
  );
}

// ============================================================
// LOAD EEPROM
// ============================================================

void loadEEPROM() {

  EEPROM.get(
    0,
    db
  );

  if (db.magic != EEPROM_MAGIC) {

    Serial.println(
      F("[EEPROM] Invalid data.")
    );

    initializeDatabase();

    saveEEPROM();
  }

  else {

    Serial.println(
      F("[EEPROM] Valid data loaded.")
    );
  }
}

// ============================================================
// MAIN MENU DISPLAY
// ============================================================

void renderMain() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("MAIN MENU"));

  const char* items[3] = {
    "START",
    "SET NEW COLOUR",
    "CALIBRATE POSITION"
  };

  for (int i = 0; i < 3; i++) {

    lcd.setCursor(0, i + 1);

    if (i == mainCursor) {
      lcd.print('>');
    }
    else {
      lcd.print(' ');
    }

    lcd.print(items[i]);
  }
}

// ============================================================
// PRINT ONE MENU CELL
// ============================================================

void printMenuCell(
  uint8_t row,
  uint8_t col,
  const char* text,
  bool selected
) {

  // Mỗi cell rộng 10 ký tự.
  uint8_t start =
    col * 10;

  lcd.setCursor(
    start,
    row
  );

  if (selected) {
    lcd.print('>');
  }
  else {
    lcd.print(' ');
  }

  uint8_t len =
    strlen(text);

  lcd.print(text);

  while (len < 9) {

    lcd.print(' ');
    len++;
  }
}

// ============================================================
// COLOUR MENU
// ============================================================

void renderColorMenu() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("SET COLOUR NAME"));

  // 6 màu / page
  int page =
    colorCursor / 6;

  int first =
    page * 6;

  for (int slot = 0; slot < 6; slot++) {

    int index =
      first + slot;

    if (index >= COLOR_COUNT) {
      break;
    }

    uint8_t row =
      1 + slot / 2;

    uint8_t col =
      slot % 2;

    printMenuCell(
      row,
      col,
      COLOR_NAMES[index],
      index == colorCursor
    );
  }
}

// ============================================================
// POSITION MENU
// ============================================================

void renderPositionMenu() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("CALIBRATE POSITION"));

  for (int i = 0; i < 6; i++) {

    uint8_t row =
      1 + i / 2;

    uint8_t col =
      i % 2;

    lcd.setCursor(
      col * 10,
      row
    );

    if (i == positionCursor) {
      lcd.print('>');
    }
    else {
      lcd.print(' ');
    }

    if (i < 5) {

      lcd.print(F("POS"));
      lcd.print(i + 1);
      lcd.print(':');

      int colorIndex =
        db.positionColor[i];

      if (
        colorIndex >= 0 &&
        colorIndex < COLOR_COUNT
      ) {

        lcd.print(
          COLOR_NAMES[colorIndex]
        );
      }

      else {
        lcd.print(F("--"));
      }
    }

    else {

      lcd.print(F("DONE"));
    }

    // Điền khoảng trắng cho cell
    // để dữ liệu cũ không còn trên LCD.
    int printed = 1;

    if (i < 5) {

      printed += 4;

      int colorIndex =
        db.positionColor[i];

      if (
        colorIndex >= 0 &&
        colorIndex < COLOR_COUNT
      ) {

        printed +=
          strlen(
            COLOR_NAMES[colorIndex]
          );
      }

      else {
        printed += 2;
      }
    }

    else {
      printed += 4;
    }

    while (printed < 10) {

      lcd.print(' ');
      printed++;
    }
  }
}

// ============================================================
// CHECK WHETHER COLOR IS ALREADY USED
// ============================================================

bool colorUsedByOtherPosition(
  int colorIndex,
  int exceptPosition
) {

  for (int i = 0; i < 5; i++) {

    if (i == exceptPosition) {
      continue;
    }

    if (
      db.positionColor[i] ==
      colorIndex
    ) {

      return true;
    }
  }

  return false;
}

// ============================================================
// BUILD AVAILABLE POSITION COLORS
// ============================================================

int buildPositionCandidates(
  int positionIndex,
  int candidateList[]
) {

  int count = 0;

  for (int i = 0; i < COLOR_COUNT; i++) {

    // Chỉ cho chọn màu đã calibration.
    if (!db.colors[i].valid) {
      continue;
    }

    // Không cho màu bị dùng bởi position khác.
    if (
      colorUsedByOtherPosition(
        i,
        positionIndex
      )
    ) {

      continue;
    }

    candidateList[count] = i;

    count++;
  }

  return count;
}

// ============================================================
// POSITION COLOR MENU
// ============================================================

void renderPositionColorMenu() {

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print(F("POS"));
  lcd.print(selectedPosition + 1);
  lcd.print(F(" SELECT COLOUR"));

  int candidates[COLOR_COUNT];

  int count =
    buildPositionCandidates(
      selectedPosition,
      candidates
    );

  if (count == 0) {

    lcd.setCursor(0, 1);
    lcd.print(F("NO FREE COLOUR"));

    return;
  }

  int page =
    positionColorCursor / 6;

  int first =
    page * 6;

  for (int slot = 0; slot < 6; slot++) {

    int candidateIndex =
      first + slot;

    if (candidateIndex >= count) {
      break;
    }

    int colorIndex =
      candidates[candidateIndex];

    uint8_t row =
      1 + slot / 2;

    uint8_t col =
      slot % 2;

    printMenuCell(
      row,
      col,
      COLOR_NAMES[colorIndex],
      candidateIndex ==
      positionColorCursor
    );
  }
}

// ============================================================
// CURSOR MOVEMENT
// ============================================================

void moveCursor(
  int &cursor,
  int count,
  int direction
) {

  cursor += direction;

  // Wrap-around
  if (cursor < 0) {
    cursor = count - 1;
  }

  if (cursor >= count) {
    cursor = 0;
  }

  Serial.print(F("[CURSOR] "));
  Serial.print(cursor);
  Serial.print('/');
  Serial.println(count - 1);
}

// ============================================================
// PRODUCT DETECTION
// A0 > 200
// MUST BE TRUE 3 TIMES CONSECUTIVELY
// ============================================================

bool detectProductStable() {

  Serial.println(F("--------------------------------"));
  Serial.println(
    F("[PRODUCT] Waiting...")
  );

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("WAIT PRODUCT"));

  int stableCount = 0;

  while (
    stableCount < PRODUCT_SAMPLES
  ) {

    int value =
      analogRead(PHOTO_PIN);

    Serial.print(F("[PRODUCT] A0 = "));
    Serial.print(value);

    if (
      value > PRODUCT_THRESHOLD
    ) {

      stableCount++;

      Serial.print(F(" -> OK "));
      Serial.print(stableCount);
      Serial.print('/');
      Serial.println(
        PRODUCT_SAMPLES
      );
    }

    else {

      stableCount = 0;

      Serial.println(
        F(" -> RESET")
      );
    }

    lcd.setCursor(0, 1);

    lcd.print(F("A0:"));
    lcd.print(value);
    lcd.print(F(" TH:"));
    lcd.print(PRODUCT_THRESHOLD);
    lcd.print(F("   "));

    lcd.setCursor(0, 2);

    lcd.print(F("STABLE: "));
    lcd.print(stableCount);
    lcd.print(F("/3      "));

    delay(50);
  }

  Serial.println(
    F("[PRODUCT] DETECTED")
  );

  lcd.setCursor(0, 3);
  lcd.print(F("PRODUCT DETECTED"));

  delay(300);

  return true;
}

// ============================================================
// TCS3200 FILTER SELECTION
// ============================================================

void setTCSColorFilter(
  bool s2,
  bool s3
) {

  digitalWrite(
    TCS_S2,
    s2 ? HIGH : LOW
  );

  digitalWrite(
    TCS_S3,
    s3 ? HIGH : LOW
  );

  // Chờ sensor ổn định sau khi đổi filter
  delayMicroseconds(1000);
}

// ============================================================
// READ TCS3200 LOW PULSE WIDTH
// ============================================================

uint32_t readTCSPeriod() {

  unsigned long period =
    pulseIn(
      TCS_OUT,
      LOW,
      PULSE_TIMEOUT_US
    );

  return (uint32_t)period;
}

// ============================================================
// READ ONE RGB SAMPLE
// ============================================================

bool readColorSample(
  ColorSample &sample
) {

  // ----------------------------------------------------------
  // RED
  // S2 = LOW
  // S3 = LOW
  // ----------------------------------------------------------

  setTCSColorFilter(
    LOW,
    LOW
  );

  sample.red =
    readTCSPeriod();

  if (sample.red == 0) {

    Serial.println(
      F("[TCS] RED TIMEOUT")
    );

    return false;
  }

  // ----------------------------------------------------------
  // BLUE
  // S2 = LOW
  // S3 = HIGH
  // ----------------------------------------------------------

  setTCSColorFilter(
    LOW,
    HIGH
  );

  sample.blue =
    readTCSPeriod();

  if (sample.blue == 0) {

    Serial.println(
      F("[TCS] BLUE TIMEOUT")
    );

    return false;
  }

  // ----------------------------------------------------------
  // GREEN
  // S2 = HIGH
  // S3 = HIGH
  // ----------------------------------------------------------

  setTCSColorFilter(
    HIGH,
    HIGH
  );

  sample.green =
    readTCSPeriod();

  if (sample.green == 0) {

    Serial.println(
      F("[TCS] GREEN TIMEOUT")
    );

    return false;
  }

  return true;
}

// ============================================================
// MEASURE RGB 3 TIMES
// CHECK STABILITY <= 20
// ============================================================

#define MAX_COLOR_RETRY 5

bool measureColorStable(ColorSample &average) {

  Serial.println(F("--------------------------------"));
  Serial.println(F("[COLOR] Start measurement"));

  ColorSample samples[COLOR_SAMPLES];

  for (int attempt = 1; attempt <= MAX_COLOR_RETRY; attempt++) {

    Serial.print(F("[COLOR] ATTEMPT "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.println(MAX_COLOR_RETRY);

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(F("READING COLOUR"));

    lcd.setCursor(0, 1);
    lcd.print(F("ATTEMPT "));
    lcd.print(attempt);
    lcd.print(F("/"));
    lcd.print(MAX_COLOR_RETRY);

    bool allValid = true;

    // ========================================================
    // ĐO 3 MẪU
    // ========================================================

    for (int i = 0; i < COLOR_SAMPLES; i++) {

      Serial.print(F("[COLOR] SAMPLE "));
      Serial.print(i + 1);
      Serial.println(F("/3"));

      lcd.setCursor(0, 2);
      lcd.print(F("SAMPLE "));
      lcd.print(i + 1);
      lcd.print(F("/3        "));

      if (!readColorSample(samples[i])) {

        allValid = false;

        Serial.println(
          F("[COLOR] SENSOR ERROR")
        );

        break;
      }

      Serial.print(F("  R = "));
      Serial.println(samples[i].red);

      Serial.print(F("  B = "));
      Serial.println(samples[i].blue);

      Serial.print(F("  G = "));
      Serial.println(samples[i].green);

      lcd.setCursor(0, 3);

      lcd.print(F("R"));
      lcd.print(samples[i].red);

      lcd.print(F(" B"));
      lcd.print(samples[i].blue);

      delay(100);
    }

    // ========================================================
    // SENSOR ERROR
    // ========================================================

    if (!allValid) {

      Serial.println(
        F("[COLOR] Invalid sample set")
      );

      delay(300);
      continue;
    }

    // ========================================================
    // FIND MIN / MAX
    // ========================================================

    uint32_t rMin = samples[0].red;
    uint32_t rMax = samples[0].red;

    uint32_t bMin = samples[0].blue;
    uint32_t bMax = samples[0].blue;

    uint32_t gMin = samples[0].green;
    uint32_t gMax = samples[0].green;

    uint32_t rSum = 0;
    uint32_t bSum = 0;
    uint32_t gSum = 0;

    for (int i = 0; i < COLOR_SAMPLES; i++) {

      if (samples[i].red < rMin)
        rMin = samples[i].red;

      if (samples[i].red > rMax)
        rMax = samples[i].red;

      if (samples[i].blue < bMin)
        bMin = samples[i].blue;

      if (samples[i].blue > bMax)
        bMax = samples[i].blue;

      if (samples[i].green < gMin)
        gMin = samples[i].green;

      if (samples[i].green > gMax)
        gMax = samples[i].green;

      rSum += samples[i].red;
      bSum += samples[i].blue;
      gSum += samples[i].green;
    }

    // ========================================================
    // CALCULATE ERROR
    // ========================================================

    uint32_t rError = rMax - rMin;
    uint32_t bError = bMax - bMin;
    uint32_t gError = gMax - gMin;

    Serial.println(F("[COLOR] Stability check"));

    Serial.print(F("  RED   : "));
    Serial.println(rError);

    Serial.print(F("  BLUE  : "));
    Serial.println(bError);

    Serial.print(F("  GREEN : "));
    Serial.println(gError);

    // ========================================================
    // ACCEPT
    // ========================================================

    if (
      rError <= COLOR_STABLE_ERROR &&
      bError <= COLOR_STABLE_ERROR &&
      gError <= COLOR_STABLE_ERROR
    ) {

      average.red =
        rSum / COLOR_SAMPLES;

      average.blue =
        bSum / COLOR_SAMPLES;

      average.green =
        gSum / COLOR_SAMPLES;

      Serial.println(
        F("[COLOR] ACCEPTED")
      );

      Serial.print(F("  AVG R = "));
      Serial.println(average.red);

      Serial.print(F("  AVG B = "));
      Serial.println(average.blue);

      Serial.print(F("  AVG G = "));
      Serial.println(average.green);

      return true;
    }

    // ========================================================
    // REJECT
    // ========================================================

    Serial.println(
      F("[COLOR] REJECTED")
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(F("UNSTABLE COLOUR"));

    lcd.setCursor(0, 1);
    lcd.print(F("Rerr:"));
    lcd.print(rError);

    lcd.setCursor(0, 2);
    lcd.print(F("Berr:"));
    lcd.print(bError);

    lcd.setCursor(0, 3);
    lcd.print(F("Gerr:"));
    lcd.print(gError);

    delay(700);
  }

  // ==========================================================
  // MAX RETRY REACHED
  // ==========================================================

  Serial.println(F("--------------------------------"));
  Serial.println(
    F("[COLOR] MAX RETRY REACHED")
  );
  Serial.println(
    F("[COLOR] Calibration cancelled")
  );

  showMessage(
    "COLOR ERROR",
    "CALIBRATION FAILED",
    1200
  );

  return false;
}
// ======================================================
// CALIBRATE ONE COLOR
// ============================================================

void calibrateColor(
  int colorIndex
) {

  Serial.println(F("================================"));

  Serial.print(
    F("[CALIBRATE] ")
  );

  Serial.println(
    COLOR_NAMES[colorIndex]
  );

  // ----------------------------------------------------------
  // STEP 1: WAIT FOR PRODUCT
  // ----------------------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("CALIBRATE COLOUR"));

  lcd.setCursor(0, 1);
  lcd.print(
    COLOR_NAMES[colorIndex]
  );

  lcd.setCursor(0, 2);
  lcd.print(F("WAIT PRODUCT..."));

  detectProductStable();

  // ----------------------------------------------------------
  // STEP 2: MEASURE RGB
  // ----------------------------------------------------------

  ColorSample average;

  if (
    !measureColorStable(
      average
    )
  ) {

    return;
  }

  // ----------------------------------------------------------
  // STEP 3: BUILD +/-30 RANGE
  // ----------------------------------------------------------

  uint32_t redMin =
    (average.red > COLOR_RANGE)
    ? average.red - COLOR_RANGE
    : 0;

  uint32_t redMax =
    average.red + COLOR_RANGE;

  uint32_t blueMin =
    (average.blue > COLOR_RANGE)
    ? average.blue - COLOR_RANGE
    : 0;

  uint32_t blueMax =
    average.blue + COLOR_RANGE;

  uint32_t greenMin =
    (average.green > COLOR_RANGE)
    ? average.green - COLOR_RANGE
    : 0;

  uint32_t greenMax =
    average.green + COLOR_RANGE;

  // ----------------------------------------------------------
  // STEP 4: SAVE TO RAM
  // ----------------------------------------------------------

  db.colors[colorIndex].valid = 1;

  db.colors[colorIndex].redMin =
    redMin;

  db.colors[colorIndex].redMax =
    redMax;

  db.colors[colorIndex].blueMin =
    blueMin;

  db.colors[colorIndex].blueMax =
    blueMax;

  db.colors[colorIndex].greenMin =
    greenMin;

  db.colors[colorIndex].greenMax =
    greenMax;

  // ----------------------------------------------------------
  // SERIAL DEBUG
  // ----------------------------------------------------------

  Serial.println(
    F("[CALIBRATE] Final limits:")
  );

  Serial.print(
    F("RED   : ")
  );

  Serial.print(redMin);
  Serial.print(F(" -> "));
  Serial.println(redMax);

  Serial.print(
    F("BLUE  : ")
  );

  Serial.print(blueMin);
  Serial.print(F(" -> "));
  Serial.println(blueMax);

  Serial.print(
    F("GREEN : ")
  );

  Serial.print(greenMin);
  Serial.print(F(" -> "));
  Serial.println(greenMax);

  // ----------------------------------------------------------
  // STEP 5: SAVE EEPROM
  // ----------------------------------------------------------

  saveEEPROM();

  // ----------------------------------------------------------
  // LCD RESULT
  // ----------------------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(
    COLOR_NAMES[colorIndex]
  );

  lcd.print(F(" SAVED"));

  lcd.setCursor(0, 1);

  lcd.print(F("R "));
  lcd.print(redMin);
  lcd.print('-');
  lcd.print(redMax);

  lcd.setCursor(0, 2);

  lcd.print(F("B "));
  lcd.print(blueMin);
  lcd.print('-');
  lcd.print(blueMax);

  lcd.setCursor(0, 3);

  lcd.print(F("G "));
  lcd.print(greenMin);
  lcd.print('-');
  lcd.print(greenMax);

  delay(1500);

  Serial.println(
    F("[CALIBRATE] COMPLETE")
  );
}

// ============================================================
// DEBUG ONE COLOR
// ============================================================

void printColorData(
  int index
) {

  Serial.println(F("--------------------------------"));

  Serial.print(
    F("COLOR: ")
  );

  Serial.println(
    COLOR_NAMES[index]
  );

  if (!db.colors[index].valid) {

    Serial.println(
      F("STATUS: NOT CALIBRATED")
    );

    return;
  }

  Serial.println(
    F("STATUS: CALIBRATED")
  );

  Serial.print(
    F("R MIN/MAX: ")
  );

  Serial.print(
    db.colors[index].redMin
  );

  Serial.print(F(" / "));

  Serial.println(
    db.colors[index].redMax
  );

  Serial.print(
    F("B MIN/MAX: ")
  );

  Serial.print(
    db.colors[index].blueMin
  );

  Serial.print(F(" / "));

  Serial.println(
    db.colors[index].blueMax
  );

  Serial.print(
    F("G MIN/MAX: ")
  );

  Serial.print(
    db.colors[index].greenMin
  );

  Serial.print(F(" / "));

  Serial.println(
    db.colors[index].greenMax
  );
}

// ============================================================
// LCD MESSAGE
// ============================================================

void showMessage(
  const char* line1,
  const char* line2,
  unsigned long ms
) {

  lcd.clear();

  lcd.setCursor(0, 1);
  lcd.print(line1);

  lcd.setCursor(0, 2);
  lcd.print(line2);

  delay(ms);
}