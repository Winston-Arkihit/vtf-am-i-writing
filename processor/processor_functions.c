#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../my_lib_func/Header_Only.h"
#include "../../My_stacks/C_StackLib/Stack_Header_Only.h"
#include "../common/commands.h"
#include "processor_functions.h"

enum {
    START_STACK_CAPACITY = 16,
    MAX_FILE_PATH_SIZE = 260
};

static int read_byte(FILE *program_file, uint8_t *byte) {
    return fread(byte, sizeof(*byte), 1, program_file) == 1;
}

static int read_int32_little_endian(FILE *program_file, int32_t *value) {
    unsigned char bytes[4] = {0};

    if (fread(bytes, sizeof(bytes), 1, program_file) != 1) {
        return 0;
    }

    uint32_t unsigned_value =
        ((uint32_t) bytes[0]) |
        ((uint32_t) bytes[1] << 8u) |
        ((uint32_t) bytes[2] << 16u) |
        ((uint32_t) bytes[3] << 24u);

    *value = (int32_t) unsigned_value;
    return 1;
}

static int pop_two_values(Stack *stack, TypeOfStackElem *first_pop,
                          TypeOfStackElem *second_pop, const char *command) {
    if (stack_pop(stack, first_pop) != STACK_OK ||
        stack_pop(stack, second_pop) != STACK_OK) {
        printf("Command %s needs two numbers in stack\n", command);
        return 0;
    }

    return 1;
}

static int jump_to_address(FILE *program_file, int32_t address) {
    if (address < 0 || fseek(program_file, address, SEEK_SET) != 0) {
        printf("Bad jump address: %d\n", address);
        return 0;
    }

    return 1;
}

