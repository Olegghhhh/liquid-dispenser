#include <NewPing.h>            
#include <Adafruit_SSD1306.h>   
#include <Adafruit_GFX.h>       
#include <Wire.h>               
#include <Bounce2.h>            

#define TRIG_PIN 3
#define ECHO_PIN 4
#define RELAY_PIN 5
#define BUTTON_UP_PIN 6
#define BUTTON_DOWN_PIN 7
#define BUTTON_MENU_PIN 8
#define BUZZER_PIN 9
#define FLOAT_SWITCH_PIN 10
#define SWITCH_BUTTON_PIN 11    

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Bounce buttonUp = Bounce();
Bounce buttonDown = Bounce();
Bounce buttonMenu = Bounce();
Bounce floatSwitch = Bounce();
Bounce switchButton = Bounce();

#define MIN_VESSEL_HEIGHT 3
int vesselHeight = 10;
int fillThreshold = 5;
const int hysteresis = 2;
bool pumpState = true;
bool inMenu = false;
bool alarmState = false;
bool dosingPaused = true;
byte menuPage = 0;
unsigned long lastMenuAction = 0;
unsigned long alarmStartTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 500;

NewPing sonar(TRIG_PIN, ECHO_PIN, 200);

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(SWITCH_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH);

  buttonUp.attach(BUTTON_UP_PIN, INPUT_PULLUP);
  buttonDown.attach(BUTTON_DOWN_PIN, INPUT_PULLUP);
  buttonMenu.attach(BUTTON_MENU_PIN, INPUT_PULLUP);
  floatSwitch.attach(FLOAT_SWITCH_PIN, INPUT_PULLUP);
  switchButton.attach(SWITCH_BUTTON_PIN, INPUT_PULLUP);

  buttonUp.interval(10);
  buttonDown.interval(10);
  buttonMenu.interval(10);
  floatSwitch.interval(10);
  switchButton.interval(10);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    while (true);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  buttonUp.update();
  buttonDown.update();
  buttonMenu.update();
  floatSwitch.update();
  switchButton.update();

  if (switchButton.fell()) {
    dosingPaused = !dosingPaused;
    if (dosingPaused) {
      pumpState = true;
      digitalWrite(RELAY_PIN, HIGH);
      alarmState = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
    lastMenuAction = millis();
  }

  handleMenu();
  if (!inMenu) {
    int distance = getFilteredDistance();
    int fillLevel = vesselHeight - distance;
    fillLevel = constrain(fillLevel, 0, vesselHeight);

    if (!dosingPaused) {
      checkAlarmConditions(distance);
    }
    controlPump(fillLevel);
    updateMainDisplay(fillLevel, distance);

    if (alarmState && !dosingPaused && (millis() / 500) % 2 == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  delay(100);
}

void checkAlarmConditions(int distance) {
  bool floatTriggered = floatSwitch.read() == LOW;
  bool criticalLevel = distance < MIN_VESSEL_HEIGHT;

  if (floatTriggered || criticalLevel) {
    if (!alarmState) {
      alarmState = true;
      alarmStartTime = millis();
    }
  } else if (alarmState && millis() - alarmStartTime > 5000) {
    alarmState = false;
  }
}

int getFilteredDistance() {
  unsigned long startTime = millis();
  int distance = sonar.convert_cm(sonar.ping_median(3));
  if (millis() - startTime > 100) {
    return vesselHeight;
  }
  return (distance <= 0 || distance > vesselHeight) ? vesselHeight : distance;
}

void controlPump(int fillLevel) {
  if (dosingPaused) {
    pumpState = true;
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    if (pumpState && fillLevel < fillThreshold) {
      pumpState = false;
      digitalWrite(RELAY_PIN, LOW);
    } else if (!pumpState && fillLevel >= fillThreshold + hysteresis) {
      pumpState = true;
      digitalWrite(RELAY_PIN, HIGH);
    }
  }
}

void handleMenu() {
  if (buttonMenu.fell()) {
    if (inMenu) {
      menuPage = (menuPage + 1) % 3;
      if (menuPage == 0) inMenu = false;
    } else {
      inMenu = true;
      menuPage = 1;
    }
    lastMenuAction = millis();
    display.clearDisplay();
  }

  if (inMenu) {
    if (millis() - lastMenuAction > 5000) {
      inMenu = false;
      menuPage = 0;
    }

    if (buttonUp.fell()) {
      if (menuPage == 1) fillThreshold = min(fillThreshold + 1, vesselHeight - hysteresis);
      else if (menuPage == 2) vesselHeight = min(vesselHeight + 1, 200);
      lastMenuAction = millis();
    }

    if (buttonDown.fell()) {
      if (menuPage == 1) fillThreshold = max(fillThreshold - 1, 0);
      else if (menuPage == 2) vesselHeight = max(vesselHeight - 1, MIN_VESSEL_HEIGHT);
      lastMenuAction = millis();
    }

    displayMenu();
  }
}

void displayMenu() {
  if (millis() - lastDisplayUpdate < displayInterval) return;
  lastDisplayUpdate = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (menuPage == 1) {
    display.setCursor(0, 0);
    display.println("Fill Threshold:");
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print("> ");
    display.print(fillThreshold);
    display.println(" cm");
  } else if (menuPage == 2) {
    display.setCursor(0, 0);
    display.println("Vessel Height:");
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print("> ");
    display.print(vesselHeight);
    display.println(" cm");
  }

  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println("+/- Adjust MENU Next");
  display.display();
}

void updateMainDisplay(int fillLevel, int distance) {
  if (millis() - lastDisplayUpdate < displayInterval) return;
  lastDisplayUpdate = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (alarmState && !dosingPaused) {
    display.setTextColor(BLACK, WHITE);
    display.setCursor(0, 0);
    display.println("!ALARM LEVEL!");
    display.setTextColor(WHITE);
  } else {
    display.setCursor(0, 0);
    display.print("Fill:");
    display.print(fillLevel);
    display.print("/");
    display.print(vesselHeight);
    display.print("cm ");
    display.print(100 * fillLevel / vesselHeight);
    display.println("%");
  }

  display.setCursor(0, 10);
  display.print("Pump<");
  display.print(fillThreshold);
  display.println("cm");

  display.setCursor(0, 20);
  display.print("P:");
  display.print(pumpState ? "OFF" : "ON");
  display.print(" F:");
  display.print(floatSwitch.read() == HIGH ? "OK" : "TR");
  display.print(" T:");
  display.print(dosingPaused ? "ON" : "OFF");

  int fillWidth = map(fillLevel, 0, vesselHeight, 0, SCREEN_WIDTH-1);
  display.drawFastHLine(0, 31, SCREEN_WIDTH, WHITE);
  display.fillRect(0, 31, fillWidth, 3, WHITE);
  display.display();
}