#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <csignal>
#include <cstdint>
#include <Windows.h>
#include <conio.h>

constexpr size_t MEMORY_MAX = 1 << 16;
std::array<uint16_t, MEMORY_MAX> memory;

enum Register {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};
std::array<uint16_t, R_COUNT> reg;

enum OpCode {
    OP_BR = 0, //branch
    OP_ADD,    //add
    OP_LD,     //load
    OP_ST,     //store
    OP_JSR,    //jump register
    OP_AND,    //bitwise and
    OP_LDR,    //load register
    OP_STR,    //store register
    OP_RTI,    //unused
    OP_NOT,    //bitwise not
    OP_LDI,    //load indirect
    OP_STI,    //store indirect
    OP_JMP,    //jump
    OP_RES,    //reserved
    OP_LEA,    //load effective address
    OP_TRAP    //execute trap
};

enum ConditionFlag {
    FL_POS = 1 << 0, //P
    FL_ZRO = 1 << 1, //Z
    FL_NEG = 1 << 2, //N
};

enum TrapCode {
    TRAP_GETC  = 0x20, //get character from keyboard, not echoed
    TRAP_OUT   = 0x21, //output a character
    TRAP_PUTS  = 0x22, //output a word string
    TRAP_IN    = 0x23, //get character from keyboard, echoed
    TRAP_PUTSP = 0x24, //output a byte string
    TRAP_HALT  = 0x25  //halt the program
};

enum MemoryMap {
    MR_KBSR = 0xFE00, //keyboard status
    MR_KBDR = 0xFE02  //keyboard data
};

constexpr uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

constexpr uint16_t swap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

void update_flags(uint16_t r) {
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) { //a 1 in the left-most bit indicates negative
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode);
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT
            ^ ENABLE_LINE_INPUT;
    SetConsoleMode(hStdin, fdwMode);
    FlushConsoleInputBuffer(hStdin);
}

void restore_input_buffering() {
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key() {
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

void handle_interrupt(int signal) {
    restore_input_buffering();
    std::cout << "\n";
    std::exit(-2);
}

void mem_write(uint16_t address, uint16_t val) {
    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
    if (address == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = static_cast<uint16_t>(_getch());
        } else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

bool read_image_file(const std::string& image_path) {
    std::ifstream file(image_path, std::ios::binary);
    if (!file.is_open()) return false;

    uint16_t origin;
    file.read(reinterpret_cast<char*>(&origin), sizeof(origin));
    origin = swap16(origin);

    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory.data() + origin;

    std::vector<uint16_t> buffer(max_read);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(max_read * sizeof(uint16_t)));
    size_t read_count = file.gcount() / sizeof(uint16_t);

    for (size_t i = 0; i < read_count; ++i) {
        *p = swap16(buffer[i]);
        ++p;
    }

    return true;
}

void trap_puts() {
    //one char per word
    uint16_t* c = memory.data() + reg[R_R0];
    while (*c) {
        std::cout << static_cast<char>(*c);
        ++c;
    }
    std::cout << std::flush;
}

void trap_putsp() {
    //one char per byte (two bytes per word)
    uint16_t* c = memory.data() + reg[R_R0];
    while (*c) {
        char char1 = static_cast<char>((*c) & 0xFF);
        std::cout << char1;
        char char2 = static_cast<char>((*c) >> 8);
        if (char2) std::cout << char2;
        ++c;
    }
    std::cout << std::flush;
}

void trap_getc() {
    reg[R_R0] = static_cast<uint16_t>(_getch());
    update_flags(R_R0);
}

void trap_out() {
    std::cout << static_cast<char>(reg[R_R0]) << std::flush;
}

void trap_in() {
    std::cout << "Enter a character: " << std::flush;
    char c = static_cast<char>(getchar());
    std::cout << c << std::flush;
    reg[R_R0] = static_cast<uint16_t>(static_cast<unsigned char>(c));
    update_flags(R_R0);
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cout << "lc3 [image-file1] ...\n";
        return 2;
    }

    for (int j = 1; j < argc; ++j) {
        if (!read_image_file(argv[j])) {
            std::cout << "failed to load image: " << argv[j] << "\n";
            return 1;
        }
    }

    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    reg[R_COND] = FL_ZRO;

    //PC set to starting pos
    constexpr uint16_t PC_START = 0x3000;
    reg[R_PC] = PC_START;

    int running = 1;
    while (running) {
        //fetch
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op) {
            case OP_ADD: {
                //destination register
                uint16_t r0 = (instr >> 9) & 0x7;
                //first operand
                uint16_t r1 = (instr >> 6) & 0x7;
                //immediate mode
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag) {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] + imm5;
                } else {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                }

                update_flags(r0);
                break;
            }
            case OP_AND: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag) {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] & imm5;
                } else {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }
                update_flags(r0);
                break;
            }
            case OP_NOT: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;

                reg[r0] = ~reg[r1];
                update_flags(r0);
                break;
            }
            case OP_BR: {
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                uint16_t cond_flag = (instr >> 9) & 0x7;
                if (cond_flag & reg[R_COND]) {
                    reg[R_PC] += pc_offset;
                }
                break;
            }
            case OP_JMP: {
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r1];
                break;
            }
            case OP_JSR: {
                uint16_t long_flag = (instr >> 11) & 1;
                reg[R_R7] = reg[R_PC];
                if (long_flag) {
                    uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += long_pc_offset;
                } else {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1];
                }
                break;
            }
            case OP_LD: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(reg[R_PC] + pc_offset);
                update_flags(r0);
                break;
            }
            case OP_LDI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(r0);
                break;
            }
            case OP_LDR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                reg[r0] = mem_read(reg[r1] + offset);
                update_flags(r0);
                break;
            }
            case OP_LEA: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = reg[R_PC] + pc_offset;
                update_flags(r0);
                break;
            }
            case OP_ST: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(reg[R_PC] + pc_offset, reg[r0]);
                break;
            }
            case OP_STI: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                break;
            }
            case OP_STR: {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                mem_write(reg[r1] + offset, reg[r0]);
                break;
            }
            case OP_TRAP: {
                reg[R_R7] = reg[R_PC];

                switch (instr & 0xFF) {
                    case TRAP_GETC:
                        trap_getc();
                        break;
                    case TRAP_OUT:
                        trap_out();
                        break;
                    case TRAP_PUTS:
                        trap_puts();
                        break;
                    case TRAP_IN:
                        trap_in();
                        break;
                    case TRAP_PUTSP:
                        trap_putsp();
                        break;
                    case TRAP_HALT:
                        std::cout << "HALT" << std::endl;
                        running = 0;
                        break;
                    default:
                        break;
                }
                break;
            }
            case OP_RES:
            case OP_RTI:
            default:
                std::abort();
        }
    }

    restore_input_buffering();
    return 0;
}