#include <Arduino.h>
#include <Wire.h>

const int sda = 21;
const int scl = 21;
const int IMUAddress = 0b1101000;

void setup() {
  // put your setup code here, to run once:
  Wire.begin(sda, scl);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  float gX = get_gs('x');
  float gY = get_gs('y');
  float gZ = get_gs('z');

  // plot data
  Serial.print(">gX:");
  Serial.println(gX);
  delay(20);
}

float get_gs(char axis) {
 /* get force along specified axis. Only 'x', 'y', or 'z' are supported.
  */
  int highAddress;
  int lowAddress;
  if (axis == 'x' || axis == 'X') {
    highAddress = 0x3B;
    lowAddress = 0x3C;
  } else if (axis == 'y' || axis == 'Y') {
    highAddress = 0x3D;
    lowAddress = 0x3E;
  } else if (axis == 'z' || axis == 'Z') {
    highAddress = 0x3F;
    lowAddress = 0x40;
  } else {throw std::invalid_argument( "received invalid axis. Only x, y, z or capital versions are accepted" );}

  Wire.beginTransmission(IMUAddress);
  Wire.write(highAddress);
  Wire.endTransmission();

  Wire.requestFrom(IMUAddress, 1);
  byte ACCEL_AXISOUT_H = Wire.read();


  Wire.beginTransmission(IMUAddress);
  Wire.write(lowAddress);
  Wire.endTransmission();

  Wire.requestFrom(IMUAddress, 1);
  byte ACCEL_AXISOUT_L = Wire.read();

  int16_t ACCEL_AXIS_RAW = ACCEL_AXISOUT_H << 8 | ACCEL_AXISOUT_L;

  float gAxis = ACCEL_AXIS_RAW / 16384.0;

  return gAxis;
}