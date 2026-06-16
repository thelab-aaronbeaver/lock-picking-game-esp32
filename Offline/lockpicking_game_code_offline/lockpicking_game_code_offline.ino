#include <Arduino.h>
#include <LiquidCrystal_I2C.h>  // Include the LiquidCrystal_I2C library

#include <WiFi.h>
#include <esp_bt.h>

// Define LCD parameters
#define I2C_ADDR 0x27  //  Check your LCD's I2C address. Common ones are 0x27 or 0x3F.
#define LCD_COLUMNS 20
#define LCD_ROWS 4

// Create LCD object
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

// Define GPIO pins
const int buttonPin = 15;

struct CylinderState {
  int pin;
  int ledPin;
  int norm;
  int raw;
  int mapped;
  bool triggered;
  unsigned long time;
};

const int CYLINDER_COUNT = 3;
CylinderState cylinders[CYLINDER_COUNT] = {
  {32, 14, 0, 0, 0, false, 0},
  {35, 12, 0, 0, 0, false, 0},
  {34, 13, 0, 0, 0, false, 0}
};

int reading = 0;
int buttonValue = 0;


const int THRESHOLD = 10;


//--- board ID --
int boardID = 18;

// Button state variables
int buttonState;
int previousButtonState = HIGH;
const int debounceDelay = 30;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressStartTime = 0;

// Stopwatch variables
unsigned long startTime = 0;
int runCount = 0;
bool stopwatchRunning = false;
const unsigned long stopwatchLimit = 120000;  // 60 seconds in milliseconds

bool confirm = false;
bool longPressHandled = false;
bool lapClearPending = false;
unsigned long lapClearStartTime = 0;
String lastLine0 = "";
String lastLine1 = "";
bool statusMessageActive = false;
unsigned long statusMessageUntil = 0;
String statusLine1 = "";
bool waitingForResultAck = false;
bool resultResetPromptShown = false;
bool cylinderResetPromptActive = false;
unsigned long resultDisplayStartMs = 0;
const unsigned long RESULT_DISPLAY_DURATION_MS = 10000;


// Function to check if all pots are past 50%
bool allCylindersTriggered() {
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    if (!cylinders[i].triggered) {
      return false;
    }
  }
  return true;
}
// Function to check if all LEDs are off
bool allLedsOff() {
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    if (digitalRead(cylinders[i].ledPin) != LOW) {
      return false;
    }
  }
  return true;
}

unsigned long lastTotalTime = 0;
unsigned long lastHeapLogMs = 0;
const unsigned long HEAP_LOG_INTERVAL_MS = 30000;


// Flag to indicate if the stopwatch should be started
bool startStopwatch = false;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    pinMode(cylinders[i].pin, INPUT);
    pinMode(cylinders[i].ledPin, OUTPUT);
    digitalWrite(cylinders[i].ledPin, LOW);
  }
  // Read initial button state
  previousButtonState = digitalRead(buttonPin);
  delay(100);
  // Initialize the LCD
  lcd.init();
  lcd.backlight();  // Turn on the backlight
  lcd.clear();      // Clear the LCD display

  // Print initial message on LCD (optional)
  lcd.setCursor(0, 0);
  lcd.print("Loc Doc ");
  lcd.print("ID:");
  lcd.print(boardID);
  // Print initial message on LCD
  lcd.setCursor(0, 1);

  // Force wireless offline mode for this firmware build.
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  btStop();
  setLine0("Built by: LOCDOC.NET");
  setLine1("Button:  Start/Reset");

  for (int i = 0; i < CYLINDER_COUNT; i++) {
    const int cylinderBaseValue = analogRead(cylinders[i].pin);
    cylinders[i].norm = cylinderBaseValue / 40.95;
  }


  Serial.println("Offline mode active (WiFi/Bluetooth disabled)");
}




