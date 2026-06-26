===========================================================================
CSOPESY OS Emulator & Process Scheduler - Infrastructure Environment Build
===========================================================================
Author: GROUP 6 

Files Configured:
 - config.h     : Application configuration metadata structure contracts.
 - config.cpp   : Space-delimited dynamic string clamping parser engine.
 - clock.cpp    : Atomic global ticking thread heartbeat counter object.
 - main.cpp     : Core micro-kernel module execution orchestrator.
 - config.txt   : Initial environment deployment parameters properties.

---------------------------------------------------------------------------
1. FULL SYSTEM EMULATOR COMPILATION COMMAND
---------------------------------------------------------------------------
To compile the entire combined workspace project files into a unified executable target, execute the following command syntax from your console environment terminal context:

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp config.cpp clock.cpp -o csopesy

---------------------------------------------------------------------------
2. INDEPENDENT HARNESS DEVELOPMENT OR TEST BUILDS
---------------------------------------------------------------------------
To isolate verify build features separately during validation checkpoints without missing linker dependencies:

A. Test Build Verification via Member 2 Scheduler Harness:
g++ -std=c++17 -pthread scheduler.cpp config.cpp clock.cpp main_2.cpp -o test2

B. Test Build Verification via Member 3 Process Interpreter Engine:
g++ -std=c++17 -pthread process.cpp interpreter.cpp config.cpp clock.cpp main_3.cpp -o test3

---------------------------------------------------------------------------
3. RUNNING SYSTEM INSTRUCTIONS
---------------------------------------------------------------------------
Ensure "config.txt" resides directly adjacent inside the runtime workspace folder context profile.
Execute Binary:
./csopesy