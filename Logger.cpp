#include "Logger.h"
#include <iostream>
#include <ctime>

static std::string makeLogFilename() {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "run_%Y%m%d_%H%M%S.txt", tm);
    return std::string(buf);
}

SessionLogger::TeeBuf::TeeBuf(std::streambuf* terminal, std::streambuf* file)
    : terminal_(terminal), file_(file) {}

int SessionLogger::TeeBuf::overflow(int c) {
    if (c == EOF) return 0;
    terminal_->sputc(static_cast<char>(c));
    file_->sputc(static_cast<char>(c));
    return c;
}

std::streamsize SessionLogger::TeeBuf::xsputn(const char* s, std::streamsize n) {
    terminal_->sputn(s, n);
    file_->sputn(s, n);
    return n;
}

SessionLogger::SessionLogger()
    : filename_(makeLogFilename())
    , file_(filename_, std::ios::trunc)
    , teeBuf_(std::cout.rdbuf(), file_.rdbuf())
    , originalCoutBuf_(std::cout.rdbuf(&teeBuf_))
{
    std::cout << "[console] Session transcript: " << filename_ << "\n";
}

SessionLogger::~SessionLogger() {
    std::cout.rdbuf(originalCoutBuf_);
}

void SessionLogger::logInput(const std::string& promptAndLine) {
    file_ << promptAndLine << "\n";
}
