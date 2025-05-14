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

// Rotary encoder pins for adjusting RPM
int encoderPinA = 18;
int encoderPinB = 19;
int encoderButtonPin = 3;
int lastPWMValue = 255;  // Start with a low RPM (higher PWM value)
int targetPWM = 255; 
int rampStep = 5;  
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
bool motorRunning = true;  // Motor is running by default

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
    // Only update the targetPWM if the motor is running
    if (motorRunning) {
      // Encoder direction: CW should increase RPM (decrease PWM)
      if (digitalRead(encoderPinA) != digitalRead(encoderPinB)) {
        encoderPos += 1;  // Increase encoder position (increase RPM)
      } else {
        encoderPos -= 1;  // Decrease encoder position (decrease RPM)
      }
      encoderPos = constrain(encoderPos, 0, 255);  // Constrain to valid PWM range
      targetPWM = encoderPos;  // Update the target PWM value
      lastEncoderTime = millis();
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

  // Set motor control pins
  pinMode(pwmPin, OUTPUT);
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
  // Check if the encoder button is pressed and debounce the press
  if (digitalRead(encoderButtonPin) == LOW && (millis() - lastButtonPressTime) > debounceDelay) {
  motorRunning = !motorRunning;  // Toggle motor state
  lastButtonPressTime = millis();

  if (motorRunning) {
    Serial.println("Motor ON");
    showOn = true;  // Show "ON" for 3 seconds
    

    // Initialize the PWM value to 0 and gradually increase to targetPWM
    pwmValue = 255;  // Start from 0
    onDisplayTime = millis();  // Record the time "ON" was shown
  } else {
    Serial.println("Motor OFF");

    // Stop the motor when turning off
    analogWrite(pwmPin, 255);   // Ensure motor is stopped (255 PWM means 0 RPM)
    pinMode(pwmPin, INPUT);     // Disable the PWM pin (set to INPUT)
    displayOFF();  // Show "OFF" on the display

    // Clear error state and reset error variables when motor is turned off
    errorState = false;        // Reset the error state
    rpmZeroStartTime = 0;      // Reset the RPM zero timer
  }
}

  // If motor is running, show "ON" for 3 seconds, then return to RPM display
  if (motorRunning && !errorState) {
  if (showOn) {
    displayON();  // Display "ON"
    if (millis() - onDisplayTime >= 3000) {
      showOn = false;  // After 3 seconds, stop showing "ON"
    }
  } else {
    updatePWMWithRamp();  // Gradually adjust PWM to target value
  }
}

  // Calculate and display the current RPM every second
  calculateRPM();

  // Check for error condition if the motor is running
  if (motorRunning && !errorState) {
    if (currentRPM == 0) {
      // If RPM is 0, start the timer if it's not already running
      if (rpmZeroStartTime == 0) {
        rpmZeroStartTime = millis();
      } else if (millis() - rpmZeroStartTime >= errorTimeout) {
        // If RPM is 0 for more than 20 seconds, trigger error state
        errorState = true;
        Serial.println("Error: Motor might be jammed!");
        analogWrite(pwmPin, 255);   // Ensure motor is stopped (255 PWM means 0 RPM)
        pinMode(pwmPin, INPUT);     // Disable the PWM pin (set to INPUT)
      }
    } else {
      // Reset the timer if RPM is not 0
      rpmZeroStartTime = 0;
    }
  }

  // Refresh the display
  if (!motorRunning) {
    displayOFF();  // Show "OFF" on the display
  } else if (errorState) {
    displayErr();  // Show error on the display
  } else if (!showOn) {
    displayRPM(currentRPM);  // Show the current RPM on the display
  }
}


//Function to gradually adjust PWM to the target value using ramping
void updatePWMWithRamp() {
  if (motorRunning) {
    if (pwmValue < targetPWM) {
      pwmValue += rampStep;
      if (pwmValue > targetPWM) pwmValue = targetPWM;
    } else if (pwmValue > targetPWM) {
      pwmValue -= rampStep;
      if (pwmValue < targetPWM) pwmValue = targetPWM;
    }
    analogWrite(pwmPin, pwmValue);
    // Serial.print("Current PWM: ");
    // Serial.println(pwmValue);
    delay(1/4);
  } else {
    analogWrite(pwmPin, 255); // Stop motor when not running
  }
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
