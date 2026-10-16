#include <Joystick.h>

#define PPM_PIN 3

// 8 axes: X, Y, Z, Rx, Ry, Rz, Rudder, Throttle; no buttons or hats
Joystick_ Joy(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_GAMEPAD,
              0, 0,
              true, true, true, true, true, true, true, true,
              false, false, false);

volatile uint16_t ch[12];
volatile uint8_t idx = 0, count = 0;
volatile uint32_t last = 0;

void isr() {
  uint32_t now = micros();
  uint32_t d = now - last;
  last = now;
  if (d > 3000) { count = idx; idx = 0; }   // sync gap = new frame
  else if (idx < 12) ch[idx++] = d;
}

void setup() {
  pinMode(PPM_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PPM_PIN), isr, RISING);

  Joy.setXAxisRange(0, 1000);
  Joy.setYAxisRange(0, 1000);
  Joy.setZAxisRange(0, 1000);
  Joy.setRxAxisRange(0, 1000);
  Joy.setRyAxisRange(0, 1000);
  Joy.setRzAxisRange(0, 1000);
  Joy.setRudderRange(0, 1000);
  Joy.setThrottleRange(0, 1000);
  Joy.begin(false);                          // we call sendState() ourselves
}

void loop() {
  uint16_t v[8] = {0};
  noInterrupts();                            // 16-bit reads aren't atomic on AVR
  for (uint8_t i = 0; i < 8 && i < count; i++)
    v[i] = constrain((int)ch[i] - 1000, 0, 1000);
  interrupts();

  Joy.setXAxis(v[0]);
  Joy.setYAxis(v[1]);
  Joy.setZAxis(v[2]);
  Joy.setRxAxis(v[3]);
  Joy.setRyAxis(v[4]);
  Joy.setRzAxis(v[5]);
  Joy.setRudder(v[6]);
  Joy.setThrottle(v[7]);
  Joy.sendState();
  delay(10);                                 // ~100 Hz
}