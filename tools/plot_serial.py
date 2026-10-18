#!/usr/bin/env python3
"""
Capture the GM6020 CSV telemetry from USART1 and plot tracking results.

Examples
--------
    python tools/plot_serial.py --port COM5 --seconds 30 --out run.csv
    python tools/plot_serial.py --csv run.csv --save run.png
"""

import argparse
import csv
import datetime
import os
import sys
import time

FIELDS = [
    "ms", "state", "mode", "auto",
    "rpm_ref_x10", "rpm_x10",
    "deg_ref_rel_x10", "deg_rel_x10",
    "voltage", "temp", "fault_hex",
]

MODE_SPEED = 0
MODE_POSITION = 1
STATE_RUN = 1


def capture(port, baud, seconds, out_path):
    try:
        import serial
    except ImportError:
        sys.exit("pyserial is missing: pip install -r tools/requirements.txt")

    ser = serial.Serial(port, baud, timeout=1)
    start = time.time()
    rows = 0

    print("capturing %s @ %d for %.1f s -> %s" %
          (port, baud, seconds, out_path))

    with open(out_path, "w", newline="", encoding="utf-8") as fp:
        writer = csv.writer(fp)
        writer.writerow(FIELDS)

        while (time.time() - start) < seconds:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("ascii", errors="ignore").strip()
            if not line or line.startswith("#"):
                continue

            if line.startswith("ms,") or line.startswith("CSV:"):
                continue

            parts = line.split(",")
            if len(parts) != len(FIELDS):
                continue

            try:
                int(parts[0])
            except ValueError:
                continue

            writer.writerow(parts)
            rows += 1

    ser.close()

    print("saved %d samples to %s" % (rows, out_path))
    return out_path


def _load(path):
    with open(path, newline="", encoding="utf-8") as fp:
        rows = list(csv.DictReader(fp))

    if not rows:
        sys.exit("no samples in %s" % path)

    clean = []
    for row in rows:
        try:
            clean.append({
                "ms": int(row["ms"]),
                "state": int(row["state"]),
                "mode": int(row["mode"]),
                "rpm_ref": float(row["rpm_ref_x10"]) / 10.0,
                "rpm": float(row["rpm_x10"]) / 10.0,
                "deg_ref": float(row["deg_ref_rel_x10"]) / 10.0,
                "deg": float(row["deg_rel_x10"]) / 10.0,
                "voltage": float(row["voltage"]),
            })
        except (KeyError, ValueError):
            continue

    if not clean:
        sys.exit("no valid rows in %s" % path)

    t0 = clean[0]["ms"]
    for row in clean:
        row["t"] = (row["ms"] - t0) / 1000.0

    return clean


def plot(path, save_path):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        sys.exit("matplotlib is missing: pip install -r tools/requirements.txt")

    rows = _load(path)
    speed = [r for r in rows if r["mode"] == MODE_SPEED and r["state"] == STATE_RUN]
    position = [r for r in rows if r["mode"] == MODE_POSITION and r["state"] == STATE_RUN]

    fig, axes = plt.subplots(2, 2, figsize=(13, 7))
    fig.suptitle("GM6020 sine tracking - %s" % os.path.basename(path))

    ax = axes[0][0]
    if speed:
        ax.plot([r["t"] for r in speed], [r["rpm_ref"] for r in speed], label="reference")
        ax.plot([r["t"] for r in speed], [r["rpm"] for r in speed], label="measured", alpha=0.8)
    ax.set_title("Speed sine")
    ax.set_xlabel("time [s]")
    ax.set_ylabel("rpm")
    ax.grid(True)
    ax.legend(loc="upper right")

    ax = axes[0][1]
    if position:
        ax.plot([r["t"] for r in position], [r["deg_ref"] for r in position], label="reference")
        ax.plot([r["t"] for r in position], [r["deg"] for r in position], label="measured", alpha=0.8)
    ax.set_title("Position sine")
    ax.set_xlabel("time [s]")
    ax.set_ylabel("deg (relative)")
    ax.grid(True)
    ax.legend(loc="upper right")

    ax = axes[1][0]
    ax.plot([r["t"] for r in rows], [r["voltage"] for r in rows], color="tab:orange")
    ax.set_title("Voltage command")
    ax.set_xlabel("time [s]")
    ax.set_ylabel("counts")
    ax.grid(True)

    ax = axes[1][1]
    if speed:
        ax.plot([r["t"] for r in speed],
                [r["rpm"] - r["rpm_ref"] for r in speed],
                label="speed error [rpm]")
    if position:
        ax.plot([r["t"] for r in position],
                [r["deg"] - r["deg_ref"] for r in position],
                label="position error [deg]")
    ax.axhline(0.0, color="black", linewidth=0.8)
    ax.set_title("Tracking error")
    ax.set_xlabel("time [s]")
    ax.grid(True)
    ax.legend(loc="upper right")

    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print("saved figure to %s" % save_path)

    plt.show()


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--out", help="capture CSV path")
    parser.add_argument("--csv", help="plot an existing CSV instead of capturing")
    parser.add_argument("--save", help="save the figure to this path")
    args = parser.parse_args()

    if args.csv:
        plot(args.csv, args.save)
        return

    if not args.port:
        parser.error("either --port (capture) or --csv (plot) is required")

    out = args.out
    if not out:
        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        out = "run_%s.csv" % stamp

    capture(args.port, args.baud, args.seconds, out)
    plot(out, args.save)


if __name__ == "__main__":
    main()
