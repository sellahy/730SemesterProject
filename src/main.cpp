#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <BleKeyboard.h>
#include <BLEDevice.h> 
#include <BLESecurity.h>
#include "gesture_templates.h"

bool mooment(float, float, float);
void read_imu_data(float &, float &, float &, float &, float &, float &);
void performMediaAction(int);
void typeStringSafely(const char*);

#define LSM6DS3_ADDR 0x6A
#define SEQ_LEN 100
#define FEATURES 6
#define MOVEMENT_THRESHOLD 5.0f 
#define PRE_RECORD_STEPS 20

const float MAX_MATCH_ERROR = 2000.0f; 

const unsigned long COOLDOWN_MS = 1000; 
const float GRAVITY_ACCEL = 9.80665;
const float GRYO_LIMIT = 4.3633;
const unsigned long SAMPLE_INTERVAL_MS = 28; 

const int SDA_PIN = 8;
const int SCL_PIN = 9;

enum SystemState {
  STATE_IDLE,
  STATE_RECORDING,
  STATE_COOLDOWN
};

SystemState currentState;
float input_tensor[SEQ_LEN * FEATURES];
unsigned long last_sample_time;

int sample_count;
unsigned long cooldown_start;

float ring_buffer[PRE_RECORD_STEPS * FEATURES];
int ring_index = 0;
bool ring_full = false;

// Bluetooth HID media controller
BleKeyboard bleKeyboard("ESP32-GR-V3", "GestureRing", 100);

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN); 
  delay(100);
  currentState = STATE_IDLE;
  sample_count = 0;

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
  
  Serial.println("==== TEMPLATE MATCHING MODE ENABLED ====");
  last_sample_time = millis();

  bleKeyboard.begin();

  BLESecurity *pSecurity = new BLESecurity();
  
  // Set authentication mode to bond without Man-In-The-Middle (MITM) protection
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  // Tell the OS this device has no screen or keyboard for a PIN
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  // Set the encryption key requirements
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  Serial.println("BLE HID Ready");
  Serial.println("Pair with: ESP32-Gesture-Ring");
}

void loop() {
    unsigned long current_time = millis();

    if (current_time - last_sample_time >= SAMPLE_INTERVAL_MS) {
      last_sample_time += SAMPLE_INTERVAL_MS;

      float ax, ay, az, gx, gy, gz;
      read_imu_data(ax, ay, az, gx, gy, gz);

      switch (currentState) {
          
        case STATE_IDLE: {
          // Store RAW data in the ring buffer
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
            
            // === TEMPLATE MATCHING ALGORITHM ===
            float best_error = 999999.0f;
            int best_class = -1;

            // Compare the recorded input_tensor against every class template
            for (int c = 0; c < NUM_CLASSES; c++) {
              float current_error = 0.0f;
              
              // Calculate Sum of Absolute Differences (SAD)
              for (int i = 0; i < TEMPLATE_SIZE; i++) {
                current_error += abs(input_tensor[i] - GESTURE_TEMPLATES[c][i]);
              }

              if (current_error < best_error) {
                best_error = current_error;
                best_class = c;
              }
            }

            Serial.print("Best Match: Class "); 
            Serial.print(best_class);
            Serial.print(" | Error Score: ");
            Serial.println(best_error);

            // Check if the best match is actually close enough, or just random noise
            if (best_error <= MAX_MATCH_ERROR) {
              switch (best_class) {
                case 0: Serial.println("--> TAP"); break;
                case 1: Serial.println("--> CRANK_RIGHT"); break;
                case 2: Serial.println("--> CRANK_LEFT"); break;
                case 3: Serial.println("--> SWIPE_LEFT"); break;
                case 4: Serial.println("--> SWIPE_RIGHT"); break;
                case 5: Serial.println("--> CIRCLE_RIGHT"); break;
              }
              performMediaAction(best_class);
            } else {
              Serial.println("--> UNKNOWN GESTURE (Error too high)");
            }
            
            currentState = STATE_COOLDOWN;
            cooldown_start = millis();
          }
          break;
        }
          
        case STATE_COOLDOWN: {
          if (millis() - cooldown_start > COOLDOWN_MS) {
            ring_index = 0;
            ring_full = false; 
            currentState = STATE_IDLE;
          }
          break;
        }
      }
    }

    vTaskDelay(1);
}

void performMediaAction(int gestureClass) {
  if (!bleKeyboard.isConnected()) {
    Serial.println("Bluetooth HID not connected.");
    return;
  }

  switch (gestureClass) {
    case 0:
      Serial.println("Action: Play/Pause");
      bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
      break;
    
    case 1:
      Serial.println("Action: Volume Up");
      bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      break;

    case 2:
      Serial.println("Action: Volume Down");
      bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      break;
    
    case 3:
      Serial.println("Action: Previous Track");
      bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
      break;
    
    case 4:
      Serial.println("Action: Next Track");
      bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
      break;
    
    case 5:
      Serial.println("Action: Visit Site");
      // 1. Press Windows Key + R to open the "Run" dialog
      bleKeyboard.press(KEY_LEFT_GUI);
      delay(50);
      bleKeyboard.releaseAll();
      delay(300); // Give Windows time to open the dialog box
      
      // 2. Type the URL
      typeStringSafely("www.youtube.com/watch?v=LqvPVwPcBp0&autoplay=1");
      delay(500);
      
      // 3. Hit Enter to launch the browser
      bleKeyboard.write(KEY_RETURN);
      break;

    default:
      Serial.println("No media action for this gesture.");
      break;
  }
}

// Types a string slowly to prevent BLE buffer overflows
void typeStringSafely(const char* text) {
  for (int i = 0; i < strlen(text); i++) {
    bleKeyboard.print(text[i]);
    
    // Give the BLE radio 20ms to transmit the packet and clear the buffer.
    delay(20); 
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
