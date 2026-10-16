/*
  ppm_test.ino  --  PPM signal tester (Arduino Leonardo)

  Reads the PPM stream from the transmitter's trainer/simulator port and
  prints the decoded channels to the Serial Monitor (115200 baud).

  Use this FIRST, before the joystick sketch. It tells you:
    - whether the Leonardo sees a valid PPM signal at all
    - how many channels the transmitter sends
    - the frame rate (usually ~45-50 frames per second)
    - the real min / max pulse widths of each channel (for calibration)

  Wiring:  PPM OUT --[1k resistor]--> D3     GND --> GND
           (never connect the port's VCC pin to the Leonardo)

  Tested hardware: Radiolink AT10 II only.
*/

#define PPM_PIN        3      // D3 = INT0 on the Leonardo
#define MAX_CH         12     // max channels stored
#define SYNC_US        3000   // an interval longer than this = frame separator
#define MIN_PULSE_US   800    // ignore intervals outside this window (noise)
#define MAX_PULSE_US   2200

volatile uint16_t work[MAX_CH];    // channels of the frame being received
volatile uint16_t ready[MAX_CH];   // last complete frame
volatile uint8_t  workIdx    = 0;
volatile uint8_t  readyCount = 0;
volatile uint32_t lastEdge   = 0;
volatile uint32_t frameCount = 0;

void ppmISR() {
  uint32_t now = micros();
  uint32_t d   = now - lastEdge;   // time since previous rising edge
  lastEdge = now;

  if (d > SYNC_US) {               // sync gap: previous frame is complete
    if (workIdx >= 4) {            // sanity: at least 4 channels
      for (uint8_t i = 0; i < workIdx; i++) ready[i] = work[i];
      readyCount = workIdx;
      frameCount++;
    }
    workIdx = 0;
  } else if (d >= MIN_PULSE_US && d <= MAX_PULSE_US && workIdx < MAX_CH) {
    work[workIdx++] = d;           // one channel value in microseconds
  }
}

uint16_t mn[MAX_CH], mx[MAX_CH];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}   // Leonardo: wait for USB serial

  for (uint8_t i = 0; i < MAX_CH; i++) { mn[i] = 0xFFFF; mx[i] = 0; }

  pinMode(PPM_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PPM_PIN), ppmISR, RISING);
  // If values look wrong, try FALLING instead of RISING.

  Serial.println(F("PPM tester ready. Move all sticks and switches to their extremes."));
}

void loop() {
  static uint32_t tPrint = 0, tRate = 0, tMinMax = 0, framesPrev = 0;
  static uint16_t fps = 0;
  uint32_t now = millis();

  if (now - tRate >= 1000) {                 // frames per second
    noInterrupts(); uint32_t f = frameCount; interrupts();
    fps = f - framesPrev;
    framesPrev = f;
    tRate = now;
  }

  uint16_t snap[MAX_CH];
  uint8_t  n;
  noInterrupts();                            // copy frame atomically
  n = readyCount;
  for (uint8_t i = 0; i < n; i++) snap[i] = ready[i];
  interrupts();

  for (uint8_t i = 0; i < n; i++) {          // track min / max
    if (snap[i] < mn[i]) mn[i] = snap[i];
    if (snap[i] > mx[i]) mx[i] = snap[i];
  }

  if (now - tPrint >= 100) {
    tPrint = now;
    Serial.print(F("fps=")); Serial.print(fps);
    Serial.print(F("  ch=")); Serial.print(n);
    Serial.print(F("  |"));
    for (uint8_t i = 0; i < n; i++) { Serial.print(' '); Serial.print(snap[i]); }
    Serial.println();
  }

  if (now - tMinMax >= 5000) {
    tMinMax = now;
    Serial.println(F("--- min / max per channel (us) ---"));
    for (uint8_t i = 0; i < n; i++) {
      Serial.print(F("CH")); Serial.print(i + 1); Serial.print(F(": "));
      Serial.print(mn[i]); Serial.print(F(" .. ")); Serial.println(mx[i]);
    }
    Serial.println(F("----------------------------------"));
  }
}
