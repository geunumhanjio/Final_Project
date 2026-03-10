import csv
import sys

timestamps = []
tx_l = []
tx_r = []
tx_t = []

rx_l = []
rx_r = []
rx_t = []

ENCODER_PPR = 3840.0
WHEEL_CIRCUMFERENCE_MM = 204.2
ODOM_DT_S = 0.02  # 20ms 

TICK_TO_MMPS = (1.0 / ENCODER_PPR) * WHEEL_CIRCUMFERENCE_MM / ODOM_DT_S

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
            # Odom receives encoder diffs (ticks / 20ms)
            l_diff = int(row[3])
            r_diff = int(row[4])
            rx_l.append(l_diff * TICK_TO_MMPS)
            rx_r.append(r_diff * TICK_TO_MMPS)
            rx_t.append(t)

try:
    import matplotlib.pyplot as plt
    
    avg_window = 10
    rx_smoothed_l = [sum(rx_l[i:i+avg_window])/avg_window for i in range(len(rx_l)-avg_window)]
    rx_smoothed_r = [sum(rx_r[i:i+avg_window])/avg_window for i in range(len(rx_r)-avg_window)]
    
    plt.figure(figsize=(12,8))
    plt.step(tx_t, tx_l, label='TX L Target (mm/s)', color='red', linestyle='--', where='post')
    plt.plot(rx_t, rx_l, label='RX L Actual (mm/s)', color='pink', alpha=0.5)
    plt.plot(rx_t[avg_window:], rx_smoothed_l, label='RX L Actual Smoothed', color='darkred')
    
    plt.step(tx_t, tx_r, label='TX R Target (mm/s)', color='blue', linestyle='--', where='post')
    plt.plot(rx_t, rx_r, label='RX R Actual (mm/s)', color='lightblue', alpha=0.5)
    plt.plot(rx_t[avg_window:], rx_smoothed_r, label='RX R Actual Smoothed', color='darkblue')
    
    plt.legend()
    plt.title('Motor Velocity PID Analysis (Corrected Units)')
    plt.xlabel('Time (s)')
    plt.ylabel('Velocity (mm/s)')
    plt.grid(True)
    plt.savefig('tools/pid_analyze/velocity_plot_corrected.png')
    print("Saved plot to tools/pid_analyze/velocity_plot_corrected.png")
except ImportError:
    print("NO MATPLOTLIB!")

