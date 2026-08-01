===========================================================================
CSOPESY OS Emulator & Process Scheduler - Process Multiplexer Workspace
===========================================================================
BRANCH: mo2
Entry Point File: main.cpp

Group Members (MO2 task split):
 - Justine Jaye Valdez  (CLI: process-smi / vmstat rendering; READ & WRITE instructions)
 - Rai Isidro           (Memory Manager: demand-paging allocator, page faults, context switching)
 - Heather Soper        (Memory Visualization & Backing Store Access; Required Memory per Process)
 - Danika Francine Dy   (User-defined instructions: screen -c)

---------------------------------------------------------------------------
1. APPLICATION OVERVIEW
---------------------------------------------------------------------------
This application is a 100% pure Console/CLI-based OS Emulator and
Multi-threaded Process Scheduler built using C++17. It features a
space-separated configuration loader, automated boundary clamping, a
background thread execution engine supporting FCFS and Round Robin
scheduling, a global atomic cycle heartbeat clock, and (MO2) a demand-paging
memory manager with backing-store support.

---------------------------------------------------------------------------
2. SYSTEM COMPILATION INSTRUCTIONS
---------------------------------------------------------------------------

To compile the entire combined workspace codebase into a single unified
production executable target, run the following command in your terminal:

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp MemoryUtils.cpp BackingStore.cpp MemoryManager.cpp InstructionParser.cpp -o csopesy

---------------------------------------------------------------------------
2b. BUILD VIA CMAKE (RECOMMENDED)
---------------------------------------------------------------------------
The workspace also ships a CMakeLists.txt that builds the console
application.

To configure and build:

   cmake -S . -B build
   cmake --build build -j4

This produces the executable at ./build/emulator. Only main.cpp is linked
as the entry point -- CMakeLists.txt explicitly excludes the old milestone
files (main_1.cpp, main_2.cpp, main_3.cpp) and the scratch test file
(test_memory.cpp), which would otherwise conflict with main.cpp's int main().

To rebuild after pulling changes, just re-run the "cmake --build build -j4"
command; re-run "cmake -S . -B build" only if CMakeLists.txt itself changed.

---------------------------------------------------------------------------
3. RUNNING INSTRUCTIONS
---------------------------------------------------------------------------
1. Ensure that the "config.txt" properties file is placed in the exact same
   workspace directory as your compiled binary (for CMake builds, copy or
   symlink config.txt into build/, or run the binary from the project root
   with ./build/emulator).
2. Launch the terminal application:
   ./csopesy        (g++ build)
   ./build/emulator (CMake build)
3. Inside the emulator interface prompt, you MUST call the initialization
   gate sequence before any other command is recognized:
   root:\> initialize

---------------------------------------------------------------------------
4. COMMAND REFERENCE (post-initialize)
---------------------------------------------------------------------------
Process creation:
  screen -s <name> <mem_size>
      Creates a process with the given memory allocation and attaches to
      its screen. mem_size must be a power of 2 in [64, 65536] bytes;
      otherwise the console prints "invalid memory allocation: <size> ..."
      and does NOT create the process. Example: screen -s process1 256

  screen -c <name> <mem_size> "<instructions>"
      Creates a process with 1-50 semicolon-separated user-defined
      instructions and attaches to its screen. Throws "invalid command" if
      the instruction count is out of range. Supported instruction
      keywords: DECLARE, ADD, SUBTRACT, PRINT, READ, WRITE, SLEEP, FOR.
      Example:
      screen -c process2 4096 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"

  screen -r <name>
      Re-attaches to a running process's screen. If the process name isn't
      found or has finished, prints "Process <name> not found." If the
      process shut down due to a memory access violation, prints
      "Process <name> shut down due to memory access violation error that
      occurred at <HH:MM:SS>. <hex address> invalid." instead of
      re-attaching.

  screen -ls
      Lists running and finished processes, each core's assignment/progress,
      and overall CPU utilization.

Scheduler control:
  scheduler-start / scheduler-stop
      Start/stop automatic batch process generation, spawning one process
      every batch-process-freq CPU ticks per config.txt.

  spawn-batch
      Manually spawns a single batch process on demand (useful for testing
      without waiting on the batch-process-freq cadence).

Reporting & memory visualization:
  report-util
      Writes a utilization report (CPU + memory snapshot) to
      csopesy-log.txt in the working directory.

  process-smi
      nvidia-smi-style summary: CPU utilization, memory usage/utilization,
      and per-process memory footprint for all running processes.

  vmstat
      Detailed memory/CPU stats: total/used/free memory, idle/active/total
      CPU ticks, and cumulative pages paged in/out.

  csopesy-backing-store.txt
      Written to the working directory at startup and kept live-updated as
      pages are evicted to / loaded from the backing store by the demand
      paging allocator. Open it any time to inspect current backing-store
      contents.

  exit
      Detaches from an attached process screen, or quits the application
      from the main menu.

---------------------------------------------------------------------------
5. CONFIG.TXT PARAMETERS (config.txt)
---------------------------------------------------------------------------
From MO1:
  num-cpu           : number of CPUs, range [1, 128]
  scheduler         : "fcfs" or "rr"
  quantum-cycles    : RR time slice, range [1, 2^32]
  batch-process-freq: process generation frequency in cycles, range [1, 2^32]
  min-ins / max-ins : instruction count bounds per generated process, range [1, 2^32]
  delay-per-exec    : busy-wait delay per instruction in cycles, range [0, 2^32]

New for MO2 (all memory ranges are [2^6, 2^16] bytes and must be a power of 2):
  max-overall-mem   : total memory available to the emulator, in bytes
  mem-per-frame     : bytes per frame/page (total frames = max-overall-mem / mem-per-frame)
  min-mem-per-proc  : minimum memory rolled for scheduler-generated processes
  max-mem-per-proc  : maximum memory rolled for scheduler-generated processes
