/*
  ppm_usb_joystick.ino  --  PPM -> USB HID joystick (Arduino Leonardo)

  Decodes the PPM stream from an RC transmitter's trainer/simulator port and
  presents the first 8 channels to the PC as 8 axes of a USB game controller.

  Requires: ArduinoJoystickLibrary by Matthew Heironimus
            https://github.com/MHeironimus/ArduinoJoystickLibrary
            (download the ZIP, then Sketch > Include Library > Add .ZIP Library)

  Board:    Arduino Leonardo (ATmega32U4, native USB). Will NOT work on Uno/Nano.
  Wiring:   PPM OUT --[1k resistor]--> D3     GND --> GND
            (never connect the port's VCC pin to the Leonardo)

  Tested hardware: Radiolink AT10 II only.
*/

#include <Joystick.h>

// ---------------------------- configuration -------------------------------
#define PPM_PIN        3       // D3 = INT0 on the Leonardo
#define MAX_CH         12      // max PPM channels stored
#define NUM_AXES       8       // axes exposed to the PC (X Y Z Rx Ry Rz Rudder Throttle)
#define SYNC_US        3000    // interval longer than this = frame separator
#define MIN_PULSE_US   800     // ignore intervals outside this window (noise)
#define MAX_PULSE_US   2200

// Calibration: set these to the min/max you measured with ppm_test.ino.
#define PPM_MIN_US     1000
#define PPM_MAX_US     2000
#define AXIS_MAX       1000    // each USB axis reports 0..AXIS_MAX

// Which PPM channel feeds which USB axis (0-based). Edit to reorder
// (e.g. to convert AETR <-> TAER) without touching the simulator settings.
const uint8_t CH_MAP[NUM_AXES] = {0, 1, 2, 3, 4, 5, 6, 7};

// Set an entry to true to invert that axis.
const bool INVERT[NUM_AXES] = {false, false, false, false, false, false, false, false};
// ---------------------------------------------------------------------------

// 8 axes, no buttons, no hat switches
Joystick_ Joy(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_GAMEPAD,
              0, 0,
              true, true, true, true, true, true, true, true,
              false, false, false);

volatile uint16_t work[MAX_CH];    // frame being received
volatile uint16_t ready[MAX_CH];   // last complete frame
volatile uint8_t  workIdx    = 0;
volatile uint8_t  readyCount = 0;
volatile uint32_t lastEdge   = 0;

void ppmISR() {
  uint32_t now = micros();
  uint32_t d   = now - lastEdge;   // time between rising edges
  lastEdge = now;

  if (d > SYNC_US) {               // sync gap -> previous frame complete
    if (workIdx >= 4) {
      for (uint8_t i = 0; i < workIdx; i++) ready[i] = work[i];
      readyCount = workIdx;
    }
    workIdx = 0;
  } else if (d >= MIN_PULSE_US && d <= MAX_PULSE_US && workIdx < MAX_CH) {
    work[workIdx++] = d;           // channel value in microseconds
  }
}

void setAxis(uint8_t axis, int16_t v) {
  switch (axis) {
    case 0: Joy.setXAxis(v);    break;
    case 1: Joy.setYAxis(v);    break;
    case 2: Joy.setZAxis(v);    break;
    case 3: Joy.setRxAxis(v);   break;
    case 4: Joy.setRyAxis(v);   break;
    case 5: Joy.setRzAxis(v);   break;
    case 6: Joy.setRudder(v);   break;
    case 7: Joy.setThrottle(v); break;
  }
}

int16_t scaleChannel(uint16_t us) {
  int32_t v = constrain((int32_t)us, PPM_MIN_US, PPM_MAX_US);
  return (int16_t)map(v, PPM_MIN_US, PPM_MAX_US, 0, AXIS_MAX);
}

void setup() {
  Joy.setXAxisRange(0, AXIS_MAX);
  Joy.setYAxisRange(0, AXIS_MAX);
  Joy.setZAxisRange(0, AXIS_MAX);
  Joy.setRxAxisRange(0, AXIS_MAX);
  Joy.setRyAxisRange(0, AXIS_MAX);
  Joy.setRzAxisRange(0, AXIS_MAX);
  Joy.setRudderRange(0, AXIS_MAX);
  Joy.setThrottleRange(0, AXIS_MAX);
  Joy.begin(false);                // false = we call sendState() ourselves

  pinMode(PPM_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PPM_PIN), ppmISR, RISING);
  // If the decoded values look wrong, try FALLING instead of RISING.
}

void loop() {
  uint16_t snap[MAX_CH];
  uint8_t  n;

  noInterrupts();                  // 16-bit reads are not atomic on AVR
  n = readyCount;
  for (uint8_t i = 0; i < n; i++) snap[i] = ready[i];
  interrupts();

  if (n > 0) {                     // no signal yet -> leave the last report as is
    for (uint8_t a = 0; a < NUM_AXES; a++) {
      uint8_t ch = CH_MAP[a];
      int16_t v  = (ch < n) ? scaleChannel(snap[ch]) : AXIS_MAX / 2;
      if (INVERT[a]) v = AXIS_MAX - v;
      setAxis(a, v);
    }
    Joy.sendState();
  }

  delay(10);                       // ~100 reports per second
}
