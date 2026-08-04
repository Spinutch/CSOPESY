// ============================================================================
// Logger.h  -  Per-run session transcript
// ============================================================================
// Every run of the emulator writes its own timestamped "run_*.txt" file
// containing everything printed to the console plus every command the user
// typed, so a full transcript survives after the terminal window closes.
// ============================================================================
#pragma once

#include <streambuf>
#include <fstream>
#include <string>

class SessionLogger {
public:
    SessionLogger();
    ~SessionLogger();

    SessionLogger(const SessionLogger&) = delete;
    SessionLogger& operator=(const SessionLogger&) = delete;

    // Records a line the user typed at a prompt. Written to the log file
    // only -- the terminal already echoes typed input on its own, so this
    // avoids printing it twice on screen.
    void logInput(const std::string& promptAndLine);

    const std::string& filename() const { return filename_; }

private:
    // Duplicates every character written to std::cout into a second
    // streambuf (the log file) as well as the original terminal buffer.
    class TeeBuf : public std::streambuf {
    public:
        TeeBuf(std::streambuf* terminal, std::streambuf* file);

    protected:
        int overflow(int c) override;
        std::streamsize xsputn(const char* s, std::streamsize n) override;

    private:
        std::streambuf* terminal_;
        std::streambuf* file_;
    };

    std::string filename_;
    std::ofstream file_;
    TeeBuf teeBuf_;
    std::streambuf* originalCoutBuf_;
};
