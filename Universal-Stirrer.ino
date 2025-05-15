// Universal Stirrer Design for BLDC with Built in Driver and Display controll using Encoder and 7 Segment 4 Digit

#include <EEPROM.h>
const int EEPROM_RPM_ADDR = 0;  // Starting byte to store RPM

// Define motor control and feedback pins
const int pwmPin = 46;         // PWM pin connected to the blue line of the motor
const int dirPin = 13;        // Digital pin for direction (connected to the yellow line)
const int feedbackPin = 2;    // Digital pin connected to FG (Green line)

// Pins 22 to 29 connected to 7-segment display segments and DP
int pinA = A9;
int pinB = A2;
int pinC = A11;
int pinD = A12;
int pinE = A8;
int pinF = A7;
int pinG = A10;
int pinDP = A13;

// 4 Digits Control connected to Pins 50 to 53
int D1 = A5;
int D2 = A3;
int D3 = A4;
int D4 = A6;

int desiredRPM = 0;              // Target RPM (set via encoder)
float dutyCycle = 0.0;           // Mapped duty cycle
int desiredPWM = 0;              // Mapped PWM value from RPM
int rampUP = 255;                // Start from OFF
unsigned long lastEncoderMoveTime = 0;
bool adjustingRPM = false;

bool allowRampUpdate = false;
unsigned long lastRampUpdate = 0;
const unsigned long rampInterval = 400;  // in ms (can tweak for smoother/slower)

int lastSavedRPM = -1;
unsigned long lastEEPROMWrite = 0;
const unsigned long eepromWriteDelay = 2000;  // Only write after 2s idle


// Rotary encoder pins for adjusting RPM
int encoderPinA = 18;
int encoderPinB = 19;
int encoderButtonPin = 3;
int lastPWMValue = 255;  // Start with a low RPM (higher PWM value)
int targetPWM = 255; 
const int rampStep = 5;

// Variables for RPM control and feedback
volatile int encoderPos = 250;  // Initialize encoder position for low RPM start
int pwmValue = 255;             // Initialize PWM to a value for low RPM
volatile unsigned int pulseCount = 0; // Stores pulse count from feedback pin
unsigned long lastRPMMillis = 0; // Timer for RPM calculation
int currentRPM = 0;          // Current RPM calculated from feedback

// Error detection variables
bool errorState = false;         // Flag to indicate error state
unsigned long rpmZeroStartTime = 0;  // Time when RPM was detected as 0
const unsigned long errorTimeout = 20000;  // 20 seconds for error detection

// Debounce timing for encoder and button
unsigned long lastEncoderTime = 0;
const unsigned long encoderDebounceDelay = 5;
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 200;

// Multiplexing control
unsigned long lastRefreshTime = 0;
int currentDigit = 0;

// Motor control flag
bool motorRunning = false;  // Motor is not running by default

// Display control flag
bool showOn = false;
unsigned long onDisplayTime = 0;

// Function declarations for seven-segment display
void displayDigit(int digit, int position);
void displayRPM(int rpm);
void displayOFF();
void displayON();
void displayErr();
void display_0();
void display_1();
void display_2();
void display_3();
void display_4();
void display_5();
void display_6();
void display_7();
void display_8();
void display_9();
void display_O();
void display_F();
void display_n();
void display_E();
void display_r();

// Interrupt Service Routine (ISR) for encoder to adjust RPM
void encoderISR() {
  if ((millis() - lastEncoderTime) > encoderDebounceDelay) {
    if (motorRunning) {
      if (digitalRead(encoderPinA) != digitalRead(encoderPinB)) {
        desiredRPM -= 50;
      } else {
        desiredRPM += 50;
      }
      desiredRPM = constrain(desiredRPM, 0, 3950);

      adjustingRPM = true;
      allowRampUpdate = false;
      lastEncoderTime = millis();
      lastEncoderMoveTime = millis();

      Serial.print("Set RPM: ");
      Serial.println(desiredRPM);
    }
  }
}


// Interrupt Service Routine (ISR) to count pulses from the feedback pin
void countPulse() {
  pulseCount++;  // Increment pulse count for each feedback signal
}

