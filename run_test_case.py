#!/usr/bin/env python3
"""
run_test_case.py  -  Member 4: Integration & Testing

Owns execution of the graded test case end-to-end:
  1. Launch ./csopesy and run 'initialize'.
  2. Run 'scheduler-start'.
  3. Wait 5 seconds.
  4. Run 'scheduler-stop'.
  5. Poll 'screen -ls' every 2 seconds until every generated process reaches
     Finished, or until 1 minute has elapsed (whichever comes first).
  6. Run 'exit' to shut the emulator down cleanly (joins all threads).
  7. Collect every memory_stamp_*.txt produced during the run and zip them
     into a single archive for submission.

Usage:
    python3 run_test_case.py [--binary ./csopesy] [--cwd .] [--poll-timeout 60]

Requires: pexpect (pip install pexpect --break-system-packages)

NOTE: This script assumes the memory manager (owned by Member 3) and the
scheduler's allocate/deallocate call sites (owned by Member 2) are already
merged in, since only then will processes actually produce
memory_stamp_*.txt snapshots and reach Finished under real memory pressure.
Running it before that merge will still exercise the shell/scheduler flow,
but the zip step will simply find zero snapshot files.
"""
import argparse
import glob
import os
import re
import sys
import time
import zipfile

try:
    import pexpect
except ImportError:
    sys.exit(
        "ERROR: pexpect is required. Install with:\n"
        "  pip install pexpect --break-system-packages\n"
    )

PROMPT = r"root:\\> "


def log(msg):
    print(f"[run_test_case] {msg}", flush=True)


def all_processes_finished(screen_ls_output: str) -> bool:
    """Return True if the 'Running processes:' block is empty (i.e. every
    generated process has moved to Finished or none were ever running)."""
    m = re.search(r"Running processes:\s*\n[-]+\s*\n(.*?)\n[-]+", screen_ls_output, re.S)
    if not m:
        # Couldn't parse the block; be conservative and say "not finished yet".
        return False
    body = m.group(1).strip()
    return body == "(none)" or body == ""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--binary", default="./csopesy", help="Path to the compiled emulator binary")
    ap.add_argument("--cwd", default=".", help="Working directory (must contain config.txt)")
    ap.add_argument("--poll-timeout", type=int, default=60, help="Max seconds to poll screen -ls")
    ap.add_argument("--poll-interval", type=int, default=2, help="Seconds between screen -ls polls")
    ap.add_argument("--warmup", type=int, default=5, help="Seconds to let scheduler-start run before stopping")
    ap.add_argument("--zip-name", default=None, help="Output zip filename (default: timestamped)")
    args = ap.parse_args()

    binary = os.path.abspath(args.binary)
    if not os.path.isfile(binary):
        sys.exit(f"ERROR: binary not found at {binary}. Build it first (see README.txt).")

    os.chdir(args.cwd)
    if not os.path.isfile("config.txt"):
        sys.exit("ERROR: config.txt not found in the working directory.")

    log(f"Spawning {binary} ...")
    child = pexpect.spawn(binary, encoding="utf-8", timeout=30, cwd=os.getcwd())
    child.logfile_read = sys.stdout  # mirror emulator output live

    child.expect(PROMPT)
    log("Sending: initialize")
    child.sendline("initialize")
    child.expect(PROMPT)

    log("Sending: scheduler-start")
    child.sendline("scheduler-start")
    child.expect(PROMPT)

    log(f"Warming up for {args.warmup}s ...")
    time.sleep(args.warmup)

    log("Sending: scheduler-stop")
    child.sendline("scheduler-stop")
    child.expect(PROMPT)

    log(f"Polling 'screen -ls' every {args.poll_interval}s (max {args.poll_timeout}s) ...")
    start = time.time()
    finished = False
    while time.time() - start < args.poll_timeout:
        child.sendline("screen -ls")
        child.expect(PROMPT)
        output = child.before
        if all_processes_finished(output):
            finished = True
            log("All processes reached Finished.")
            break
        time.sleep(args.poll_interval)

    if not finished:
        log(f"WARNING: 1-minute window elapsed without all processes finishing.")

    log("Sending: exit")
    child.sendline("exit")
    child.expect(pexpect.EOF, timeout=15)
    child.close()
    log(f"Emulator exited (status={child.exitstatus}).")

    # ---- Collect and zip memory_stamp_*.txt ----
    stamp_files = sorted(glob.glob("memory_stamp_*.txt"))
    if not stamp_files:
        log("No memory_stamp_*.txt files found (memory manager may not be merged in yet).")
    zip_name = args.zip_name or time.strftime("memory_stamps_%Y%m%d_%H%M%S.zip")
    with zipfile.ZipFile(zip_name, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in stamp_files:
            zf.write(f)
    log(f"Wrote {len(stamp_files)} snapshot file(s) to {zip_name}")


if __name__ == "__main__":
    main()
