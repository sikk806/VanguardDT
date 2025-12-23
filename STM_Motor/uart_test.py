import serial, time

ser = serial.Serial('/dev/ttyAMC0', 115200, timeout=1)

while True:
    line = ser.readline()
    if line:
        print("RX:", line.decode(errors='ignore').strip())

    ser.write(b'A')
    time.sleep(1)