void loop() {
    reading = digitalRead(buttonPin);

    if (millis() - lastHeapLogMs >= HEAP_LOG_INTERVAL_MS) {
      lastHeapLogMs = millis();
      Serial.printf("Heap: %u bytes\n", ESP.getFreeHeap());
    }

    // Read potentiometer values
    for (int i = 0; i < CYLINDER_COUNT; i++) {
      cylinders[i].raw = analogRead(cylinders[i].pin);
      cylinders[i].mapped = map(cylinders[i].raw, 0, 4095, 0, 100);
    }
    // --- LED Light Check (before button logic) ---
    for (int i = 0; i < CYLINDER_COUNT; i++) {
      cylinders[i].triggered = checkLEDLight(cylinders[i]);
    }

    // --- Button logic with debouncing and LED check ---
    if (reading != previousButtonState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == LOW) {  // Button pressed
          buttonPressStartTime = millis();
          longPressHandled = false;
          if (waitingForResultAck) {
            waitingForResultAck = false;
            resultResetPromptShown = false;
            cylinderResetPromptActive = false;
            clearLapRows();
            statusMessageActive = false;
            statusLine1 = "";
            lastLine1 = "";
            clearLine(1);
            setLine0("Built by: LOCDOC.NET");
            setLine1("Button:  Start/Reset");
          } else if (stopwatchRunning) {
            stopwatchRunning = false;
            Serial.println("Stopwatch Stopped");
          } else if (allLedsOff()) {
            lcd.clear();
            startLightSequence();
            stopwatchRunning = true;
            startTime = millis();
            for (int i = 0; i < CYLINDER_COUNT; i++) {
              cylinders[i].time = 0;
              cylinders[i].triggered = false;
            }
            lapClearPending = false;
            lapClearStartTime = 0;
            resultResetPromptShown = false;
            cylinderResetPromptActive = false;
            resultDisplayStartMs = 0;
            lcd.clear();  // Clear any previous messages
          } else {
            setLine0("  Reset cylinders! ");
            cylinderResetPromptActive = true;
            delay(2000);
          }
        }
      }
    }

    if (buttonState == LOW && !waitingForResultAck) {  // Check for long press only when button is held down
      unsigned long longPressTime = millis() - buttonPressStartTime;
      if (longPressTime > 10000 && !longPressHandled) {  // 10 seconds
        longPressHandled = true;
        resetGameState();  // Re-init game state only
      }
    } else {
      longPressHandled = false;
    }

    previousButtonState = reading;

    // --- Stopwatch and Cylinder Logic ---
    if (stopwatchRunning) {
      for (int i = 0; i < CYLINDER_COUNT; i++) {
        checkCylinderTrigger(cylinders[i]);
      }
      displayLapTimesOnLCD();

      unsigned long currentTime = millis() - startTime;

      if (currentTime >= stopwatchLimit || allCylindersTriggered()) {
        stopwatchRunning = false;
        sendDataToDatabase(currentTime, cylinders[0].time, cylinders[1].time, cylinders[2].time, boardID);
        waitingForResultAck = true;
        resultResetPromptShown = false;
        resultDisplayStartMs = millis();
        lapClearPending = false;
        lapClearStartTime = 0;
        lastTotalTime = currentTime;
        if (currentTime >= stopwatchLimit) {
          //lcd.setCursor(0, 0);
          //lcd.print("Times Up!");
        } else {
          //lcd.setCursor(0, 0);
          //lcd.print("Finished!");
        }
      } else {
        displayTimeOnLCD(currentTime);
      }
    } else {
      // --- Reset cylinders when stopwatch is not running ---
      for (int i = 0; i < CYLINDER_COUNT; i++) {
        cylinders[i].triggered = checkLEDLight(cylinders[i]);
      }
      if (lapClearPending) {
        if (allLedsOff() && lapClearStartTime == 0) {
          lapClearStartTime = millis();
        }
        if (lapClearStartTime > 0 && (millis() - lapClearStartTime) >= 5000) {
          clearLapRows();
          lapClearPending = false;
        }
      }

      // --- Display "Ready" or "Finished!" ---
      if (statusMessageActive && millis() >= statusMessageUntil) {
        statusMessageActive = false;
        if (statusLine1.length() > 0) {
          clearLine(1);
        }
        statusLine1 = "";
        lastLine0 = "";
        lastLine1 = "";
      }

      if (waitingForResultAck) {
        if (!resultResetPromptShown && (millis() - resultDisplayStartMs) >= RESULT_DISPLAY_DURATION_MS) {
          clearLapRows();
          clearLine(1);
          lcd.setCursor(0, 1);
          lcd.print("Press button reset");
          lastLine1 = "Press button reset";
          resultResetPromptShown = true;
        }
      } else if (allLedsOff() && !statusMessageActive) {
        if (cylinderResetPromptActive) {
          lcd.clear();
          cylinderResetPromptActive = false;
          lastLine0 = "";
          lastLine1 = "";
        }
        setLine0("Built by: LOCDOC.NET");
        setLine1("Button:  Start/Reset");
        //lcd.setCursor(0, 1);
        //lcd.print("Push Button to Start");
      } else {
        //lcd.print("DONE! ");
        lastLine0 = "";
      }
    }
    delay(1);
  
}


