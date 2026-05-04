#include <Arduino.h>
#include <Wire.h>
#include <math.h>

extern "C" {
  #include "model.h"
}

bool mooment(float, float, float);
void read_imu_data(float &, float &, float &, float &, float &, float &);

// I2C Address for LSM6DS3
#define LSM6DS3_ADDR 0x6A

// 2. Define the static dimensions
#define BATCH_SIZE 1
#define SEQ_LEN 100
#define FEATURES 6
#define OUTPUT_CLASSES 8 
#define MOVEMENT_THRESHOLD 7.0f // g-force above standard 1g gravity

const unsigned long COOLDOWN_MS = 1000; // 1 second ignore period after a gesture
const float GRAVITY_ACCEL = 9.80665;
const float GRYO_LIMIT = 4.3633;
const unsigned long SAMPLE_INTERVAL_MS = 28; // 35Hz.

// --- State Machine Enums ---
enum SystemState {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_COOLDOWN
};

SystemState currentState;
float input_tensor[BATCH_SIZE * SEQ_LEN * FEATURES];
float output_tensor[BATCH_SIZE * OUTPUT_CLASSES];
unsigned long last_sample_time;

int sample_count;
unsigned long cooldown_start;

void setup() {
  Wire.begin(8, 9); // SDA, SCL for C3 Super Mini
  delay(100);
  currentState = STATE_IDLE;
  sample_count = 0;
  cooldown_start = 0;
  last_sample_time = 0;

  Serial.begin(115200);
  while (!Serial);

  // Initialize accelerometer (CTRL1_XL register)
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x10); // CTRL1_XL
  Wire.write(0x30); // 1.66 kHz, +/- 2g
  Wire.endTransmission();

  // Initialize gyroscope (CTRL2_G register)
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x11); // CTRL1_XL
  Wire.write(0x30); // 1.66 kHz, +/- 250 dps
  Wire.endTransmission();

  memset(input_tensor, 0, sizeof(input_tensor));
}

void loop() {
    unsigned long current_time = millis();

    // 2. Check if enough time has passed since the last reading
    if (current_time - last_sample_time >= SAMPLE_INTERVAL_MS) {
      // 3. Reset the timer. 
      // We add the interval to the last time rather than setting it to current_time 
      // to prevent drift over time.
      last_sample_time = millis();

      // 1. Read IMU (Replace with your actual IMU read functions)
      float ax, ay, az, gx, gy, gz;
      read_imu_data(ax, ay, az, gx, gy, gz);

      // 2. State Machine Logic
      switch (currentState) {
          
          case STATE_IDLE: {
            if (mooment(ax, ay, az)) {
              Serial.println("Motion detected! Recording gesture...");
              currentState = STATE_RECORDING;
              sample_count = 0;
            }
            break;
          }

          case STATE_RECORDING: {
            // Append data to the flat buffer
            int base_index = sample_count * FEATURES;
            input_tensor[base_index + 0] = ax;
            input_tensor[base_index + 1] = ay;
            input_tensor[base_index + 2] = az;
            input_tensor[base_index + 3] = gx;
            input_tensor[base_index + 4] = gy;
            input_tensor[base_index + 5] = gz;

            sample_count++;

            // If buffer is full, run inference!
            if (sample_count >= SEQ_LEN) {
              Serial.println("Buffer full. Running model...");
              
              unsigned long start_time = millis();
              entry(input_tensor, output_tensor); // Call ONNX2C model
              
              Serial.print("Inference time (ms): ");
              Serial.println(millis() - start_time);

              // TODO: Find the max value in output_tensor to get the class ID
              Serial.print("Predictions: ");
              // for (int i = 0; i < OUTPUT_CLASSES; i++) {
              //     Serial.print(output_tensor[i], 4); 
              //     Serial.print(" ");
              // }
              int max_ind = std::distance(output_tensor, std::max_element(output_tensor, output_tensor + BATCH_SIZE * OUTPUT_CLASSES));
              switch (max_ind) {
                case 0:
                  Serial.println("STATIC");
                  break;
                case 1:
                  Serial.println("SLIDE_UP");
                  break;
                case 2:
                  Serial.println("SLIDE_DOWN");
                  break;
                case 3:
                  Serial.println("SLIDE_LEFT");
                  break;
                case 4:
                  Serial.println("SLIDE_RIGHT");
                  break;
                case 5:
                  Serial.println("RELEASE");
                  break;
                case 6:
                  Serial.println("GRASP");
                  break;
                case 7:
                  Serial.println("NONE");
                  break;
              }
              
              // Switch to cooldown so we don't immediately trigger again
              currentState = STATE_COOLDOWN;
              cooldown_start = millis();
            }
            break;
          }
          
          case STATE_COOLDOWN: {
            // Ignore all IMU data until cooldown expires
              if (millis() - cooldown_start > COOLDOWN_MS) {
                Serial.println("Cooldown finished. Ready for next gesture.");
                currentState = STATE_IDLE;
              }
              break;
          }
      }
    }
}

bool mooment(float ax, float ay, float az) {
  float magnitude = sqrt((ax * ax) + (ay * ax) + (az * ax));
  //Serial.println(magnitude);
  float dynamic_accel = abs(magnitude - GRAVITY_ACCEL); // Remove 1g of gravity
  
  if ((dynamic_accel > MOVEMENT_THRESHOLD)) {
    Serial.print("Dynamic accel: ");
    Serial.println(dynamic_accel);
  }
  return (dynamic_accel > MOVEMENT_THRESHOLD);
}

void read_imu_data(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x22); // Start reading from OUTX_L_G
  Wire.endTransmission();
  Wire.requestFrom(LSM6DS3_ADDR, 12);

  int16_t rawGx = Wire.read() | (Wire.read() << 8);
  int16_t rawGy = Wire.read() | (Wire.read() << 8);
  int16_t rawGz = Wire.read() | (Wire.read() << 8);
  int16_t rawAx = Wire.read() | (Wire.read() << 8);
  int16_t rawAy = Wire.read() | (Wire.read() << 8);
  int16_t rawAz = Wire.read() | (Wire.read() << 8);

  ax = ((float)rawAx * 0.061f / 1000.0f) * GRAVITY_ACCEL;
  ay = ((float)rawAy * 0.061f / 1000.0f) * GRAVITY_ACCEL;
  az = ((float)rawAz * 0.061f / 1000.0f) * GRAVITY_ACCEL;

  gx = constrain(((float)rawGx * 8.75f / 1000.0f) * (M_PI / 180.0), -GRYO_LIMIT, GRYO_LIMIT);
  gy = constrain(((float)rawGy * 8.75f / 1000.0f) * (M_PI / 180.0), -GRYO_LIMIT, GRYO_LIMIT);
  gz = constrain(((float)rawGz * 8.75f / 1000.0f) * (M_PI / 180.0), -GRYO_LIMIT, GRYO_LIMIT);
  // gx = ((float)rawGx * 8.75f / 1000.0f) * (M_PI / 180.0);
  // gy = ((float)rawGy * 8.75f / 1000.0f) * (M_PI / 180.0);
  // gz = ((float)rawGz * 8.75f / 1000.0f) * (M_PI / 180.0);
}
