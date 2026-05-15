#include <float.h>
#include "model.h"

void gesture_check_lstm(
    int seq_len, int features, float input_tensor[seq_len * features],
    int num_classes, float output_tensor[num_classes],
    const float means[num_classes], const float stds[num_classes],
    void(*print_results)(int best_class)
) {
    // --- INFERENCE MODE: Normalize then Predict ---
    //Serial.println("Buffer full. Running model...");
    
    // In-place normalization
    for (int i = 0; i < seq_len; i++) {
        int idx = i * features;
        for (int f = 0; f < features; f++) {
            input_tensor[idx + f] = (input_tensor[idx + f] - means[f]) / stds[f];
        }
    }

    //unsigned long start_time = millis();
    entry(input_tensor, output_tensor); 
    
    // Serial.print("Inference time (ms): ");
    // Serial.println(millis() - start_time);
    int max_ind = -1;
    float max_val = FLT_MIN;
    for (int i = 0; i < num_classes; i++) {
        if (output_tensor[i] > max_val) {
            max_ind = i;
            max_val = output_tensor[i];
        }
    }

    //int max_ind = std::distance(output_tensor, std::max_element(output_tensor, output_tensor + num_classes));
    print_results(max_ind);
}
