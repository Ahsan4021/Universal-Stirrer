# Universal BLDC Stirrer Controller

This project is a precision **Brushless DC (BLDC) Stirrer Controller** built around an Arduino-compatible microcontroller (e.g., Mega), designed to control and monitor laboratory or industrial stirrer motors.

It features encoder-based RPM control, real-time feedback monitoring from the motor’s FG line, and a 7-segment display interface for human interaction.

---

## ⚙️ Key Features

- 🌀 **Control of BLDC motors** via PWM and direction signals
- 🎛️ **Rotary encoder input** for intuitive speed setting
- 📟 **Live RPM monitoring** via FG (frequency generator) output from the BLDC
- ⚡ **Soft start/stop with PWM ramping**
- 🔢 **4-digit multiplexed 7-segment display** shows:
  - Current RPM
  - `"ON"` during startup
  - `"OFF"` when idle
  - `"Err"` in case of motor fault
- 🧠 **Automatic error detection** when RPM = 0 for 20s
- 👆 **Encoder button toggles motor ON/OFF**
- 🖥️ Serial debugging support

---

## 🧰 Hardware Required

- Arduino Mega (or similar with multiple I/O)
- BLDC motor with **FG output** (hall feedback pulse)
- BLDC driver/controller (e.g., DRV10983, external ESC, etc.)
- Rotary encoder with push button
- 4-digit 7-segment (common cathode) display
- External power supply for motor
- Jumper wires / prototyping board

---

## 🔌 Pin Configuration (Default)

### Motor Driver
| Function     | Pin | Notes                           |
|--------------|-----|---------------------------------|
| PWM Signal   | 46  | Controls motor speed            |
| Direction    | 13  | HIGH/LOW for CW/CCW             |
| FG Feedback  | 2   | Digital input for RPM sensing   |

### Rotary Encoder
| Function   | Pin |
|------------|-----|
| Encoder A  | 18  |
| Encoder B  | 19  |
| Button     | 3   |

### 7-Segment Display (Segments)
| Segment | Pin |
|---------|-----|
| A       | A9  |
| B       | A2  |
| C       | A11 |
| D       | A12 |
| E       | A8  |
| F       | A7  |
| G       | A10 |

### 7-Segment Display (Digit Selection)
| Digit | Pin |
|-------|-----|
| D1    | A5  |
| D2    | A3  |
| D3    | A4  |
| D4    | A6  |

---

## 🧠 How It Works

- Encoder sets target speed → mapped to PWM (255 = stop, 0 = full speed)
- Direction set via a digital pin (adjustable)
- RPM calculated from FG pulse rate (e.g., 5 pulses per revolution)
- System detects stall if FG stops pulsing for 20s while "ON"
- Visual feedback is shown on a 7-segment display:
  - `"ON"` during startup
  - Actual RPM during run
  - `"OFF"` when stopped
  - `"Err"` when fault is detected

---

## 🧪 Example Behavior

- Turn encoder → RPM increases (PWM decreases)
- Press encoder button → toggles motor ON/OFF
- If motor is ON and feedback stops → shows `"Err"`

---

## 🛠️ Notes & Customization

- FG pulse/rev may differ based on your BLDC model → adjust RPM formula
- PWM logic (inverted) assumes `255 = OFF`, `0 = MAX SPEED`
- Error timeout is set to 20s by default
- Use a logic-level shifter if interfacing 3.3V FG to a 5V board

---

## 💡 Future Ideas

- Display RPM on an OLED or TFT screen
- Wireless control (via Bluetooth or Wi-Fi)
- PID-based closed-loop RPM control
- Overvoltage/overcurrent protection integration

---

## 📜 License

MIT License

---

> Developed for flexible control of BLDC-based stirrers in scientific and industrial environments, with user-friendly control and robust safety.