// Function to format and display time on LCD
void displayTimeOnLCD(unsigned long time) {
  unsigned long milliseconds = time % 1000;
  unsigned long seconds = (time / 1000) % 60;
  unsigned long minutes = (time / (1000 * 60)) % 60;

  // Divide milliseconds by 10 to get 2 digits
  int formattedMillis = milliseconds / 10;
  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu:%02d", minutes, seconds, formattedMillis);

  lastLine0 = "__timer__";
  clearLine(0);
  lcd.setCursor(0, 0);
  lcd.print("Timer:");
  lcd.print(timeBuf);
}

void displayLapTimesOnLCD() {
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    lcd.setCursor(0, i + 1);
    lcd.print("C");
    lcd.print(i + 1);
    lcd.print(" ");
    displayLapTime(cylinders[i].time);
  }
}

void displayLapTime(unsigned long time) {
  unsigned long milliseconds = time % 1000;
  unsigned long seconds = (time / 1000) % 60;
  unsigned long minutes = (time / (1000 * 60)) % 60;

  // Divide milliseconds by 10 to get 2 digits
  int formattedMillis = milliseconds / 10;

  char timeBuf[9];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu:%02d", minutes, seconds, formattedMillis);
  lcd.print(timeBuf);
}

void clearLine(int row) {
  lcd.setCursor(0, row);
  for (int i = 0; i < LCD_COLUMNS; i++) {
    lcd.print(" ");
  }
  lcd.setCursor(0, row);
}

void setLine0(const String& text) {
  if (lastLine0 != text) {
    clearLine(0);
    lcd.setCursor(0, 0);
    lcd.print(text);
    lastLine0 = text;
  }
}

void setLine1(const String& text) {
  if (lastLine1 != text) {
    clearLine(1);
    lcd.setCursor(0, 1);
    lcd.print(text);
    lastLine1 = text;
  }
}

void showStatusMessage(const String& line0, const String& line1, unsigned long durationMs) {
  statusMessageActive = true;
  statusMessageUntil = millis() + durationMs;
  statusLine1 = line1;
  setLine0(line0);
  setLine1(line1);
}

void showLine0Message(const String& line0, unsigned long durationMs) {
  statusMessageActive = true;
  statusMessageUntil = millis() + durationMs;
  statusLine1 = "";
  setLine0(line0);
}

void clearLapRows() {
  for (int row = 1; row <= 3; row++) {
    clearLine(row);
  }
}

