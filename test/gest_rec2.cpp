#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <algorithm>

extern "C" {
  #include "model.h"
}

bool mooment(float, float, float);
void read_imu_data(float &, float &, float &, float &, float &, float &);

#define LSM6DS3_ADDR 0x6A

// --- DATA COLLECTION CONFIGURATION ---
const bool TRAINING = true;        // Set to false to run the AI model
const int CURRENT_GESTURE = 2;     // Change this before resetting the board for a new gesture

#define BATCH_SIZE 1
#define SEQ_LEN 100
#define FEATURES 6
#define OUTPUT_CLASSES 8 
#define MOVEMENT_THRESHOLD 5.0f 

#define PRE_RECORD_STEPS 20

const unsigned long COOLDOWN_MS = 1000; 
const float GRAVITY_ACCEL = 9.80665;
const float GRYO_LIMIT = 4.3633;
const unsigned long SAMPLE_INTERVAL_MS = 28; 

// (These will be updated once you train your own model!)
const float MEANS[6] = {-0.7395, 4.8429, 3.3637, 0.0850, 0.0463, 0.0117};
const float STDS[6]  = {5.7835, 3.3225, 4.3546, 0.7242, 0.9477, 0.5873};

const int SDA_PIN = 8;
const int SCL_PIN = 9;

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

float ring_buffer[PRE_RECORD_STEPS * FEATURES];
int ring_index = 0;
bool ring_full = false;

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN); 
  delay(100);
  currentState = STATE_IDLE;
  sample_count = 0;
  cooldown_start = 0;

  Serial.begin(115200);
  while (!Serial);

  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x10);
  Wire.write(0x30);
  Wire.endTransmission();

  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x11);
  Wire.write(0x30);
  Wire.endTransmission();

  memset(input_tensor, 0, sizeof(input_tensor));
  memset(ring_buffer, 0, sizeof(ring_buffer));
  
  if(TRAINING) {
      Serial.println("==== TRAINING MODE ENABLED ====");
      Serial.print("Recording gesture ID: ");
      Serial.println(CURRENT_GESTURE);
  } else {
      Serial.println("==== INFERENCE MODE ENABLED ====");
  }

  last_sample_time = millis();
}

