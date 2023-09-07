import time
import sys

PROGRESS_HEADER = " -> "
CURRENT = 0
TOTAL = 1
PREV = 0

def init(total):
    global CURRENT, TOTAL
    CURRENT = 0
    TOTAL = total
    sys.stdout.write(PROGRESS_HEADER + "  0%")
    sys.stdout.flush()
    sys.stdout.write("\b" * 4)

def update():
    global CURRENT, TOTAL, PREV
    CURRENT = CURRENT + 1
    percent = CURRENT / float(TOTAL)
    percent = max(0, min(percent, 1.0))
    percent = int(100 * percent)
    nspaces = 0
    if 0 <= percent < 10:
        nspaces = 2
    elif 10 <= percent < 100:
        nspaces = 1
    if percent != PREV:
        PREV = percent
        sys.stdout.write(" " * nspaces + str(percent) + "%")
        sys.stdout.flush()
        sys.stdout.write("\b" * 4)

def deinit():
    sys.stdout.write("100%")
    sys.stdout.write("\n")
