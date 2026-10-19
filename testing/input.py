import serial
import time

SERIAL_PORT = '/dev/ttyACM0' # update serial port
BAUD_RATE = 115200

def send_command(command_str):
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            time.sleep(0.1) # pause to allow connection
            
            # send the command followed by a newline character
            full_command = f"{command_str}\n"
            ser.write(full_command.encode('utf-8'))
            print(f"Sent: {command_str}")
            
            # read the response back from the Pico
            response = ser.readline().decode('utf-8').strip()
            if response:
                print(f"Pico Response: {response}")
                
    except serial.SerialException as e:
        print(f"Error communicating with Pico: {e}")

if __name__ == '__main__':
    print("Testing communication with Raspberry Pi Pico W...")
    pass
