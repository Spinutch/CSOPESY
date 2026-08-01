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
This application is a 100% pure Console/CLI-based OS Emulator and Multi-threaded Process Scheduler built using C++17. It features a space-separated configuration loader, automated boundary clamping, a background thread execution engine supporting FCFS and Round Robin scheduling, a global atomic cycle heartbeat clock, and (MO2) a demand-paging memory manager with backing-store support.

---------------------------------------------------------------------------
2. SYSTEM COMPILATION INSTRUCTIONS
---------------------------------------------------------------------------

To compile the entire combined workspace codebase into a single unified
production executable target, run the following command in your terminal:

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp MemoryUtils.cpp BackingStore.cpp MemoryManager.cpp -o csopesy

NOTE: MemoryManager.cpp (the frame table / page fault / eviction engine) is
owned by Rai and is not yet present in the workspace as of this revision.
The build command above is pre-registered per the MO2 file layout; it will
not link until MemoryManager.cpp/.h are merged in. Until then, drop
"MemoryManager.cpp" from the command below to build the current state of
the project (everything except real demand paging already compiles and
runs, including screen -s memory validation and the backing-store file):

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp MemoryUtils.cpp BackingStore.cpp -o csopesy

---------------------------------------------------------------------------
2b. BUILD VIA CMAKE (RECOMMENDED)
---------------------------------------------------------------------------
The workspace also ships a CMakeLists.txt that builds the console
application. It requires GLFW3 (via pkg-config) and OpenGL to be available
on your system to configure.

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
3. RUNNING & EXECUTION INSTRUCTIONS
---------------------------------------------------------------------------
1. Ensure that the "config.txt" properties file is placed in the exact same workspace directory as your compiled binary (for CMake builds, copy or symlink config.txt into build/, or run the binary from the project root with ./build/emulator).
2. Launch the terminal application:
   ./csopesy        (g++ build)
   ./build/emulator (CMake build)
3. Inside the emulator interface prompt, you MUST call the initialization gate sequence before any other operations can be utilized:
   root:\> initialize
4. Following initialization, standard commands such as 'screen -ls', 'scheduler-start', and 'report-util' will become fully unlocked and operational. Use 'exit' to terminate.
5. MO2 commands:
   - screen -s <process_name> <process_memory_size>   : creates a process with the given memory allocation.
     Size must be a power of 2 in [64, 65536] bytes; otherwise the console
     prints "invalid memory allocation: <size> ..." and does NOT create the
     process. Example: screen -s process1 256
   - csopesy-backing-store.txt  : written to the working directory at startup and
     kept live-updated as pages are swapped in/out. Open it any time to inspect
     the current backing store contents (empty until Rai's Memory Manager
     starts evicting/loading pages through BackingStore.h).
   - process-smi / vmstat   : (pending Justine's CLI implementation) will render
     memory/CPU-tick data assembled via MemoryStats.h once Rai's frame table
     can report used/free memory.
   - screen -c <process_name> <mem_size> "<instructions>"  : (pending Danika) creates
     a process with 1-50 semicolon-separated user-defined instructions.

---------------------------------------------------------------------------
4. CONFIG.TXT PARAMETERS (config.txt)
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