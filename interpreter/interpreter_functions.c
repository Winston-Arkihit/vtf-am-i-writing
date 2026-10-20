#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../my_lib_func/Header_Only.h"
#include "../../My_stacks/C_StackLib/Stack_Header_Only.h"
#include "interpreter_functions.h"

enum {
    START_STACK_CAPACITY = 16,
    MAX_FILE_PATH_SIZE = 260,
    MAX_LINE_SIZE = 256,
    MAX_COMMAND_SIZE = 32
};

static void string_to_upper(char *string) {
    if (string == NULL) {
        return;
    }

    for (size_t i = 0; string[i] != '\0'; i++) {
        string[i] = (char) toupper((unsigned char) string[i]);
    }
}

static void remove_comment(char *line) {
    char *semicolon_comment = strchr(line, ';');
    char *slash_comment = strstr(line, "//");
    char *comment = NULL;

    if (semicolon_comment != NULL && slash_comment != NULL) {
        comment = semicolon_comment < slash_comment ? semicolon_comment : slash_comment;
    }
    else if (semicolon_comment != NULL) {
        comment = semicolon_comment;
    }
    else {
        comment = slash_comment;
    }

    if (comment != NULL) {
        *comment = '\0';
    }
}

static int process_push(Stack *stack, const char *argument, size_t line_number) {
    if (argument == NULL) {
        printf("Line %zu: PUSH needs a number\n", line_number);
        return 0;
    }

    char *number_end = NULL;
    TypeOfStackElem value = strtod(argument, &number_end);

    if (number_end == argument) {
        printf("Line %zu: bad PUSH argument\n", line_number);
        return 0;
    }

    stack_push(stack, value);

    if (!stack_is_ok(stack)) {
        printf("Line %zu: stack push error\n", line_number);
        return 0;
    }

    return 1;
}

static int process_binary_operation(Stack *stack, const char *command, size_t line_number) {
    TypeOfStackElem right_value = 0;
    TypeOfStackElem left_value = 0;

    // pop the right value first, then the left one
    if (stack_pop(stack, &right_value) != STACK_OK || stack_pop(stack, &left_value) != STACK_OK) {
        printf("Line %zu: command %s needs two numbers in stack\n", line_number, command);
        return 0;
    }

    if (strcmp(command, "ADD") == 0) {
        stack_push(stack, left_value + right_value);
        return stack_is_ok(stack);
    }

    if (strcmp(command, "SUB") == 0) {
        stack_push(stack, left_value - right_value);
        return stack_is_ok(stack);
    }

    if (strcmp(command, "DIV") == 0) {
        if (right_value == 0) {
            printf("Line %zu: division by zero\n", line_number);
            return 0;
        }

        stack_push(stack, left_value / right_value);
        return stack_is_ok(stack);
    }

    return 0;
}

static int process_out(Stack *stack, size_t line_number) {
    TypeOfStackElem value = 0;

    if (stack_pop(stack, &value) != STACK_OK) {
        printf("Line %zu: OUT needs one number in stack\n", line_number);
        return 0;
    }

    printf("%lg\n", value);

    return 1;
}

static int process_program(FILE *source_file) {
    Stack stack = {};
    char line[MAX_LINE_SIZE] = "";
    size_t line_number = 0;
    int program_finished = 0;

    stack_activate(&stack, START_STACK_CAPACITY);

    if (!stack_is_ok(&stack)) {
        printf("Stack activation error\n");
        return 0;
    }

    // the old version reads and executes each line at once
    while (fgets(line, sizeof(line), source_file) != NULL) {
        char command[MAX_COMMAND_SIZE] = "";
        char argument[MAX_LINE_SIZE] = "";

        line_number++;

        remove_comment(line);

        int scanned = sscanf(line, "%31s %255s", command, argument);
        if (scanned <= 0) {
            continue;
        }

        string_to_upper(command);

        if (strcmp(command, "PUSH") == 0) {
            if (!process_push(&stack, scanned == 2 ? argument : NULL, line_number)) {
                stack_destruct(&stack);
                return 0;
            }
        }
        else if (strcmp(command, "ADD") == 0 ||
                 strcmp(command, "SUB") == 0 ||
                 strcmp(command, "DIV") == 0) {
            if (!process_binary_operation(&stack, command, line_number)) {
                stack_destruct(&stack);
                return 0;
            }
        }
        else if (strcmp(command, "OUT") == 0) {
            if (!process_out(&stack, line_number)) {
                stack_destruct(&stack);
                return 0;
            }
        }
        else if (strcmp(command, "HLT") == 0) {
            program_finished = 1;
            break;
        }
        else {
            printf("Line %zu: unknown command: %s\n", line_number, command);
            stack_destruct(&stack);
            return 0;
        }
    }

    if (!program_finished) {
        printf("Warning: program finished without HLT\n");
    }

    stack_destruct(&stack);
    return 1;
}

int run_interpreter(int argc, char *argv[]) {
    char file_path[MAX_FILE_PATH_SIZE] = "";

    if (argc >= 2) {
        strncpy(file_path, argv[1], sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';
    }
    else {
        printf("Enter path to source file: ");

        if (fgets(file_path, sizeof(file_path), stdin) == NULL) {
            printf("Input error\n");
            return 1;
        }

        remove_newline(file_path);
    }

    remove_quotes(file_path);

    FILE *source_file = fopen(file_path, "r");
    if (source_file == NULL) {
        printf("Cannot open file: %s\n", file_path);
        return 1;
    }

    int result = process_program(source_file);

    fclose(source_file);

    if (!result) {
        printf("Program finished with error\n");
        return 1;
    }

    return 0;
}
