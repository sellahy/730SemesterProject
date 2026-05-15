#ifndef GESTURE_DETECT_AVG_H   /* Include guard */
#define GESTURE_DETECT_AVG_H

#ifdef __cplusplus
extern "C" {
#endif

void gesture_check_avg(int, int, float [], int, int, const float [], float, void(*)(int), void(*)(int));

#ifdef __cplusplus
}
#endif

#endif // GESTURE_DETECT_AVG_H