// Setup function
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  //EEPROM Initialize and Data
  EEPROM.get(EEPROM_RPM_ADDR, desiredRPM);
  desiredRPM = constrain(desiredRPM, 0, 3950);  // Safety clamp

  Serial.print("Loaded RPM from EEPROM: ");
  Serial.println(desiredRPM);


  // Set motor control pins
  pinMode(pwmPin, INPUT);       // Disable output
  displayOFF();                 // Show OFF state immediately
  analogWrite(pwmPin, 255);     // Fully off (255 for inverted PWM logic)
  pinMode(dirPin, OUTPUT);
  pinMode(feedbackPin, INPUT_PULLUP);

  // Set seven-segment control pins
  pinMode(pinA, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinD, OUTPUT);
  pinMode(pinE, OUTPUT);
  pinMode(pinF, OUTPUT);
  pinMode(pinG, OUTPUT);
  
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);

  // Set rotary encoder pins
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  pinMode(encoderButtonPin, INPUT_PULLUP);

  // Attach interrupt for encoder
  attachInterrupt(digitalPinToInterrupt(encoderPinA), encoderISR, CHANGE);
  
  // Attach interrupt for feedback signal
  attachInterrupt(digitalPinToInterrupt(feedbackPin), countPulse, RISING);

  // Set initial motor direction
  digitalWrite(dirPin, LOW); // CCW direction initially
}

void loop() {
  // Toggle motor via encoder button
  if (digitalRead(encoderButtonPin) == LOW && (millis() - lastButtonPressTime) > debounceDelay) {
    motorRunning = !motorRunning;
    lastButtonPressTime = millis();

    if (motorRunning) {
      Serial.println("Motor ON");
      showOn = true;
      onDisplayTime = millis();
      rampUP = 255;
      allowRampUpdate = false;
    } else {
      Serial.println("Motor OFF");
      analogWrite(pwmPin, 255);
      pinMode(pwmPin, INPUT);
      displayOFF();
      errorState = false;
      rpmZeroStartTime = 0;
    }
  }

  // Handle ON display
  if (motorRunning && !errorState) {
    if (showOn) {
      displayON();
      if (millis() - onDisplayTime >= 3000) {
        showOn = false;
        allowRampUpdate = true;
      }
    } else {
      controlMotorSpeed();  // Allow ramp logic
    }
  }

  // Update current RPM once per second
  calculateRPM();

  // Auto-allow ramping after 3s idle
  if (adjustingRPM && (millis() - lastEncoderMoveTime > 1000)) {
    adjustingRPM = false;
    allowRampUpdate = true;
  }
  // Save desiredRPM to EEPROM if it changed, and 2s have passed
  if (desiredRPM != lastSavedRPM && millis() - lastEncoderMoveTime > eepromWriteDelay) {
    EEPROM.put(EEPROM_RPM_ADDR, desiredRPM);
    lastSavedRPM = desiredRPM;
    Serial.print("EEPROM updated with RPM: ");
    Serial.println(desiredRPM);
  }


  // Error detection: motor stuck
  if (motorRunning && !errorState) {
    if (currentRPM == 0) {
      if (rpmZeroStartTime == 0) {
        rpmZeroStartTime = millis();
      } else if (millis() - rpmZeroStartTime >= errorTimeout) {
        errorState = true;
        Serial.println("Error: Motor might be jammed!");
        analogWrite(pwmPin, 255);
        pinMode(pwmPin, INPUT);
      }
    } else {
      rpmZeroStartTime = 0;
    }
  }

// DISPLAY HANDLING
if (showOn) {
  displayON();  // Skip all other display logic while "ON" is shown
} else {
  if (!motorRunning) {
    displayOFF();
  } else if (errorState) {
    displayErr();
  } else if (adjustingRPM) {
    displayRPM(desiredRPM);
  } else {
    static bool toggle = false;
    static unsigned long lastToggle = 0;
    unsigned long interval = toggle ? 3000 : 1000;  // false = desired, true = current

    if (millis() - lastToggle >= interval) {
      toggle = !toggle;
      lastToggle = millis();
    }

    if (toggle) {
      displayRPM(currentRPM);
    } else {
      displayRPM(desiredRPM);
    }
  }
  }


  // SERIAL MONITOR OUTPUT
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.print("Current RPM: ");
    Serial.print(currentRPM);
    Serial.print(" | Set RPM: ");
    Serial.print(desiredRPM);
    Serial.print(" | PWM: ");
    Serial.println(rampUP);
  }
}



