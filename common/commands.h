#pragma once

typedef enum COMMAND_CODE {
    COMMAND_HLT  = 0x00, // stop the program
    COMMAND_PUSH = 0x01, // push a number or register
    COMMAND_ADD  = 0x02, // add two top values
    COMMAND_SUB  = 0x03, // subtract the first pop from the second
    COMMAND_DIV  = 0x04, // divide the second pop by the first
    COMMAND_OUT  = 0x05, // print the top value
    COMMAND_IN   = 0x06, // read a number from input
    COMMAND_JMP  = 0x07, // jump to a label
    COMMAND_JA   = 0x08, // jump if the first pop is greater
    COMMAND_CALL = 0x09, // call a function by label
    COMMAND_RET  = 0x0A, // return to the caller
    COMMAND_POP  = 0x0B, // pop a value into a register
} COMMAND_CODE;

typedef enum PUSH_ARGUMENT_TYPE {
    PUSH_ARGUMENT_INT = 0x01,      // next bytes contain an int
    PUSH_ARGUMENT_REGISTER = 0x02, // next byte contains a register
} PUSH_ARGUMENT_TYPE;

typedef enum REGISTER_CODE {
    // register numbers used in the binary file
    REGISTER_AX = 0x00,
    REGISTER_BX = 0x01,
    REGISTER_CX = 0x02,
    REGISTER_DX = 0x03,
    REGISTER_EX = 0x04,
    REGISTER_FX = 0x05,
    REGISTER_GX = 0x06,
    REGISTER_HX = 0x07,
    REGISTERS_COUNT = 8,
} REGISTER_CODE;
