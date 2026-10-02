// Light-tracking servo for the Elegoo Mega 2560 R3.
//
// Two photoresistors are mounted side by side (ideally with a small divider
// wall between them). The servo turns toward whichever sensor sees more light
// until both read about the same.
//
// Wiring (each photoresistor is half of a voltage divider):
//
//   5V ---[photoresistor]---+---[10k resistor]--- GND
//                           |
//                     A0 (left) / A1 (right)
//
//   Servo: brown/black -> GND, red -> 5V, orange/yellow (signal) -> pin 9
//
// With this wiring, MORE light gives a HIGHER analogRead() value.

#include <Arduino.h>
#include <Servo.h>

const int SERVO_PIN = 9;
const int LEFT_SENSOR_PIN = A0;
const int RIGHT_SENSOR_PIN = A1;

// How different the two sensors must be before the servo moves.
// Raise this if the servo jitters back and forth; lower it for more precision.
const int DEADBAND = 20;

// Degrees moved per loop. Bigger = faster but less smooth.
const int STEP = 1;

// Delay between loop iterations in milliseconds. Smaller = faster tracking.
const int LOOP_DELAY_MS = 15;

// If the servo turns AWAY from the light, change this to true.
const bool REVERSE_DIRECTION = false;

const int SERVO_MIN = 0;
const int SERVO_MAX = 180;

Servo servo;
int position = 90;

// Average several readings to smooth out noise.
int readSensor(int pin) {
  long total = 0;
  for (int i = 0; i < 8; i++) {
    total += analogRead(pin);
  }
  return total / 8;
}

void setup() {
  Serial.begin(9600);
  servo.attach(SERVO_PIN);
  servo.write(position);
  delay(500);
}

void loop() {
  int left = readSensor(LEFT_SENSOR_PIN);
  int right = readSensor(RIGHT_SENSOR_PIN);
  int difference = left - right;

  if (abs(difference) > DEADBAND) {
    // Positive difference means the left side is brighter.
    int direction = (difference > 0) ? STEP : -STEP;
    if (REVERSE_DIRECTION) {
      direction = -direction;
    }
    position = constrain(position + direction, SERVO_MIN, SERVO_MAX);
    servo.write(position);
  }

  Serial.print("Left: ");
  Serial.print(left);
  Serial.print("  Right: ");
  Serial.print(right);
  Serial.print("  Diff: ");
  Serial.print(difference);
  Serial.print("  Servo: ");
  Serial.println(position);

  delay(LOOP_DELAY_MS);
}
