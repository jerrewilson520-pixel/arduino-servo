// Car-activated traffic light for the Elegoo Mega 2560 R3.
//
// The light rests on RED. When the ultrasonic sensor sees a "car" (your hand
// or a toy car) close by, the light turns GREEN and the passive buzzer plays a
// melody. Green stays on while the car is there (between MIN_GREEN_MS and
// MAX_GREEN_MS), then goes YELLOW, then back to RED.
//
// Nothing here uses delay(), so the sensor keeps checking for cars and the
// lights keep changing while the melody plays.
//
// Wiring:
//   Red LED:    pin 2 -> 220 ohm resistor -> LED long leg; LED short leg -> GND
//   Yellow LED: pin 3 -> 220 ohm resistor -> LED long leg; LED short leg -> GND
//   Green LED:  pin 4 -> 220 ohm resistor -> LED long leg; LED short leg -> GND
//   HC-SR04:    VCC -> 5V, Trig -> pin 6, Echo -> pin 7, GND -> GND
//   Passive buzzer: + leg -> pin 8, other leg -> GND

#include <Arduino.h>
#include "notes.h"

const int RED_PIN = 2;
const int YELLOW_PIN = 3;
const int GREEN_PIN = 4;
const int TRIG_PIN = 6;
const int ECHO_PIN = 7;
const int BUZZER_PIN = 8;

// A "car" is anything closer than this.
const int CAR_DISTANCE_CM = 10;

// Readings in a row needed before we believe a car arrived or left.
// Stops one bad reading from flipping the light.
const int READINGS_TO_CONFIRM = 3;

// The HC-SR04 needs about 60 ms between pings so echoes don't overlap.
const unsigned long SENSOR_INTERVAL_MS = 60;

// Give up listening for an echo after this long. 12000 us is about 2 m,
// and a short timeout keeps the melody timing steady.
const unsigned long ECHO_TIMEOUT_US = 12000;

const unsigned long MIN_RED_MS = 3000;     // red always lasts at least this long
const unsigned long MIN_GREEN_MS = 5000;   // green always lasts at least this long
const unsigned long MAX_GREEN_MS = 15000;  // green never lasts longer than this
const unsigned long YELLOW_MS = 2000;

// true = repeat the melody for as long as the light is green.
const bool LOOP_MELODY = true;

// ---------------------------------------------------------------------------
// Melody
//
// PLACEHOLDER TUNE: replace it with the real one later.
// Each line is {note, length}. Lengths: 1 = whole, 2 = half, 4 = quarter,
// 8 = eighth, 16 = sixteenth. A negative length is dotted (1.5x as long),
// so -4 is a dotted quarter. Use REST for silence.
// ---------------------------------------------------------------------------
const int TEMPO_BPM = 170;

struct Note {
  int pitch;
  int length;
};

const Note MELODY[] = {
  {NOTE_E5, 8}, {NOTE_G5, 8}, {NOTE_A5, 8}, {NOTE_G5, 8},
  {NOTE_E5, 8}, {NOTE_D5, 8}, {NOTE_C5, 4},
  {NOTE_D5, 8}, {NOTE_E5, 8}, {NOTE_G5, 8}, {NOTE_E5, 8},
  {NOTE_D5, -4}, {REST, 8},
  {NOTE_E5, 8}, {NOTE_G5, 8}, {NOTE_A5, 8}, {NOTE_C6, 8},
  {NOTE_B5, 8}, {NOTE_A5, 8}, {NOTE_G5, 4},
  {NOTE_A5, 8}, {NOTE_G5, 8}, {NOTE_E5, 8}, {NOTE_D5, 8},
  {NOTE_C5, -4}, {REST, 8},
};

const int MELODY_LENGTH = sizeof(MELODY) / sizeof(MELODY[0]);
const unsigned long WHOLE_NOTE_MS = (60000UL * 4) / TEMPO_BPM;

bool melodyPlaying = false;
int melodyIndex = 0;
unsigned long noteStartedAt = 0;
unsigned long noteLengthMs = 0;

void startMelody() {
  melodyPlaying = true;
  melodyIndex = 0;
  noteLengthMs = 0;  // makes updateMelody() start the first note right away
  noteStartedAt = millis();
}

void stopMelody() {
  melodyPlaying = false;
  noTone(BUZZER_PIN);
}

// Call often. Starts the next note when the current one is finished.
void updateMelody() {
  if (!melodyPlaying || millis() - noteStartedAt < noteLengthMs) {
    return;
  }

  if (melodyIndex >= MELODY_LENGTH) {
    if (!LOOP_MELODY) {
      stopMelody();
      return;
    }
    melodyIndex = 0;
  }

  Note note = MELODY[melodyIndex++];
  noteLengthMs = WHOLE_NOTE_MS / abs(note.length);
  if (note.length < 0) {
    noteLengthMs = noteLengthMs * 3 / 2;
  }
  noteStartedAt = millis();

  if (note.pitch == REST) {
    noTone(BUZZER_PIN);
  } else {
    // Play for 90% of the length so repeated notes don't blur together.
    tone(BUZZER_PIN, note.pitch, noteLengthMs * 9 / 10);
  }
}

// ---------------------------------------------------------------------------
// Car sensor
// ---------------------------------------------------------------------------
bool carPresent = false;
int confirmCount = 0;
unsigned long lastPingAt = 0;
long lastDistanceCm = -1;

// Returns distance in cm, or -1 if nothing is in range.
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long echoUs = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (echoUs == 0) {
    return -1;
  }
  return echoUs / 58;
}

// Call often. Pings the sensor every SENSOR_INTERVAL_MS and updates carPresent.
void updateCarSensor() {
  if (millis() - lastPingAt < SENSOR_INTERVAL_MS) {
    return;
  }
  lastPingAt = millis();

  lastDistanceCm = readDistanceCm();
  bool seesCar = lastDistanceCm > 0 && lastDistanceCm <= CAR_DISTANCE_CM;

  if (seesCar == carPresent) {
    confirmCount = 0;
    return;
  }

  confirmCount++;
  if (confirmCount >= READINGS_TO_CONFIRM) {
    carPresent = seesCar;
    confirmCount = 0;
    Serial.println(carPresent ? "Car arrived" : "Car left");
  }
}

// ---------------------------------------------------------------------------
// Traffic light
// ---------------------------------------------------------------------------
enum LightState { RED, GREEN, YELLOW };

LightState state = RED;
unsigned long stateStartedAt = 0;

void showLight(LightState light) {
  digitalWrite(RED_PIN, light == RED ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, light == YELLOW ? HIGH : LOW);
  digitalWrite(GREEN_PIN, light == GREEN ? HIGH : LOW);
}

void changeState(LightState newState) {
  state = newState;
  stateStartedAt = millis();
  showLight(newState);

  if (newState == GREEN) {
    Serial.println("Light: GREEN");
    startMelody();
  } else {
    stopMelody();
    Serial.println(newState == YELLOW ? "Light: YELLOW" : "Light: RED");
  }
}

void updateLight() {
  unsigned long timeInState = millis() - stateStartedAt;

  switch (state) {
    case RED:
      if (carPresent && timeInState >= MIN_RED_MS) {
        changeState(GREEN);
      }
      break;

    case GREEN:
      if (timeInState >= MIN_GREEN_MS && (!carPresent || timeInState >= MAX_GREEN_MS)) {
        changeState(YELLOW);
      }
      break;

    case YELLOW:
      if (timeInState >= YELLOW_MS) {
        changeState(RED);
      }
      break;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  changeState(RED);
}

void loop() {
  updateCarSensor();
  updateLight();
  updateMelody();
}
