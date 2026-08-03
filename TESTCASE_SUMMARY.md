# MO2 Quiz — Test Case Configs, Run Commands & Expected Outputs

Important: the emulator always reads a file literally named `config.txt`
(hardcoded in `console.cpp:164`, `cfg.loadFromFile("config.txt")`). It does
NOT read `config3.txt`, `config4.txt`, etc. directly. So for each test case,
copy that test case's config file over `config.txt` **before** launching/
building, per the quiz rule that only `config.txt` may be touched once the
program is running.

Files added to the repo root: `config3.txt`, `config4.txt`, `config5.txt`,
`config6.txt`, `config7.txt` (numbered to match quiz question numbers).
Test cases 1 and 2 don't require a config (source code link / PPT report).

## Build once

```
cmake -S . -B build
cmake --build build -j4
```

Binary: `./build/emulator`

## Common run pattern for every test case

```
cp config<N>.txt config.txt
./build/emulator
root:\> initialize
```

Then type the commands listed for that test case, in order.

---

## Test Case 3 — `config3.txt`

```
cp config3.txt config.txt && ./build/emulator
```
Sequence: `initialize` → `scheduler-start` → wait 2s → `process-smi` → `screen -ls` → `vmstat` every 1-2s, ×10.

Config: num-cpu 4, rr, quantum 4, batch-freq 1, ins 1000-2000, mem 32768 total / 32 per frame / 8 per proc (note: 8 is below the emulator's allowed floor of 64 bytes and will be clamped up to 64 by `Config.cpp`'s `clampToPow2`).

**Expected output:** With generous memory (32768 bytes) and tiny per-process footprints, almost every generated process fits in memory at once. `screen -ls` should show 100% CPU utilization with most/all processes in the "running" state (little to no waiting on memory). `vmstat` should show total/used/free memory with used memory > 0 and climbing as more processes are created, but overall utilization staying low relative to the large pool.

## Test Case 4 — `config4.txt`

```
cp config4.txt config.txt && ./build/emulator
```
Sequence: `initialize` → `scheduler-start` → wait 10s → `scheduler-stop` → wait 30s → `vmstat` → inspect backing-store text file(s).

Config: num-cpu 8, rr, quantum 1, batch-freq 1, ins fixed 1000, mem 1024 total / 256 per frame / 1024 per proc — since `min/max-mem-per-proc` (1024) equals the entire `max-overall-mem`, only one process can ever be resident at a time.

**Expected output:** Only one process fits in memory at once, so every process swap forces a page-out of the current resident process and a page-in of the next. `vmstat`'s "num paged in" / "num paged out" counters should be very high — roughly `> min-ins × number of processes generated`, since (per the quiz) the memory manager attempts a page in/out on every single instruction executed. `csopesy-backing-store.txt` should show entries being written as processes are swapped out to the backing store.

## Test Case 5 — `config5.txt`

```
cp config5.txt config.txt && ./build/emulator
```
Sequence: `initialize` → `scheduler-test` → periodically `screen -ls` (watch for both 100% and 0% CPU utilization) → `vmstat` near the end.

Config: num-cpu 1, rr, quantum 10, batch-freq 60 (batch generation is infrequent relative to a single CPU), ins 30-45, delay-per-exec 2 (any 1-4 is valid per the quiz — used 2), mem 4096 total / 64 per frame / 512 per proc (generous memory relative to process count/size).

**Expected output:** Because there's only 1 CPU and `batch-process-freq` is high (60 cycles), there will be stretches where a process is actively running (100% CPU utilization in `screen -ls`) and stretches where no process exists yet / the previous one finished before the next batch is generated (0% CPU utilization, scheduler idle). Memory is generous enough that processes are never blocked waiting for frames — the 0% periods are caused purely by the scheduler having nothing to run, not by memory pressure. `vmstat` at the end should show reasonable used/free memory with no anomalies.

## Test Case 6 — `config6.txt`

```
cp config6.txt config.txt && ./build/emulator
```
Sequence: `initialize` → run `screen -c faulty_process "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x000 varA; READ varC 0x000; PRINT(\"Variable A: \" + varA); PRINT(\"Result: \" + varC)"` → `screen -ls` → `screen -r faulty_process`.

Config: num-cpu 1, rr, quantum 10, batch-freq 1, ins fixed 1000 (batch generator, not used by the manual `screen -c` process), mem 256 total / 256 per frame / 256 per proc — a single frame exactly sized to fit one process, no paging pressure expected for this manual test.

**Expected output:** `varA = 10 + 5 = 15` is written to address `0x000`, then read back into `varC`. `screen -r faulty_process` (or the process's own console output) should print:
```
Variable A: 15
Result: 15
```
No memory access violation should occur since `0x000` is a valid address within the process's allocated memory.

## Test Case 7 — `config7.txt`

```
cp config7.txt config.txt && ./build/emulator
```
Sequence: `initialize` → `scheduler-test` → wait 5s → `scheduler-stop` → `process-smi` every 2s for 10s → `vmstat`.

Config: num-cpu 8, rr, quantum 4, batch-freq 1, ins fixed 10000, mem 16384 total / 8 per frame / 32768 per proc — `min/max-mem-per-proc` (32768) is **larger than total available memory** (16384), so no single process can ever be fully paged in.

**Expected output:** Deadlock. Every generated process needs more memory than exists in the system, so the memory manager can never satisfy an allocation and no process can make progress. `process-smi` should show 0% CPU utilization indefinitely across all repeated checks over the 10-second window, and `vmstat` should confirm no running processes / no forward progress despite processes existing in the ready/waiting state.
