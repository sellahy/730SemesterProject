import os
import glob
import numpy as np

# Adjust these paths if necessary
DATA_DIR = "gesture_data"
LABEL_DIR = "gesture_label"
OUTPUT_FILE = "gesture_templates.h"

# 8 classes, 100 steps, 6 features
NUM_CLASSES = 2
SEQ_LEN = 100
FEATURES = 6

def main():
    print("Finding CSV files...")
    data_files = sorted(glob.glob(os.path.join(DATA_DIR, "*.csv")))
    label_files = sorted(glob.glob(os.path.join(LABEL_DIR, "*.csv")))

    if not data_files:
        print("No CSV files found! Check your directories.")
        return

    # Dictionary to hold lists of tensors for each label
    # e.g., class_data[1] = [tensor1, tensor2, ...]
    class_data = {i: [] for i in range(NUM_CLASSES)}

    for d_file, l_file in zip(data_files, label_files):
        # Read Label
        with open(l_file, 'r') as f:
            lines = f.readlines()
            label = int(lines[1].strip()) # Skip header

        # Read Data
        # Skip header, and extract the 6 IMU columns (skipping timestamp)
        data_matrix = np.loadtxt(d_file, delimiter=',', skiprows=1, usecols=(1,2,3,4,5,6))
        
        if data_matrix.shape == (SEQ_LEN, FEATURES):
            class_data[label].append(data_matrix)
        else:
            print(f"Skipping {d_file}: wrong shape {data_matrix.shape}")

    # Calculate Averages and Write to C++ Header
    with open(OUTPUT_FILE, 'w') as f:
        f.write("// AUTO-GENERATED GESTURE TEMPLATES\n")
        f.write("#ifndef GESTURE_TEMPLATES_H\n")
        f.write("#define GESTURE_TEMPLATES_H\n\n")
        
        f.write(f"const int NUM_CLASSES = {NUM_CLASSES};\n")
        f.write(f"const int TEMPLATE_SIZE = {SEQ_LEN * FEATURES};\n\n")
        
        f.write("const float GESTURE_TEMPLATES[NUM_CLASSES][TEMPLATE_SIZE] = {\n")
        
        for class_id in range(NUM_CLASSES):
            f.write(f"  {{ // Class {class_id}\n    ")
            if len(class_data[class_id]) == 0:
                # If no data for this class, fill with 0s
                f.write("0.0, " * (SEQ_LEN * FEATURES - 1) + "0.0")
            else:
                # Stack all arrays for this class and take the mean along the sample axis (0)
                mean_matrix = np.mean(np.stack(class_data[class_id]), axis=0)
                flat_data = mean_matrix.flatten()
                
                # Format as comma-separated values
                formatted_vals = [f"{val:.4f}" for val in flat_data]
                f.write(", ".join(formatted_vals))
                
            f.write("\n  }")
            if class_id < NUM_CLASSES - 1:
                f.write(",\n")
                
        f.write("\n};\n\n")
        f.write("#endif\n")

    print(f"Success! {OUTPUT_FILE} generated.")

if __name__ == "__main__":
    main()