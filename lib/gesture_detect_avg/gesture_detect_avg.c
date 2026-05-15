#include <math.h>

void gesture_check_avg(
    int seq_len, int features, float input_tensor[seq_len * features], 
    int num_classes, int template_size, const float GESTURE_TEMPLATES[num_classes * template_size], 
    float max_match_error, void(*media_action)(int gestureClass), void(*print_results)(int gestureClass)
) {
    float best_error = 999999.0f;
    int best_class = -1;

    // Compare the recorded input_tensor against every class template
    for (int c = 0; c < num_classes; c++) {
        float current_error = 0.0f;
        
        // Calculate Sum of Absolute Differences (SAD)
        for (int i = 0; i < template_size; i++) {
            current_error += abs(input_tensor[i] - GESTURE_TEMPLATES[c * template_size + i]);
        }

        if (current_error < best_error) {
            best_error = current_error;
            best_class = c;
        }
    }

    // Serial.print("Best Match: Class "); 
    // Serial.print(best_class);
    // Serial.print(" | Error Score: ");
    // Serial.println(best_error);

    // Check if the best match is actually close enough, or just random noise
    if (best_error <= max_match_error) {
        print_results(best_class);
        media_action(best_class);
    } else {
        //Serial.println("--> UNKNOWN GESTURE (Error too high)");
    }
}