void loop() {
    unsigned long current_time = millis();

    if (current_time - last_sample_time >= SAMPLE_INTERVAL_MS) {
      last_sample_time += SAMPLE_INTERVAL_MS;

      float ax, ay, az, gx, gy, gz;
      read_imu_data(ax, ay, az, gx, gy, gz);

      switch (currentState) {
          
          case STATE_IDLE: {
            // Store RAW data in the ring buffer, not normalized data!
            float raw_vals[6] = {ax, ay, az, gx, gy, gz};

            for(int i = 0; i < FEATURES; i++) {
                ring_buffer[(ring_index * FEATURES) + i] = raw_vals[i];
            }
            
            ring_index++;
            if (ring_index >= PRE_RECORD_STEPS) {
                ring_index = 0;
                ring_full = true; 
            }

            if (mooment(ax, ay, az)) {
              Serial.println("Motion detected! Recording...");
              
              int steps_to_copy = ring_full ? PRE_RECORD_STEPS : ring_index;
              int start_idx = ring_full ? ring_index : 0; 
              
              for (int i = 0; i < steps_to_copy; i++) {
                  int src_idx = ((start_idx + i) % PRE_RECORD_STEPS) * FEATURES;
                  int dst_idx = i * FEATURES;
                  for(int f = 0; f < FEATURES; f++) {
                      input_tensor[dst_idx + f] = ring_buffer[src_idx + f];
                  }
              }

              sample_count = steps_to_copy; 
              currentState = STATE_RECORDING;
            }
            break;
          }

          case STATE_RECORDING: {
            int base_index = sample_count * FEATURES;
            
            // Store RAW data
            input_tensor[base_index + 0] = ax;
            input_tensor[base_index + 1] = ay;
            input_tensor[base_index + 2] = az;
            input_tensor[base_index + 3] = gx;
            input_tensor[base_index + 4] = gy;
            input_tensor[base_index + 5] = gz;

            sample_count++;

            if (sample_count >= SEQ_LEN) {
              
              if (TRAINING) {
                  // --- TRAINING MODE: Print CSV formats to Serial ---
                  Serial.println("=== START DATA CSV ===");
                  // NOTE: "accleration" is kept misspelled to exactly match Kaggle's CSV headers
                  Serial.println("timestamp,Imu0_linear_accleration_x,Imu0_linear_accleration_y,Imu0_linear_accleration_z,Imu0_angular_velocity_x,Imu0_angular_velocity_y,Imu0_angular_velocity_z");
                  
                  // Calculate a fake starting timestamp so the ms gaps look authentic
                  unsigned long base_time = millis() - (SEQ_LEN * SAMPLE_INTERVAL_MS);
                  
                  for (int i = 0; i < SEQ_LEN; i++) {
                      int idx = i * FEATURES;
                      Serial.print((base_time + (i * SAMPLE_INTERVAL_MS)) / 1000.0, 6); Serial.print(",");
                      Serial.print(input_tensor[idx + 0], 4); Serial.print(",");
                      Serial.print(input_tensor[idx + 1], 4); Serial.print(",");
                      Serial.print(input_tensor[idx + 2], 4); Serial.print(",");
                      Serial.print(input_tensor[idx + 3], 4); Serial.print(",");
                      Serial.print(input_tensor[idx + 4], 4); Serial.print(",");
                      Serial.println(input_tensor[idx + 5], 4);
                  }
                  Serial.println("=== END DATA CSV ===");
                  
                  Serial.println("=== START LABEL CSV ===");
                  Serial.println("label");
                  Serial.println(CURRENT_GESTURE);
                  Serial.println("=== END LABEL CSV ===");
                  
                  Serial.println("Recording complete. Waiting for cooldown...");

              } else {
                  // --- INFERENCE MODE: Normalize then Predict ---
                  Serial.println("Buffer full. Running model...");
                  
                  // In-place normalization
                  for (int i = 0; i < SEQ_LEN; i++) {
                      int idx = i * FEATURES;
                      for (int f = 0; f < FEATURES; f++) {
                          input_tensor[idx + f] = (input_tensor[idx + f] - MEANS[f]) / STDS[f];
                      }
                  }

                  unsigned long start_time = millis();
                  entry(input_tensor, output_tensor); 
                  
                  Serial.print("Inference time (ms): ");
                  Serial.println(millis() - start_time);

                  int max_ind = std::distance(output_tensor, std::max_element(output_tensor, output_tensor + BATCH_SIZE * OUTPUT_CLASSES));
                  switch (max_ind) {
                    case 0: Serial.println("STATIC"); break;
                    case 1: Serial.println("SLIDE_UP"); break;
                    case 2: Serial.println("SLIDE_DOWN"); break;
                    case 3: Serial.println("SLIDE_LEFT"); break;
                    case 4: Serial.println("SLIDE_RIGHT"); break;
                    case 5: Serial.println("RELEASE"); break;
                    case 6: Serial.println("GRASP"); break;
                    case 7: Serial.println("NONE"); break;
                  }
              }
              
              currentState = STATE_COOLDOWN;
              cooldown_start = millis();
            }
            break;
          }
          
          case STATE_COOLDOWN: {
              if (millis() - cooldown_start > COOLDOWN_MS) {
                Serial.println("Cooldown finished. Ready for next.");
                ring_index = 0;
                ring_full = false; 
                currentState = STATE_IDLE;
              }
              break;
          }
      }
    }
}

bool mooment(float ax, float ay, float az) {
  float magnitude = sqrt((ax * ax) + (ay * ay) + (az * az));
  float dynamic_accel = abs(magnitude - GRAVITY_ACCEL); 
  return (dynamic_accel > MOVEMENT_THRESHOLD);
}

void read_imu_data(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x22); 
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
}