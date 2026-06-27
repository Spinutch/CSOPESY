===========================================================================
CSOPESY OS Emulator & Process Scheduler - Process Multiplexer Workspace
===========================================================================
Entry Point File: main.cpp

Group Members:
 - Heather Soper        (Integration, Configuration & Heartbeat)
 - Justin Valdes        (Shell & System UI)
 - Danika Dy            (Scheduler Engine)
 - Rai Isidro           (Process, Interpreter & UI)

---------------------------------------------------------------------------
SYSTEM COMPILATION INSTRUCTIONS
---------------------------------------------------------------------------

To compile into a single unified production executable target, run the following command:

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp -o csopesy

---------------------------------------------------------------------------
INDEPENDENT HARNESS/TEST BUILDS
---------------------------------------------------------------------------
To verify individual modules separately during checkpoint testing loops without hitting linker dependency conflicts:

A. Scheduler Validation Harness:
g++ -std=c++17 -pthread scheduler.cpp Config.cpp Clock.cpp main_2.cpp -o test2

B. Process/Interpreter Validation Harness:
g++ -std=c++17 -pthread process.cpp interpreter.cpp Config.cpp Clock.cpp main_3.cpp -o test3

---------------------------------------------------------------------------
RUNNING & EXECUTION INSTRUCTIONS
---------------------------------------------------------------------------
1. Ensure that the "config.txt" properties file is placed in the exact same workspace directory as compiled binary.
2. Launch the terminal application:
   ./csopesy
3. Inside the emulator interface prompt, you MUST call the initialization gate sequence before any other operations can be utilized:
   root:\> initialize
4. Following initialization, standard commands such as 'screen -ls', 'scheduler-start', and 'report-util' will become fully unlocked and operational. Use 'exit' to terminate.