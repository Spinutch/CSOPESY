===========================================================================
CSOPESY OS Emulator & Process Scheduler - Process Multiplexer Workspace
===========================================================================
BRANCH: mo1
Entry Point File: main.cpp

Group Members:
 - Justine Jaye Valdez  (Member 1 - Shell & System UI)
 - Danika Francine Dy   (Member 2 - Scheduler Engine)
 - Rai Isidro           (Member 3 - Process, Interpreter & UI)
 - Heather Soper        (Member 4 - Integration, Configuration & Heartbeat)

---------------------------------------------------------------------------
1. APPLICATION OVERVIEW
---------------------------------------------------------------------------
This application is a 100% pure Console/CLI-based OS Emulator and Multi-threaded Process Scheduler built using C++17. It features a space-separated configuration loader, automated boundary clamping, a background thread execution engine supporting FCFS and Round Robin scheduling, and a global atomic cycle heartbeat clock.

---------------------------------------------------------------------------
2. SYSTEM COMPILATION INSTRUCTIONS
---------------------------------------------------------------------------

To compile the entire combined workspace codebase into a single unified 
production executable target, run the following command in your terminal:

g++ -std=c++17 -pthread main.cpp console.cpp system_ui.cpp util.cpp scheduler.cpp process.cpp interpreter.cpp Config.cpp Clock.cpp -o csopesy

---------------------------------------------------------------------------
3. RUNNING & EXECUTION INSTRUCTIONS
---------------------------------------------------------------------------
1. Ensure that the "config.txt" properties file is placed in the exact same workspace directory as your compiled binary.
2. Launch the terminal application:
   ./csopesy
3. Inside the emulator interface prompt, you MUST call the initialization gate sequence before any other operations can be utilized:
   root:\> initialize
4. Following initialization, standard commands such as 'screen -ls', 'scheduler-start', and 'report-util' will become fully unlocked and operational. Use 'exit' to terminate.