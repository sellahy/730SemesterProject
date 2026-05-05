import os
import glob

# --- CONFIGURATION ---
OLD_LABEL = 3     # The gesture ID you want to change
NEW_LABEL = 5     # What you want to change it to
LABEL_DIR = "gesture_label"
# ---------------------

def main():
    print(f"Scanning for files with label: {OLD_LABEL}...")
    
    # Find all label CSVs
    label_files = glob.glob(os.path.join(LABEL_DIR, "*.csv"))
    changed_count = 0
    
    for label_path in label_files:
        try:
            # Read the file
            with open(label_path, 'r') as f:
                lines = f.readlines()
                
            # Ensure the file has at least a header and a label
            if len(lines) >= 2:
                file_label = int(lines[1].strip())
                
                if file_label == OLD_LABEL:
                    # Modify the label line (keeping the newline character)
                    lines[1] = f"{NEW_LABEL}\n"
                    
                    # Write the changes back to the file
                    with open(label_path, 'w') as f:
                        f.writelines(lines)
                        
                    print(f"Changed: {os.path.basename(label_path)} (from {OLD_LABEL} to {NEW_LABEL})")
                    changed_count += 1
                        
        except Exception as e:
            print(f"Could not process {label_path}: {e}")

    print(f"\nUpdate complete! Changed {changed_count} files from label {OLD_LABEL} to label {NEW_LABEL}.")

if __name__ == "__main__":
    main()
