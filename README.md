# Gesture-Recognition

Smart ring for Bluetooth device control

## Details

Technical Aspects:

    Board used: AITRIP ESP32-C3 MINI Development Board
    Accelerometer: Adafruit LSM6DS3TR-C 6-DoF Accel + Gyro IMU
    IDE: PlatformIO through VSCode
    Coding Language: C++
    Processor: ARM

Hackster.io: [Link](https://www.hackster.io/549207/gesture-ring-faba2e)

Video Demonstration: [Link](https://www.youtube.com/watch?v=mXp_kpskX5w)

## Setup Workflows

### Recording

1. Set the value of `programState` from [main.cpp](src/main.cpp#L31) to `RECORDING`
2. Change `CURRENT_GESTURE` [right below](src/main.cpp#L32) to the index of the gesture you currently wish to record
3. Build and run the program, uploading to your connected ESP32
4. Once that is running, run [csv_saver.py](exploration/csv_saver.py) in your terminal, which will start its own Serial terminal.
5. Start moving around your hand/ring to record gestures, which will automatically be saved into the folders defined in [csv_saver.py](exploration/csv_saver.py#L7)

### Averages Inference

This was the most reliable way for us to detect our gestures.

1. Run the [gesture_prep.py](exploration/gesture_prep.py) script after changing the `NUM_CLASSES` value [near the top](exploration/gesture_prep.py#L11) to the total number of gestures you recorded in the [previous step](#recording)
2. Take the produced `gesture_templates.h` header and copy it over to the [gesture_template](lib/gesture_template/) folder to replace the existing [gesture_templates.h](lib/gesture_template/gesture_templates.h) header
3. Set the value of `programState` from [main.cpp](src/main.cpp#L31) to `AVG_INFERENCE`
5. Build and run the program, uploading to your connected ESP32
6. On a BlueTooth-capable device, find `ESP32-GR-V3`, or whichever device name defined in [main.cpp](src/main.cpp#L66), and pair with it
7. Open up some media application (e.g. Spotify) and test your gestures

### LSTM Inference

For our purposes, and with our limited recordings of 50 samples per gesture, we weren't able to train a reliable model. However, here are the steps to use such a model if have more success than we did.

1. Run through the full [lstm_train.ipynb](exploration/lstm_train.ipynb) notebook, replacing label and data folders, and noting the final `model_path` for your produced LSTM onnx model
2. As the final cell of the notebook denotes, and depending on the name/path of your onnx model, you need to run this onnxsim command:
```bash
onnxsim ./model/gesture_rec_static.onnx ./model/gesture_rec_static_sim.onnx
```
3. You will then need to build and run [onnx2c](https://github.com/kraiskil/onnx2c), with `gesture_rec_static_sim.onnx` from the previous step (or whatever you changed the output name to in the `onnxsim` command) as the input. The command should look like this, assuming you copied over the onnx model to the local git directory of onnx2c:
```bash
./onnx2c gesture_rec_static_sim.onnx > model.c
```
4. Take the produced `model.c` script, and copy it over to the [lstm_model](lib/lstm_model/) folder to replace the existing [model.c](lib/lstm_model/model.c)
5. Set the value of `programState` from [main.cpp](src/main.cpp#L31) to `LSTM_INFERENCE`
6. Build and run the program, uploading to your connected ESP32
7. Open up the serial monitor, and test various gestures (BlueTooth control has not yet been configured)
