#include "mo1.h"
#include "Process.h"
#include "interpreter.h"
#include <iostream>
#include <cassert>

std::atomic<uint64_t> g_cpuTick{0};

int main()
{
    Process p;
    p.id = 1;
    p.name = "test6";
    p.state = ProcessState::READY;
    p.creationTimestamp = formatTimestamp();
    p.coreId = -1;

    // Seed DECLARE x,y,z = 0
    for (const auto &v : {"x", "y", "z"}) {
        Instruction d;
        d.type = InstructionType::DECLARE;
        d.varName = v;
        d.varValue = 0;
        p.instructions.push_back(d);
    }

    // FOR body: ADD x,x,1 ; PRINT x ; ADD y,y,1 ; PRINT y ; ADD z,z,1 ; PRINT z
    Instruction addx;
    addx.type = InstructionType::ADD;
    addx.dest = "x";
    addx.src1 = "x";
    addx.src1IsVar = true;
    addx.src2IsVar = false;
    addx.src2Val = 1;

    Instruction prx;
    prx.type = InstructionType::PRINT;
    prx.msg = "Value x: ";
    prx.src1IsVar = true;
    prx.src1 = "x";

    Instruction addy = addx;
    addy.dest = "y";
    addy.src1 = "y";

    Instruction pry = prx;
    pry.msg = "Value y: ";
    pry.src1 = "y";

    Instruction addz = addx;
    addz.dest = "z";
    addz.src1 = "z";

    Instruction prz = prx;
    prz.msg = "Value z: ";
    prz.src1 = "z";

    Instruction forIns;
    forIns.type = InstructionType::FOR;
    forIns.repeatCount = 100;
    forIns.forBody = {addx, prx, addy, pry, addz, prz};

    p.instructions.push_back(forIns);

    // Set totalCommands to flattened count: 3 DECLARE + 6*100
    p.totalCommands = 3 + static_cast<int>(6 * forIns.repeatCount);
    p.executedCommands = 0;

    // Optional: seed vars explicitly
    p.writeVar("x", 0);
    p.writeVar("y", 0);
    p.writeVar("z", 0);

    std::cout << "Starting Test-6 harness: " << p.name << " (" << p.totalCommands << " instructions)\n";

    uint64_t tick = 0;
    uint16_t lastx = 0, lasty = 0, lastz = 0;
    size_t lastLogs = 0;

    while (true) {
        StepResult res = stepProcess(p, 0, tick);
        tick++;

        // Read vars and ensure monotonicity
        uint16_t x = p.readVar("x");
        uint16_t y = p.readVar("y");
        uint16_t z = p.readVar("z");

        assert(x >= lastx);
        assert(y >= lasty);
        assert(z >= lastz);

        lastx = x; lasty = y; lastz = z;

        // Check that logs only grow or stay same
        assert(p.logs.size() >= lastLogs);
        lastLogs = p.logs.size();

        if (res.type == StepResult::FINISHED) {
            std::cout << "Process finished at tick " << tick << "\n";
            break;
        }
        // No sleeping expected in this program, but handle defensively
        if (res.type == StepResult::SLEEPING) {
            tick += res.sleepTicks;
        }
    }

    // After finish, check final values
    std::cout << "Final vars: x=" << p.readVar("x") << " y=" << p.readVar("y") << " z=" << p.readVar("z") << "\n";
    std::cout << "Total logs: " << p.logs.size() << " (expected " << (forIns.repeatCount * 3) << ")\n";

    // Render SMI
    renderProcessSmi(p, std::cout);

    // Basic assertions
    assert(p.readVar("x") == forIns.repeatCount);
    assert(p.readVar("y") == forIns.repeatCount);
    assert(p.readVar("z") == forIns.repeatCount);
    assert(p.logs.size() == forIns.repeatCount * 3);
    assert(p.executedCommands == p.totalCommands);
    assert(p.state == ProcessState::FINISHED);

    std::cout << "Test-6 harness passed.\n";
    return 0;
}