void showStartScreen(int litCount) {
  clearLine(1);
  clearLine(2);
  clearLine(3);
  lcd.setCursor(0, 1);
  lcd.print("Get Ready...");
  lcd.setCursor(0, 2);
  lcd.print("Lights: [");
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    lcd.print(i < litCount ? "#" : " ");
  }
  lcd.print("]");
  lcd.setCursor(0, 3);
  if (litCount < CYLINDER_COUNT) {
    lcd.print("Hold steady");
  } else {
    lcd.print("GO!");
  }
}

void startLightSequence() {
  setLine0("   START SEQUENCE");
  showStartScreen(1);
  digitalWrite(cylinders[0].ledPin, HIGH);  // Red LED on
  delay(1000);
  showStartScreen(2);
  digitalWrite(cylinders[1].ledPin, HIGH);  // Yellow LED on
  delay(1000);
  showStartScreen(3);
  digitalWrite(cylinders[2].ledPin, HIGH);  // Green LED on
  delay(2000);
  digitalWrite(cylinders[0].ledPin, LOW);  // All LEDs off
  digitalWrite(cylinders[1].ledPin, LOW);
  digitalWrite(cylinders[2].ledPin, LOW);
}

void resetGameState() {
  stopwatchRunning = false;
  startStopwatch = false;
  lapClearPending = false;
  lapClearStartTime = 0;
  lastTotalTime = 0;
  for (int i = 0; i < CYLINDER_COUNT; i++) {
    cylinders[i].triggered = false;
    cylinders[i].time = 0;
    digitalWrite(cylinders[i].ledPin, LOW);
  }
  clearLapRows();
  setLine0("Game reset");
  delay(5000);
  setLine0("Ready");
}

void checkCylinderTrigger(CylinderState& cylinder) {
  const int UPPER_THRESHOLD = cylinder.norm + THRESHOLD;
  const int LOWER_THRESHOLD = cylinder.norm - THRESHOLD;
  if ((cylinder.mapped > UPPER_THRESHOLD || cylinder.mapped < LOWER_THRESHOLD) && !cylinder.triggered) {
    cylinder.time = millis() - startTime;
    cylinder.triggered = true;
    digitalWrite(cylinder.ledPin, HIGH);
    //... (other actions)
  }
}

bool checkLEDLight(CylinderState& cylinder) {
  const int UPPER_THRESHOLD = cylinder.norm + THRESHOLD;
  const int LOWER_THRESHOLD = cylinder.norm - THRESHOLD;
  if (cylinder.mapped > LOWER_THRESHOLD && cylinder.mapped < UPPER_THRESHOLD) {
    cylinder.triggered = false;
    digitalWrite(cylinder.ledPin, LOW);
    return false;
  } else {
    return cylinder.triggered;
  }
}

void sendDataToDatabase(unsigned long totalTime, unsigned long cylinder1, unsigned long cylinder2, unsigned long cylinder3, unsigned long boardID) {
  unsigned long totalMilliseconds = totalTime % 1000;
  unsigned long totalSeconds = (totalTime / 1000) % 60;
  unsigned long totalMinutes = (totalTime / (1000 * 60)) % 60;
  int totalFormattedMillis = totalMilliseconds / 10;
  char totalBuf[18];
  snprintf(totalBuf, sizeof(totalBuf), "Result:%02lu:%02lu:%02d", totalMinutes, totalSeconds, totalFormattedMillis);
  setLine0(String(totalBuf));
  clearLine(1);
  lcd.setCursor(0, 1);
  lcd.print("C1 ");
  displayLapTime(cylinder1);
  clearLine(2);
  lcd.setCursor(0, 2);
  lcd.print("C2 ");
  displayLapTime(cylinder2);
  clearLine(3);
  lcd.setCursor(0, 3);
  lcd.print("C3 ");
  displayLapTime(cylinder3);
  lastLine1 = "";
  Serial.println("Run result (offline):");
  Serial.printf("Board: %lu\n", boardID);
  Serial.printf("Total: %lu ms, C1: %lu ms, C2: %lu ms, C3: %lu ms\n", totalTime, cylinder1, cylinder2, cylinder3);
}