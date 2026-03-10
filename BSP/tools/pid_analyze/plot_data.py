import csv
import sys
import os

try:
    import matplotlib.pyplot as plt
    has_plot = True
except ImportError:
    has_plot = False

timestamps = []
tx_l = []
tx_r = []
tx_t = []

rx_l = []
rx_r = []
rx_t = []

with open('tools/pid_analyze/serial_bridge_node.csv', 'r') as f:
    reader = csv.reader(f)
    next(reader) # skip header
    
    last_tx_l = 0
    last_tx_r = 0
    
    for row in reader:
        if len(row) < 3:
            continue
            
        t = float(row[0])
        direction = row[1]
        msg_type = row[2]
        
        if direction == 'TX' and msg_type == 'VELOCITY':
            last_tx_l = int(row[3])
            last_tx_r = int(row[4])
            tx_l.append(last_tx_l)
            tx_r.append(last_tx_r)
            tx_t.append(t)
        elif direction == 'RX' and msg_type == 'ODOM':
            # Odom receives encoder diffs
            rx_l.append(int(row[3]))
            rx_r.append(int(row[4]))
            rx_t.append(t)

print(f"Parsed {len(tx_t)} TX messages and {len(rx_t)} RX ODOM messages")
print(f"TX speed ranges: L: {min(tx_l)} to {max(tx_l)}, R: {min(tx_r)} to {max(tx_r)}")
print(f"RX speed ranges: L: {min(rx_l)} to {max(rx_l)}, R: {min(rx_r)} to {max(rx_r)}")

if len(rx_l) > 10:
    avg_window = 10
    rx_smoothed_l = [sum(rx_l[i:i+avg_window])/avg_window for i in range(len(rx_l)-avg_window)]
    rx_smoothed_r = [sum(rx_r[i:i+avg_window])/avg_window for i in range(len(rx_r)-avg_window)]

    print(f"Max smoothed RX L: {max(rx_smoothed_l):.1f}, R: {max(rx_smoothed_r):.1f}")

    if has_plot:
        plt.figure(figsize=(10,6))
        # use step interpolation for TX target to show the changes
        plt.step(tx_t, tx_l, label='TX L Target', color='red', linestyle='--', where='post')
        plt.plot(rx_t, rx_l, label='RX L Actual (Raw)', color='pink', alpha=0.5)
        plt.plot(rx_t[avg_window:], rx_smoothed_l, label='RX L Actual (Smoothed)', color='darkred')
        
        plt.step(tx_t, tx_r, label='TX R Target', color='blue', linestyle='--', where='post')
        plt.plot(rx_t, rx_r, label='RX R Actual (Raw)', color='lightblue', alpha=0.5)
        plt.plot(rx_t[avg_window:], rx_smoothed_r, label='RX R Actual (Smoothed)', color='darkblue')
        
        plt.legend()
        plt.title('Motor Velocity PID Analysis')
        plt.xlabel('Time (s)')
        plt.ylabel('Velocity (ticks/dt)')
        plt.savefig('tools/pid_analyze/velocity_plot.png')
        print("Saved plot to tools/pid_analyze/velocity_plot.png")
    else:
        print("NO MATPLOTLIB!")

# Let's also look at the relationship between TX and RX at intervals
for i in range(0, len(rx_t), max(1, len(rx_t)//20)):
    t = rx_t[i]
    # find active tx at time t
    active_tx_l = 0
    active_tx_r = 0
    for j in range(len(tx_t)):
        if tx_t[j] <= t:
            active_tx_l = tx_l[j]
            active_tx_r = tx_r[j]
    print(f"Time {t:.2f}: Target ({active_tx_l}, {active_tx_r}) -> Actual ({rx_l[i]}, {rx_r[i]})")
