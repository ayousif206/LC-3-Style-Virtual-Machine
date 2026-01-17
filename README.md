# LC-3 Virtual Machine

C++20 implementation of the LC-3 virtual machine, this emulates the LC-3 hardware therefore allowing you to run assembly programs (like the game 2048 provided as example) on Windows only for now.

## Overview

The LC-3 is a simple computer architecture used for education of assembly language and computer systems. What is simulated here is the memory, registers, and the instruction set; so binary programs compiled for the LC-3 should be able to be executed.

## Features

* **65,536 Memory Locations** (16-bit address space)
* **10 Registers**: 8 General Purpose (R0-R7), Program Counter (PC), and Condition Flags (COND)
* **Full Instruction Set**: Implements all standard opcodes (`ADD`, `AND`, `BR`, `JMP`, `JSR`, `LD`, `LDI`, `LDR`, `LEA`, `NOT`, `ST`, `STI`, `STR`, `TRAP`)
* **Trap Routines**: Handles I/O operations like `PUTS` (string output) and `GETC` (character input)

## Prerequisites

* **Operating System**: Windows
* **Compiler**: C++20 compatible compiler

## Quick Start

To use this program, compile it then run it in terminal with 2048.obj as an argument or any other assembly program you want to test.