static int process_program(FILE *program_file) {
    Stack data_stack = {0};
    Stack return_stack = {0};
    TypeOfStackElem registers[REGISTERS_COUNT] = {0};
    int is_running = 1;
    int result = 1;

    // values and CALL return addresses use separate stacks
    stack_activate(&data_stack, START_STACK_CAPACITY);
    stack_activate(&return_stack, START_STACK_CAPACITY);

    if (!stack_is_ok(&data_stack) || !stack_is_ok(&return_stack)) {
        printf("Stack activation error\n");
        result = 0;
        is_running = 0;
    }

    while (is_running) {
        uint8_t command_code = 0;

        if (!read_byte(program_file, &command_code)) {
            printf("Program ended without HLT\n");
            result = 0;
            break;
        }

        switch (command_code) {
            case COMMAND_HLT:
                is_running = 0;
                break;

            case COMMAND_PUSH: {
                uint8_t argument_type = 0;

                if (!read_byte(program_file, &argument_type)) {
                    printf("Broken PUSH command\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                // this byte tells what comes after PUSH
                if (argument_type == PUSH_ARGUMENT_INT) {
                    int32_t value = 0;

                    if (!read_int32_little_endian(program_file, &value)) {
                        printf("Broken PUSH integer\n");
                        result = 0;
                        is_running = 0;
                        break;
                    }
                    stack_push(&data_stack, (TypeOfStackElem) value);
                }
                else if (argument_type == PUSH_ARGUMENT_REGISTER) {
                    uint8_t register_code = 0;

                    if (!read_byte(program_file, &register_code) ||
                        register_code >= REGISTERS_COUNT) {
                        printf("Broken PUSH register\n");
                        result = 0;
                        is_running = 0;
                        break;
                    }
                    stack_push(&data_stack, registers[register_code]);
                }
                else {
                    printf("Unknown PUSH argument type: %02X\n",
                           (unsigned int) argument_type);
                    result = 0;
                    is_running = 0;
                }
                break;
            }

            case COMMAND_POP: {
                uint8_t argument_type = 0;
                uint8_t register_code = 0;
                TypeOfStackElem value = 0;

                if (!read_byte(program_file, &argument_type) ||
                    !read_byte(program_file, &register_code) ||
                    argument_type != PUSH_ARGUMENT_REGISTER ||
                    register_code >= REGISTERS_COUNT) {
                    printf("Broken POP command\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                if (stack_pop(&data_stack, &value) != STACK_OK) {
                    printf("POP needs one number in stack\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                // POP ax takes a value from the stack and stores it in ax
                registers[register_code] = value;
                break;
            }

            case COMMAND_ADD:
            case COMMAND_SUB:
            case COMMAND_DIV: {
                TypeOfStackElem first_pop = 0;
                TypeOfStackElem second_pop = 0;

                if (!pop_two_values(&data_stack, &first_pop, &second_pop, "arithmetic")) {
                    result = 0;
                    is_running = 0;
                    break;
                }

                if (command_code == COMMAND_ADD) {
                    stack_push(&data_stack, second_pop + first_pop);
                }
                else if (command_code == COMMAND_SUB) {
                    stack_push(&data_stack, second_pop - first_pop);
                }
                else if (first_pop == 0) {
                    printf("Division by zero\n");
                    result = 0;
                    is_running = 0;
                }
                else {
                    stack_push(&data_stack, second_pop / first_pop);
                }
                break;
            }

            case COMMAND_IN: {
                TypeOfStackElem value = 0;
                printf("Enter number: ");

                if (scanf("%lf", &value) != 1) {
                    printf("Input error\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                stack_push(&data_stack, value);
                break;
            }

            case COMMAND_OUT: {
                TypeOfStackElem value = 0;

                if (stack_pop(&data_stack, &value) != STACK_OK) {
                    printf("OUT needs one number in stack\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                printf("%lg\n", value);
                break;
            }

            case COMMAND_JMP:
            case COMMAND_JA:
            case COMMAND_CALL: {
                int32_t address = 0;

                if (!read_int32_little_endian(program_file, &address)) {
                    printf("Broken jump command\n");
                    result = 0;
                    is_running = 0;
                    break;
                }

                if (command_code == COMMAND_JA) {
                    TypeOfStackElem first_pop = 0;
                    TypeOfStackElem second_pop = 0;

                    if (!pop_two_values(&data_stack, &first_pop, &second_pop, "JA")) {
                        result = 0;
                        is_running = 0;
                        break;
                    }

                    // continue with the next command if the condition is false
                    if (first_pop <= second_pop) {
                        break;
                    }
                }

                if (command_code == COMMAND_CALL) {
                    // RET must continue right after CALL
                    long return_address = ftell(program_file);
                    if (return_address < 0) {
                        printf("Cannot remember return address\n");
                        result = 0;
                        is_running = 0;
                        break;
                    }
                    stack_push(&return_stack, (TypeOfStackElem) return_address);
                }

                if (!jump_to_address(program_file, address)) {
                    result = 0;
                    is_running = 0;
                }
                break;
            }

            case COMMAND_RET: {
                TypeOfStackElem return_address = 0;

                // take the address saved by the latest CALL
                if (stack_pop(&return_stack, &return_address) != STACK_OK ||
                    !jump_to_address(program_file, (int32_t) return_address)) {
                    printf("Cannot return: return stack is empty or damaged\n");
                    result = 0;
                    is_running = 0;
                }
                break;
            }

            default:
                printf("Unknown command code: %02X\n", (unsigned int) command_code);
                result = 0;
                is_running = 0;
                break;
        }

        if (is_running && (!stack_is_ok(&data_stack) || !stack_is_ok(&return_stack))) {
            printf("Stack error\n");
            result = 0;
            is_running = 0;
        }
    }

    stack_destruct(&return_stack);
    stack_destruct(&data_stack);
    return result;
}

int run_processor(int argc, char *argv[]) {
    char file_path[MAX_FILE_PATH_SIZE] = "";

    if (argc >= 2) {
        strncpy(file_path, argv[1], sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';
    }
    else {
        printf("Enter path to compiled file: ");

        if (fgets(file_path, sizeof(file_path), stdin) == NULL) {
            printf("Input error\n");
            return EXIT_FAILURE;
        }

        remove_newline(file_path);
    }

    remove_quotes(file_path);

    FILE *program_file = fopen(file_path, "rb");
    if (program_file == NULL) {
        printf("Cannot open compiled file: %s\n", file_path);
        return EXIT_FAILURE;
    }

    int result = process_program(program_file);
    fclose(program_file);

    return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