int rpmToPwm(int rpm) {
  if (rpm <= 300) return 255;
  else if (rpm <= 350) return 240;
  else if (rpm <= 400) return 238;
  else if (rpm <= 450) return 236;
  else if (rpm <= 500) return 233;
  else if (rpm <= 550) return 230;
  else if (rpm <= 600) return 228;
  else if (rpm <= 650) return 225;
  else if (rpm <= 700) return 222;
  else if (rpm <= 750) return 220;
  else if (rpm <= 800) return 217;
  else if (rpm <= 850) return 214;
  else if (rpm <= 900) return 212;
  else if (rpm <= 950) return 210;
  else if (rpm <= 1000) return 207;
  else if (rpm <= 1050) return 205;
  else if (rpm <= 1100) return 203;
  else if (rpm <= 1150) return 200;
  else if (rpm <= 1200) return 198;   
  else if (rpm <= 1250) return 196;
  else if (rpm <= 1300) return 193;
  else if (rpm <= 1350) return 191;
  else if (rpm <= 1400) return 188;
  else if (rpm <= 1450) return 185;
  else if (rpm <= 1500) return 182;
  else if (rpm <= 1550) return 179;
  else if (rpm <= 1600) return 176;
  else if (rpm <= 1650) return 173;
  else if (rpm <= 1700) return 170;
  else if (rpm <= 1750) return 167;
  else if (rpm <= 1800) return 164;
  else if (rpm <= 1850) return 161;
  else if (rpm <= 1900) return 158;
  else if (rpm <= 1950) return 155;
  else if (rpm <= 2000) return 151;
  else if (rpm <= 2050) return 149;
  else if (rpm <= 2100) return 145;
  else if (rpm <= 2150) return 141;
  else if (rpm <= 2200) return 138;
  else if (rpm <= 2250) return 135;
  else if (rpm <= 2300) return 130;
  else if (rpm <= 2350) return 127;
  else if (rpm <= 2400) return 123;
  else if (rpm <= 2450) return 119;
  else if (rpm <= 2500) return 115;
  else if (rpm <= 2550) return 111;
  else if (rpm <= 2600) return 107;
  else if (rpm <= 2650) return 103;
  else if (rpm <= 2700) return 99;
  else if (rpm <= 2750) return 94;
  else if (rpm <= 2800) return 90;
  else if (rpm <= 2850) return 85;
  else if (rpm <= 2900) return 81;
  else if (rpm <= 2950) return 76;
  else if (rpm <= 3000) return 72;
  else if (rpm <= 3050) return 67;
  else if (rpm <= 3100) return 63;
  else if (rpm <= 3150) return 58;
  else if (rpm <= 3200) return 54;
  else if (rpm <= 3250) return 49;
  else if (rpm <= 3300) return 45;
  else if (rpm <= 3350) return 40;
  else if (rpm <= 3400) return 36;
  else if (rpm <= 3450) return 32;
  else if (rpm <= 3500) return 28;
  else if (rpm <= 3550) return 23;
  else if (rpm <= 3600) return 18;
  else if (rpm <= 3650) return 14;
  else if (rpm <= 3700) return 10;
  else if (rpm <= 3750) return 5;
  else if (rpm <= 3800) return 1;
  else if (rpm <= 3950) return 0;

  else return 0;  
}



//Function to gradually adjust PWM to the target value using ramping
void controlMotorSpeed() {
  desiredPWM = rpmToPwm(desiredRPM);

  if (!motorRunning) {
    analogWrite(pwmPin, 255);
    motorOff();
    return;
  }

  motorON();

  if (!allowRampUpdate) {
    analogWrite(pwmPin, rampUP);
    return;
  }

  // Only update rampUP every rampInterval ms
  if (millis() - lastRampUpdate >= rampInterval) {
    lastRampUpdate = millis();

    if (rampUP > desiredPWM) {
      rampUP -= rampStep;
      if (rampUP < desiredPWM) rampUP = desiredPWM;
    } else if (rampUP < desiredPWM) {
      rampUP += rampStep;
      if (rampUP > desiredPWM) rampUP = desiredPWM;
    }
  }

  analogWrite(pwmPin, rampUP);
}



void motorON() {
  pinMode(pwmPin, OUTPUT);
}

void motorOff() {
  pinMode(pwmPin, INPUT);
}



// Function to calculate the current RPM from the feedback pin
void calculateRPM() {
  if (millis() - lastRPMMillis >= 1000) {
    lastRPMMillis = millis();
    
    // Calculate RPM based on the pulse count
    // Assuming 5 pulses per revolution
    currentRPM = (pulseCount * 60) / 5;  // Convert pulses to RPM
    pulseCount = 0;  // Reset pulse count for the next calculation
  }
}

