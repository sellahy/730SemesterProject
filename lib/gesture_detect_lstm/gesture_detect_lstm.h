#ifndef GESTURE_DETECT_LSTM_H   /* Include guard */
#define GESTURE_DETECT_LSTM_H

#ifdef __cplusplus
extern "C" {
#endif

void gesture_check_lstm(int, int, float [], int, float [], const float [], const float [], void(*)(int));

#ifdef __cplusplus
}
#endif

#endif // GESTURE_DETECT_LSTM_H