#include <NewPing.h>            
#include <Adafruit_SSD1306.h>   
#include <Adafruit_GFX.h>       
#include <Wire.h>               
#include <Bounce2.h>            

// Pin definitions
#define TRIG_PIN 3
#define ECHO_PIN 4
#define RELAY_PIN 5
#define BUTTON_UP_PIN 6
#define BUTTON_DOWN_PIN 7
#define BUTTON_MENU_PIN 8
#define BUZZER_PIN 9
#define FLOAT_SWITCH_PIN 10
#define SWITCH_BUTTON_PIN 11    

// Display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// Initialize OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Initialize debounced buttons
Bounce buttonUp = Bounce();
Bounce buttonDown = Bounce();
Bounce buttonMenu = Bounce();
Bounce floatSwitch = Bounce();
Bounce switchButton = Bounce();

// Constants and variables for system configuration
#define MIN_VESSEL_HEIGHT 3
int vesselHeight = 10; // Default vessel height in cm
int fillThreshold = 5; // Default fill threshold in cm
const int hysteresis = 2; // Hysteresis for pump control
bool pumpState = true; // Pump initial state (true = OFF)
bool inMenu = false; // Menu navigation state
bool alarmState = false; // Alarm state
bool dosingPaused = true; // Dosing pause state
byte menuPage = 0; // Current menu page
unsigned long lastMenuAction = 0; // Last menu interaction time
unsigned long alarmStartTime = 0; // Alarm start time
unsigned long lastDisplayUpdate = 0; // Last display update time
const unsigned long displayInterval = 500; // Display update interval in ms

// Initialize ultrasonic sensor
NewPing sonar(TRIG_PIN, ECHO_PIN, 200);

// Setup function to initialize hardware and configurations
void setup() {
  Serial.begin(115200); // Start serial communication
  pinMode(RELAY_PIN, OUTPUT); // Set relay pin as output
  pinMode(BUZZER_PIN, OUTPUT); // Set buzzer pin as output
  pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP); // Set float switch pin
  pinMode(SWITCH_BUTTON_PIN, INPUT_PULLUP); // Set switch button pin
  digitalWrite(RELAY_PIN, HIGH); // Initialize pump to OFF

  // Attach buttons to debouncer with pull-up resistors
  buttonUp.attach(BUTTON_UP_PIN, INPUT_PULLUP);
  buttonDown.attach(BUTTON_DOWN_PIN, INPUT_PULLUP);
  buttonMenu.attach(BUTTON_MENU_PIN, INPUT_PULLUP);
  floatSwitch.attach(FLOAT_SWITCH_PIN, INPUT_PULLUP);
  switchButton.attach(SWITCH_BUTTON_PIN, INPUT_PULLUP);

  // Set debounce interval for buttons
  buttonUp.interval(10);
  buttonDown.interval(10);
  buttonMenu.interval(10);
  floatSwitch.interval(10);
  switchButton.interval(10);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    while (true); // Halt if display initialization fails
  }
  display.clearDisplay();
  display.display(); // Clear and update display
}

// Main loop to handle system logic
void loop() {
  // Update button states
  buttonUp.update();
  buttonDown.update();
  buttonMenu.update();
  floatSwitch.update();
  switchButton.update();

  // Toggle dosing pause on switch button press
  if (switchButton.fell()) {
    dosingPaused = !dosingPaused;
    if (dosingPaused) {
      pumpState = true;
      digitalWrite(RELAY_PIN, HIGH); // Turn pump OFF
      alarmState = false;
      digitalWrite(BUZZER_PIN, LOW); // Turn buzzer OFF
    }
    lastMenuAction = millis();
  }

  // Handle menu navigation
  handleMenu();
  
  // Main system logic when not in menu
  if (!inMenu) {
    int distance = getFilteredDistance(); // Get filtered distance from sensor
    int fillLevel = vesselHeight - distance; // Calculate fill level
    fillLevel = constrain(fillLevel, 0, vesselHeight); // Constrain fill level

    if (!dosingPaused) {
      checkAlarmConditions(distance); // Check for alarm conditions
    }
    controlPump(fillLevel); // Control pump based on fill level
    updateMainDisplay(fillLevel, distance); // Update display

    // Control buzzer based on alarm state
    if (alarmState && !dosingPaused && (millis() / 500) % 2 == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  delay(100); // Loop delay
}

// Check alarm conditions based on sensor data
void checkAlarmConditions(int distance) {
  bool floatTriggered = floatSwitch.read() == LOW; // Check float switch
  bool criticalLevel = distance < MIN_VESSEL_HEIGHT; // Check critical level

  if (floatTriggered || criticalLevel) {
    if (!alarmState) {
      alarmState = true;
      alarmStartTime = millis(); // Start alarm timer
    }
  } else if (alarmState && millis() - alarmStartTime > 5000) {
    alarmState = false; // Reset alarm after 5 seconds
  }
}

// Get filtered distance from ultrasonic sensor
int getFilteredDistance() {
  unsigned long startTime = millis();
  int distance = sonar.convert_cm(sonar.ping_median(3)); // Get median of 3 pings
  if (millis() - startTime > 100) {
    return vesselHeight; // Return max height if timeout
  }
  return (distance <= 0 || distance > vesselHeight) ? vesselHeight : distance; // Filter invalid readings
}

// Control pump based on fill level
void controlPump(int fillLevel) {
  if (dosingPaused) {
    pumpState = true;
    digitalWrite(RELAY_PIN, HIGH); // Turn pump OFF
  } else {
    if (pumpState && fillLevel < fillThreshold) {
      pumpState = false;
      digitalWrite(RELAY_PIN, LOW); // Turn pump ON
    } else if (!pumpState && fillLevel >= fillThreshold + hysteresis) {
      pumpState = true;
      digitalWrite(RELAY_PIN, HIGH); // Turn pump OFF
    }
  }
}

// Handle menu navigation and input
void handleMenu() {
  if (buttonMenu.fell()) {
    if (inMenu) {
      menuPage = (menuPage + 1) % 3; // Cycle through menu pages
      if (menuPage == 0) inMenu = false; // Exit menu
    } else {
      inMenu = true;
      menuPage = 1; // Enter menu
    }
    lastMenuAction = millis();
    display.clearDisplay(); // Clear display on menu change
  }

  if (inMenu) {
    if (millis() - lastMenuAction > 5000) {
      inMenu = false;
      menuPage = 0; // Exit menu after timeout
    }

    // Adjust settings based on button presses
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

    displayMenu(); // Update menu display
  }
}

// Display menu interface
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

// Update main display with system status
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