// Function to display RPM on seven-segment display
void displayRPM(int rpm) {
  unsigned long currentTime = millis();
  if (currentTime - lastRefreshTime > 0.5) {  // Refresh every 0.5 ms per digit
    lastRefreshTime = currentTime;

    // Break down RPM value into digits
    int digit1 = (rpm / 1000) % 10;
    int digit2 = (rpm / 100) % 10;
    int digit3 = (rpm / 10) % 10;
    int digit4 = rpm % 10;

    // Display each digit in sequence
    switch (currentDigit) {
      case 0:
        displayDigit(digit1, 1);  // Thousands
        currentDigit++;
        break;
      case 1:
        displayDigit(digit2, 2);  // Hundreds
        currentDigit++;
        break;
      case 2:
        displayDigit(digit3, 3);  // Tens
        currentDigit++;
        break;
      case 3:
        displayDigit(digit4, 4);  // Units
        currentDigit = 0;
        break;
    }
  }
}
// Function to display "OFF" on the seven-segment display
void displayOFF() {
  // Display "O" on digit 1, "F" on digit 2, and "F" on digit 3
  display_O();  // Display O
  digitalWrite(D1, HIGH);  // Activate digit 1
  delay(5);  // Delay for multiplexing
  digitalWrite(D1, LOW);

  display_F();  // Display F
  digitalWrite(D2, HIGH);  // Activate digit 2
  delay(5);  // Delay for multiplexing
  digitalWrite(D2, LOW);

  display_F();  // Display F again
  digitalWrite(D3, HIGH);  // Activate digit 3
  delay(5);  // Delay for multiplexing
  digitalWrite(D3, LOW);

  // Leave the 4th digit blank
  digitalWrite(D4, LOW);
}

// Function to display "ON" on the seven-segment display
void displayON() {
  // Display "O" on digit 1 and "n" on digit 2
  display_O();  // Display O
  digitalWrite(D1, HIGH);  // Activate digit 1
  delay(5);  // Delay for multiplexing
  digitalWrite(D1, LOW);

  display_n();  // Display n
  digitalWrite(D2, HIGH);  // Activate digit 2
  delay(5);  // Delay for multiplexing
  digitalWrite(D2, LOW);

  // Leave the 3rd and 4th digits blank
  digitalWrite(D3, LOW);
  digitalWrite(D4, LOW);
}
// Function to display "Err" on the seven-segment display for an error condition
void displayErr() {
  // Display "E" on digit 1, "r" on digit 2, and "r" on digit 3
  display_E();  // Display E
  digitalWrite(D1, HIGH);  // Activate digit 1
  delay(5);  // Delay for multiplexing
  digitalWrite(D1, LOW);

  display_r();  // Display r
  digitalWrite(D2, HIGH);  // Activate digit 2
  delay(5);  // Delay for multiplexing
  digitalWrite(D2, LOW);

  display_O();  // Display O
  digitalWrite(D3, HIGH);  // Activate digit 3
  delay(5);  // Delay for multiplexing
  digitalWrite(D3, LOW);

  display_r();  // Display r again
  digitalWrite(D4, HIGH);  // Activate digit 4
  delay(5);  // Delay for multiplexing
  digitalWrite(D4, LOW);
}

// Functions for digits 0 to 9, "O", "F", "n", "E", and "r"
void display_0() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, HIGH);
}

void display_1() {
  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, HIGH);
}

void display_2() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, LOW);
}

void display_3() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, LOW);
}

void display_4() {
  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_5() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_6() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_7() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, HIGH);
}

void display_8() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_9() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, HIGH);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_O() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, HIGH);
}

void display_F() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_n() {
  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, LOW);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, LOW);
}

void display_E() {
  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinD, LOW);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, LOW);
  digitalWrite(pinG, LOW);
}

void display_r() {
  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, HIGH);
  digitalWrite(pinC, HIGH);
  digitalWrite(pinD, HIGH);
  digitalWrite(pinE, LOW);
  digitalWrite(pinF, HIGH);
  digitalWrite(pinG, LOW);
}

// Function to display a digit on the seven-segment display
void displayDigit(int digit, int position) {
  // Turn off all digits first
  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  digitalWrite(D3, LOW);
  digitalWrite(D4, LOW);

  // Select the correct digit to display
  if (position == 1) digitalWrite(D1, HIGH);
  if (position == 2) digitalWrite(D2, HIGH);
  if (position == 3) digitalWrite(D3, HIGH);
  if (position == 4) digitalWrite(D4, HIGH);

  // Display the corresponding digit
  switch (digit) {
    case 0: display_0(); break;
    case 1: display_1(); break;
    case 2: display_2(); break;
    case 3: display_3(); break;
    case 4: display_4(); break;
    case 5: display_5(); break;
    case 6: display_6(); break;
    case 7: display_7(); break;
    case 8: display_8(); break;
    case 9: display_9(); break;
  }
}
