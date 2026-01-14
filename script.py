import re
import statistics
import sys

def analyze_log_file(filename):
    try:
        with open(filename, 'r') as f:
            log_data = f.read()
    except FileNotFoundError:
        print(f"Error: The file '{filename}' was not found.")
        return
    # Matches: [Phase Name] took: [num]ms
    # Handles scientific notation and optional spaces before 'ms'
    pattern = r"(.*?)\s+took:\s+([\d.e-]+)\s*ms"
    matches = re.findall(pattern, log_data)
    if not matches:
        print("No timing data found. Ensure your logs use the format: 'Phase Name took: 123ms'")
        return
    data = {}
    for phase, time_str in matches:
        phase = phase.strip()
        time_val = float(time_str)
        if phase not in data:
            data[phase] = []
        data[phase].append(time_val)
    # Print Table Header
    print(f"\n{'PHASE NAME':<30} | {'COUNT':<6} | {'TOTAL (ms)':<12} | {'MEAN (ms)':<12} | {'MAX (ms)':<10}")
    print("-" * 85)
    # Sort by total time descending
    sorted_phases = sorted(data.items(), key=lambda x: sum(x[1]), reverse=True)
    for phase, times in sorted_phases:
        count = len(times)
        total = sum(times)
        mean = statistics.mean(times)
        max_val = max(times)
        print(f"{phase:<30} | {count:<6} | {total:<12.4f} | {mean:<12.6f} | {max_val:<10.4f}")

if __name__ == "__main__":
    target_file = sys.argv[1] if len(sys.argv) > 1 else 'log.txt'
    analyze_log_file(target_file)