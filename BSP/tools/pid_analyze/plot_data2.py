import csv
import sys

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

# let's find that drop around 1773038706.0
for i in range(len(rx_t)):
    t = rx_t[i]
    if 1773038705.8 < t < 1773038706.5:
        # find closest tx
        closest_tx_l = 0
        for j in range(len(tx_t)):
            if tx_t[j] <= t:
                closest_tx_l = tx_l[j]
        print(f"Time {t:.3f}: TX L={closest_tx_l}, RX L={rx_l[i]}, RX R={rx_r[i]}")

