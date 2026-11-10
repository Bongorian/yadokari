#!/usr/bin/env python3
"""Summarize captured EFF rows; partial runs remain explicitly incomplete."""
import argparse
import json
import re
from pathlib import Path


def fields(line):
    words = line.split()
    return {words[i]: int(words[i + 1]) for i in range(0, len(words), 2)}


def summarize(paths):
    result = {"sources": [str(p) for p in paths], "rows": {}, "native": []}
    phases = ("load", "ready", "play", "stop", "drain", "restore", "idle", "unused")
    for path in paths:
        raw=path.read_text(errors="replace")
        if re.search(r'EFF revision [23] begin',raw):phases=("load","ready","play","stop","drain","restore","fileplay","idle")
        for line in raw.splitlines():
            match = re.search(r"EFF row (\d+) (.*)", line)
            if match:
                row = int(match[1])
                if row in result["rows"]:
                    raise ValueError(f"Duplicate row {row}; do not merge different trials")
                values = fields(match[2])
                values.update(song=row // 8 + 1, phase=phases[row % 8], cores={})
                values["average_us"] = values["total-us"] / values["blocks"] if values["blocks"] else None
                result["rows"][row] = values
            assist = re.search(r"EFF assist row (\d+) (.*)", line)
            if assist:
                result["rows"][int(assist[1])]["assist"] = fields(assist[2])
            match = re.search(r"EFF core row (\d+) core (\d+) (.*)", line)
            if match:
                row, core = int(match[1]), int(match[2])
                values = fields(match[3])
                values["work_duty_percent"] = values["busy-us"] * 100 / result["rows"][row]["wall-us"]
                result["rows"][row]["cores"][core] = values
            for kind in ['pool','peak','reset','event']:
                extra=re.search(r"EFF "+kind+r" row (\d+) (.*)",line)
                if extra:result['rows'][int(extra[1])][kind]=fields(extra[2])
            match = re.search(r"EFF native song (\d+) track (\d+) (.*)", line)
            if match:
                values = fields(match[3])
                values.update(song=int(match[1]), track=int(match[2]))
                values["average_us_per24"] = values["ticks"] / 54 * 24 / values["frames"]
                values["sampled_max24_us"] = values["max24-ticks"] / 54
                result["native"].append(values)
    result["complete_play_songs"] = [r["song"] for r in result["rows"].values() if r["phase"] == "play"]
    result["clock_hz"] = 54000000
    result["interpretation"] = "Elapsed productive work duty, not PMU utilization; native maxima are stride127 observations, not upper bounds."
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.logs), indent=2))
