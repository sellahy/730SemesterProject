import serial
import time

PORT = '/dev/ttyACM1' # Change this to your port!
BAUD = 115200

def start_listening():
    recording = False
    file = None
    current_timestamp = int(time.time())
    
    print(f"Waiting for {PORT}...")

    while True:
        try:
            # Configure port safely to prevent ESP32 resets
            ser = serial.Serial()
            ser.port = PORT
            ser.baudrate = BAUD
            ser.setDTR(False) 
            ser.setRTS(False) 
            ser.open()
            print(f"Connected to {PORT}!")

            while True:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode('utf-8', errors='replace').strip()
                    except UnicodeDecodeError:
                        continue 
                    
                    if line:
                        print(line) # Echo to terminal
                        
                        # --- Handle DATA CSV ---
                        if line == "=== START DATA CSV ===":
                            # Lock in the timestamp for this specific gesture recording
                            current_timestamp = int(time.time()) 
                            filename = f"gesture_data/gesture_data_{current_timestamp}.csv"
                            file = open(filename, "w")
                            recording = True
                            print(f"\n[Saving to {filename}...]")
                            continue
                            
                        if line == "=== END DATA CSV ===":
                            if file:
                                file.close()
                            recording = False
                            print(f"[{filename} saved!]")
                            continue

                        # --- Handle LABEL CSV ---
                        if line == "=== START LABEL CSV ===":
                            # Uses the exact same timestamp so the files match!
                            filename = f"gesture_label/gesture_label_{current_timestamp}.csv"
                            file = open(filename, "w")
                            recording = True
                            print(f"\n[Saving to {filename}...]")
                            continue
                            
                        if line == "=== END LABEL CSV ===":
                            if file:
                                file.close()
                            recording = False
                            print(f"[{filename} saved!]\n")
                            continue
                            
                        # --- Write to whichever file is currently open ---
                        if recording and file:
                            file.write(line + "\n")

        except serial.SerialException:
            if file and not file.closed:
                file.close()
                recording = False
            
            print("\n[Connection lost. Waiting for ESP32 to return...]")
            time.sleep(2) 

        except KeyboardInterrupt:
            print("\nExiting...")
            if 'file' in locals() and file and not file.closed:
                file.close()
            break

if __name__ == "__main__":
    start_listening()