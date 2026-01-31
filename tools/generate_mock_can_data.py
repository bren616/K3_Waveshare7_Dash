import csv

# Configuration matching main.c
class DashVariable:
    def __init__(self, name, can_id, num_bytes, msb_idx, lsb_idx, current_val, min_val, max_val, step_val, increasing):
        self.name = name
        self.can_id = can_id
        self.num_bytes = num_bytes
        self.msb_idx = msb_idx
        self.lsb_idx = lsb_idx
        self.current_val = current_val
        self.min_val = min_val
        self.max_val = max_val
        self.step_val = step_val
        self.increasing = increasing

# {"RPM", &ui_RPMLabel, 0x100, 2, 1, 0, 1000, 1000, 14000, 100, true},
vars_init = [
    ("RPM", 0x100, 2, 1, 0, 1000, 1000, 14000, 100, True),
    ("Gear", 0x121, 1, -1, 2, 0, 0, 6, 1, True),
    ("WaterTemp", 0x111, 2, 0, 1, 60, 60, 120, 1, True),
    ("OilTemp", 0x132, 2, 0, 1, 20, 20, 110, 1, True),
    ("OilPress", 0x133, 2, 0, 1, 1, 1, 90, 1, True),
    ("FLTemp", 0x126, 1, -1, 2, 5, 5, 35, 1, True),
    ("FRTemp", 0x127, 1, -1, 2, 5, 5, 35, 1, True),
    ("FLPress", 0x126, 2, 3, 4, 11, 11, 22, 1, True),
    ("FRPress", 0x127, 2, 3, 4, 11, 11, 22, 1, True),
    ("RLTemp", 0x128, 1, -1, 2, 5, 5, 35, 1, True),
    ("RRTemp", 0x129, 1, -1, 2, 5, 5, 35, 1, True),
    ("RLPress", 0x128, 2, 3, 4, 11, 11, 22, 1, True),
    ("RRPress", 0x129, 2, 3, 4, 11, 11, 22, 1, True)
]

dash_vars = [DashVariable(*v) for v in vars_init]

# Duration to generate
# 100ms per step.
# Max sweep is RPM: (14000-1000)/100 = 130 steps = 13s.
# 30 seconds = 300 steps.
steps = 300
interval_us = 100000 # 100ms

filename = "mock_data_savvycan.csv"

with open(filename, 'w', newline='') as f:
    writer = csv.writer(f)
    # Header compatible with SavvyCAN
    writer.writerow(["Time Stamp", "ID", "Extended", "Dir", "Bus", "LEN", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8"])
    
    current_time = 0
    
    for _ in range(steps):
        # We need to buffer frames per ID because multiple vars might write to same ID
        frames = {} # ID -> bytearray(8)
        
        for v in dash_vars:
           # Simulate update (Mock logic from can_manager.c)
            if v.increasing:
                v.current_val += v.step_val
                if v.current_val >= v.max_val:
                    v.current_val = v.max_val
                    v.increasing = False
            else:
                v.current_val -= v.step_val
                if v.current_val <= v.min_val:
                    v.current_val = v.min_val
                    v.increasing = True
            
            # Pack into frame
            if v.can_id not in frames:
                frames[v.can_id] = bytearray(8)
            
            data = frames[v.can_id]
            val = int(v.current_val)
            
            if v.num_bytes == 1:
                if v.lsb_idx < 8:
                    data[v.lsb_idx] = val & 0xFF
            elif v.num_bytes == 2:
                # MSB and LSB handling
                if v.msb_idx < 8:
                    data[v.msb_idx] = (val >> 8) & 0xFF
                if v.lsb_idx < 8:
                    data[v.lsb_idx] = val & 0xFF

        # Write all frames to CSV
        for can_id, payload in sorted(frames.items()):
            row = [
                current_time,
                f"0x{can_id:X}",
                "false",
                "Rx",
                "0",
                "8",
                f"0x{payload[0]:02X}",
                f"0x{payload[1]:02X}",
                f"0x{payload[2]:02X}",
                f"0x{payload[3]:02X}",
                f"0x{payload[4]:02X}",
                f"0x{payload[5]:02X}",
                f"0x{payload[6]:02X}",
                f"0x{payload[7]:02X}"
            ]
            writer.writerow(row)
        
        current_time += interval_us

print(f"Generated {filename}")
