import os
import glob

# --- CONFIGURATION ---
TARGET_LABEL = 1 # Change this to the gesture ID you want to delete
DATA_DIR = "gesture_data"
LABEL_DIR = "gesture_label"
# ---------------------

def main():
    print(f"Scanning for files with label: {TARGET_LABEL}...")
    
    # Find all label CSVs
    label_files = glob.glob(os.path.join(LABEL_DIR, "*.csv"))
    deleted_count = 0
    
    for label_path in label_files:
        try:
            # Read the label file
            with open(label_path, 'r') as f:
                lines = f.readlines()
                # The label is usually on the second line (index 1) after the header
                if len(lines) >= 2:
                    file_label = int(lines[1].strip())
                    
                    if file_label == TARGET_LABEL:
                        # Extract the filename without the folder path
                        base_filename = os.path.basename(label_path)
                        
                        # Reconstruct the corresponding data file path
                        # Assuming format: gesture_label_123.csv -> gesture_data_123.csv
                        # Or if they share the exact same name across different folders
                        if "label" in base_filename:
                            data_filename = base_filename.replace("label", "data")
                        else:
                            data_filename = base_filename 
                            
                        data_path = os.path.join(DATA_DIR, data_filename)
                        
                        # Delete the Label file
                        os.remove(label_path)
                        
                        # Delete the matching Data file if it exists
                        if os.path.exists(data_path):
                            os.remove(data_path)
                            
                        print(f"Deleted: {label_path} AND {data_path}")
                        deleted_count += 1
                        
        except Exception as e:
            print(f"Could not process {label_path}: {e}")

    print(f"\nCleanup complete! Deleted {deleted_count} pairs of files for gesture {TARGET_LABEL}.")

if __name__ == "__main__":
    main()
