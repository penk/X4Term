#!/usr/bin/env python3
"""Test new X4Term features over serial."""
import serial
import time
import sys

PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem2101'
BAUD = 115200

def send(ser, s):
    ser.write(s.encode())
    ser.flush()

def pause(t=1.5):
    time.sleep(t)

def clear(ser):
    send(ser, '\033[2J\033[H')

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(0.5)

# --- Test 1: New geometric shapes ---
clear(ser)
send(ser, '--- Geometric Shapes ---\r\n')
send(ser, 'Up tri:    \u25b2  Down tri:  \u25bc\r\n')
send(ser, 'Right tri: \u25b6  Left tri:  \u25c0\r\n')
send(ser, 'Blk dia:   \u25c6  Wht dia:   \u25c7\r\n')
send(ser, 'Blk sq:    \u25a0  Wht sq:    \u25a1\r\n')
send(ser, 'Blk cir:   \u25cf  Wht cir:   \u25cb\r\n')
send(ser, 'Check:     \u2713  X mark:    \u2717\r\n')
pause()

# --- Test 2: Fixed dot leaders ---
send(ser, '\r\n--- Fixed Dot Leaders ---\r\n')
send(ser, 'One dot:   \u2024\r\n')
send(ser, 'Two dot:   \u2025\r\n')
send(ser, 'Hyph pt:   \u2027\r\n')
pause()

# --- Test 3: Diagonal lines ---
send(ser, '\r\n--- Diagonal Lines ---\r\n')
send(ser, 'Fwd slash: \u2571  Back slash: \u2572  Cross: \u2573\r\n')
pause()

# --- Test 4: Extended block elements (quadrants) ---
send(ser, '\r\n--- Quadrant Blocks ---\r\n')
send(ser, '\u2596\u2597\u2598\u259d  ')
send(ser, '\u2599\u259a\u259b\u259c  ')
send(ser, '\u259e\u259f  ')
send(ser, '\u2594\u2595\r\n')
pause()

# --- Test 5: CSI X (erase characters) ---
send(ser, '\r\n--- ECH Test ---\r\n')
send(ser, 'ABCDEFGHIJ')
send(ser, '\033[5D')     # move back 5
send(ser, '\033[3X')     # erase 3 chars at cursor
send(ser, '  <- FGH erased\r\n')
pause()

# --- Test 6: CSI s/u (save/restore cursor) ---
send(ser, '\r\n--- Save/Restore Cursor ---\r\n')
send(ser, 'Before')
send(ser, '\033[s')       # save cursor
send(ser, '\033[5C')      # move right 5
send(ser, 'MOVED')
send(ser, '\033[u')       # restore cursor
send(ser, ' OK\r\n')
pause()

# --- Test 7: Cursor hide/show ---
send(ser, '\r\n--- Cursor Hide/Show ---\r\n')
send(ser, 'Hiding cursor...')
send(ser, '\033[?25l')    # hide cursor
pause(2)
send(ser, '\033[?25h')    # show cursor
send(ser, ' visible again!\r\n')
pause()

# --- Test 8: Alternate screen buffer ---
send(ser, '\r\n--- Press any key for alt screen test ---\r\n')
send(ser, 'Main screen content here.\r\n')
pause(3)

# Switch to alt screen
send(ser, '\033[?1049h')  # enter alt screen
clear(ser)
send(ser, '=== ALTERNATE SCREEN ===\r\n\r\n')
send(ser, 'This is the alternate screen buffer.\r\n')
send(ser, 'The main screen content is preserved.\r\n')
send(ser, '\r\nReturning in 4 seconds...\r\n')
pause(4)

# Switch back to main screen
send(ser, '\033[?1049l')  # exit alt screen
pause(2)

send(ser, '\r\n--- All tests complete! ---\r\n')

ser.close()
print("Done!")
