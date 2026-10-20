#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../my_lib_func/Header_Only.h"
#include "../common/commands.h"
#include "compiler_functions.h"

enum {
    MAX_FILE_PATH_SIZE = 260,
    MAX_LINE_SIZE = 256,
    MAX_COMMAND_SIZE = 32,
    MAX_LABEL_SIZE = 64,
    MAX_LABELS_COUNT = 128
};

typedef struct LabelS {
    char name[MAX_LABEL_SIZE];
    int32_t address;
} LABEL;

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

static int write_byte(FILE *output_file, uint8_t byte) {
    return fwrite(&byte, sizeof(byte), 1, output_file) == 1;
}

static int write_int32_little_endian(FILE *output_file, int32_t value) {
    uint32_t unsigned_value = (uint32_t) value;
    unsigned char bytes[4] = {
        (unsigned char) (unsigned_value & 0xFFu),
        (unsigned char) ((unsigned_value >> 8u) & 0xFFu),
        (unsigned char) ((unsigned_value >> 16u) & 0xFFu),
        (unsigned char) ((unsigned_value >> 24u) & 0xFFu),
    };

    return fwrite(bytes, sizeof(bytes), 1, output_file) == 1;
}

static int read_int32_little_endian(FILE *input_file, int32_t *value) {
    unsigned char bytes[4] = {0};

    if (fread(bytes, sizeof(bytes), 1, input_file) != 1) {
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

static const char *get_register_name(uint8_t register_code);

static int print_compiled_file(FILE *compiled_file) {
    uint8_t command_code = 0;

    // show the compiled file in a readable form
    rewind(compiled_file);
    printf("Compiled file:\n");

    while (fread(&command_code, sizeof(command_code), 1, compiled_file) == 1) {
        if (command_code == COMMAND_PUSH) {
            uint8_t argument_type = 0;

            if (fread(&argument_type, sizeof(argument_type), 1, compiled_file) != 1) {
                printf("Broken PUSH command in compiled file\n");
                return 0;
            }

            if (argument_type == PUSH_ARGUMENT_INT) {
                int32_t argument = 0;

                if (!read_int32_little_endian(compiled_file, &argument)) {
                    printf("Broken PUSH integer in compiled file\n");
                    return 0;
                }
                printf("%02X %d\n", (unsigned int) command_code, argument);
            }
            else if (argument_type == PUSH_ARGUMENT_REGISTER) {
                uint8_t register_code = 0;

                if (fread(&register_code, sizeof(register_code), 1, compiled_file) != 1 ||
                    get_register_name(register_code) == NULL) {
                    printf("Broken PUSH register in compiled file\n");
                    return 0;
                }
                printf("%02X %s\n", (unsigned int) command_code,
                       get_register_name(register_code));
            }
            else {
                printf("Unknown PUSH argument type: %02X\n", (unsigned int) argument_type);
                return 0;
            }
        }
        else if (command_code == COMMAND_POP) {
            uint8_t argument_type = 0;
            uint8_t register_code = 0;

            if (fread(&argument_type, sizeof(argument_type), 1, compiled_file) != 1 ||
                fread(&register_code, sizeof(register_code), 1, compiled_file) != 1 ||
                argument_type != PUSH_ARGUMENT_REGISTER ||
                get_register_name(register_code) == NULL) {
                printf("Broken POP command in compiled file\n");
                return 0;
            }

            printf("%02X %s\n", (unsigned int) command_code,
                   get_register_name(register_code));
        }
        else if (command_code == COMMAND_JMP ||
                 command_code == COMMAND_JA ||
                 command_code == COMMAND_CALL) {
            int32_t address = 0;

            if (!read_int32_little_endian(compiled_file, &address)) {
                printf("Broken jump command in compiled file\n");
                return 0;
            }

            printf("%02X %d\n", (unsigned int) command_code, address);
        }
        else {
            printf("%02X\n", (unsigned int) command_code);
        }
    }

    if (ferror(compiled_file)) {
        printf("Cannot read compiled file\n");
        return 0;
    }

    return 1;
}

static int parse_int_argument(const char *argument, int32_t *value) {
    if (argument == NULL || value == NULL) {
        return 0;
    }

    errno = 0;
    char *number_end = NULL;
    long parsed_value = strtol(argument, &number_end, 10);

    if (number_end == argument || *number_end != '\0' || errno == ERANGE ||
        parsed_value < INT32_MIN || parsed_value > INT32_MAX) {
        return 0;
    }

    *value = (int32_t) parsed_value;
    return 1;
}

static int parse_register(const char *argument, uint8_t *register_code) {
    if (argument == NULL || register_code == NULL || strlen(argument) != 2) {
        return 0;
    }

    char register_name[3] = {argument[0], argument[1], '\0'};
    string_to_upper(register_name);

    if (register_name[1] != 'X' || register_name[0] < 'A' || register_name[0] > 'H') {
        return 0;
    }

    *register_code = (uint8_t) (register_name[0] - 'A');
    return 1;
}

static const char *get_register_name(uint8_t register_code) {
    static const char *register_names[REGISTERS_COUNT] = {
        "ax", "bx", "cx", "dx", "ex", "fx", "gx", "hx"
    };

    if (register_code >= REGISTERS_COUNT) {
        return NULL;
    }

    return register_names[register_code];
}

static int compile_push(FILE *output_file, const char *argument, size_t line_number) {
    int32_t value = 0;

    // PUSH can use a number or one of eight registers
    if (parse_int_argument(argument, &value)) {
        if (!write_byte(output_file, COMMAND_PUSH) ||
            !write_byte(output_file, PUSH_ARGUMENT_INT) ||
            !write_int32_little_endian(output_file, value)) {
            printf("Line %zu: cannot write PUSH into output file\n", line_number);
            return 0;
        }

        return 1;
    }

    uint8_t register_code = 0;
    if (parse_register(argument, &register_code)) {
        if (!write_byte(output_file, COMMAND_PUSH) ||
            !write_byte(output_file, PUSH_ARGUMENT_REGISTER) ||
            !write_byte(output_file, register_code)) {
            printf("Line %zu: cannot write PUSH register into output file\n", line_number);
            return 0;
        }

        return 1;
    }

    printf("Line %zu: PUSH needs an integer or register ax-hx\n", line_number);
    return 0;
}

static int compile_pop(FILE *output_file, const char *argument, size_t line_number) {
    uint8_t register_code = 0;

    if (!parse_register(argument, &register_code)) {
        printf("Line %zu: POP needs a register ax-hx\n", line_number);
        return 0;
    }

    if (!write_byte(output_file, COMMAND_POP) ||
        !write_byte(output_file, PUSH_ARGUMENT_REGISTER) ||
        !write_byte(output_file, register_code)) {
        printf("Line %zu: cannot write POP into output file\n", line_number);
        return 0;
    }

    return 1;
}

static int get_command_code(const char *command, uint8_t *command_code) {
    if (strcmp(command, "ADD") == 0) {
        *command_code = COMMAND_ADD;
    }
    else if (strcmp(command, "SUB") == 0) {
        *command_code = COMMAND_SUB;
    }
    else if (strcmp(command, "DIV") == 0) {
        *command_code = COMMAND_DIV;
    }
    else if (strcmp(command, "OUT") == 0) {
        *command_code = COMMAND_OUT;
    }
    else if (strcmp(command, "IN") == 0) {
        *command_code = COMMAND_IN;
    }
    else if (strcmp(command, "JMP") == 0) {
        *command_code = COMMAND_JMP;
    }
    else if (strcmp(command, "JA") == 0) {
        *command_code = COMMAND_JA;
    }
    else if (strcmp(command, "CALL") == 0) {
        *command_code = COMMAND_CALL;
    }
    else if (strcmp(command, "RET") == 0) {
        *command_code = COMMAND_RET;
    }
    else if (strcmp(command, "POP") == 0) {
        *command_code = COMMAND_POP;
    }
    else if (strcmp(command, "HLT") == 0) {
        *command_code = COMMAND_HLT;
    }
    else {
        return 0;
    }

    return 1;
}

static int find_label(const LABEL labels[], size_t labels_count,
                      const char *name, int32_t *address) {
    for (size_t label_index = 0; label_index < labels_count; label_index++) {
        if (strcmp(labels[label_index].name, name) == 0) {
            if (address != NULL) {
                *address = labels[label_index].address;
            }
            return 1;
        }
    }

    return 0;
}

static int add_label(LABEL labels[], size_t *labels_count,
                     const char *name, int32_t address, size_t line_number) {
    if (strlen(name) >= MAX_LABEL_SIZE) {
        printf("Line %zu: label is too long\n", line_number);
        return 0;
    }

    if (find_label(labels, *labels_count, name, NULL)) {
        printf("Line %zu: duplicate label: %s\n", line_number, name);
        return 0;
    }

    if (*labels_count >= MAX_LABELS_COUNT) {
        printf("Line %zu: too many labels\n", line_number);
        return 0;
    }

    strcpy(labels[*labels_count].name, name);
    labels[*labels_count].address = address;
    (*labels_count)++;
    return 1;
}

static int is_label(char *word) {
    size_t word_length = strlen(word);

    if (word_length == 0 || word[word_length - 1] != ':') {
        return 0;
    }

    word[word_length - 1] = '\0';
    return 1;
}

static int collect_labels(FILE *source_file, LABEL labels[], size_t *labels_count) {
    char line[MAX_LINE_SIZE] = "";
    size_t line_number = 0;
    int32_t current_address = 0;

    // first pass. collect labels and count addresses
    while (fgets(line, sizeof(line), source_file) != NULL) {
        char command[MAX_COMMAND_SIZE] = "";
        char argument[MAX_LINE_SIZE] = "";
        char extra_argument[MAX_LINE_SIZE] = "";

        line_number++;
        remove_comment(line);

        int scanned = sscanf(line, "%31s %255s %255s", command, argument, extra_argument);
        if (scanned <= 0) {
            continue;
        }

        if (is_label(command)) {
            if (scanned != 1 || command[0] == '\0' ||
                !add_label(labels, labels_count, command, current_address, line_number)) {
                return 0;
            }
            continue;
        }

        string_to_upper(command);

        if (strcmp(command, "PUSH") == 0) {
            int32_t value = 0;
            uint8_t register_code = 0;

            if (scanned != 2) {
                printf("Line %zu: PUSH needs one argument\n", line_number);
                return 0;
            }

            if (parse_int_argument(argument, &value)) {
                current_address += 6;
            }
            else if (parse_register(argument, &register_code)) {
                current_address += 3;
            }
            else {
                printf("Line %zu: bad PUSH argument: %s\n", line_number, argument);
                return 0;
            }
        }
        else if (strcmp(command, "POP") == 0) {
            uint8_t register_code = 0;

            if (scanned != 2 || !parse_register(argument, &register_code)) {
                printf("Line %zu: POP needs a register ax-hx\n", line_number);
                return 0;
            }
            current_address += 3;
        }
        else if (strcmp(command, "JMP") == 0 ||
                 strcmp(command, "JA") == 0 ||
                 strcmp(command, "CALL") == 0) {
            current_address += 5;
        }
        else {
            uint8_t command_code = 0;
            if (!get_command_code(command, &command_code)) {
                printf("Line %zu: unknown command: %s\n", line_number, command);
                return 0;
            }
            current_address += 1;
        }
    }

    rewind(source_file);
    return 1;
}

static int compile_program(FILE *source_file, FILE *output_file,
                           const LABEL labels[], size_t labels_count) {
    char line[MAX_LINE_SIZE] = "";
    size_t line_number = 0;

    // second pass. write commands using known label addresses
    while (fgets(line, sizeof(line), source_file) != NULL) {
        char command[MAX_COMMAND_SIZE] = "";
        char argument[MAX_LINE_SIZE] = "";
        char extra_argument[MAX_LINE_SIZE] = "";

        line_number++;
        remove_comment(line);

        int scanned = sscanf(line, "%31s %255s %255s", command, argument, extra_argument);
        if (scanned <= 0) {
            continue;
        }

        if (is_label(command)) {
            continue;
        }

        string_to_upper(command);

        if (strcmp(command, "PUSH") == 0) {
            if (scanned != 2 || !compile_push(output_file, argument, line_number)) {
                return 0;
            }
            continue;
        }

        if (strcmp(command, "POP") == 0) {
            if (scanned != 2 || !compile_pop(output_file, argument, line_number)) {
                return 0;
            }
            continue;
        }

        uint8_t command_code = 0;
        if (!get_command_code(command, &command_code)) {
            printf("Line %zu: unknown command: %s\n", line_number, command);
            return 0;
        }

        if (command_code == COMMAND_JMP ||
            command_code == COMMAND_JA ||
            command_code == COMMAND_CALL) {
            int32_t address = 0;

            if (scanned != 2) {
                printf("Line %zu: command %s needs one label\n", line_number, command);
                return 0;
            }

            if (!find_label(labels, labels_count, argument, &address)) {
                printf("Line %zu: unknown label: %s\n", line_number, argument);
                return 0;
            }

            if (!write_byte(output_file, command_code) ||
                !write_int32_little_endian(output_file, address)) {
                printf("Line %zu: cannot write jump command\n", line_number);
                return 0;
            }
            continue;
        }

        if (scanned != 1) {
            printf("Line %zu: command %s does not take arguments\n", line_number, command);
            return 0;
        }

        if (!write_byte(output_file, command_code)) {
            printf("Line %zu: cannot write command into output file\n", line_number);
            return 0;
        }

    }

    return 1;
}

static void get_file_path(int argc, char *argv[], int argument_number,
                          const char *message, char *file_path) {
    if (argc > argument_number) {
        strncpy(file_path, argv[argument_number], MAX_FILE_PATH_SIZE - 1);
        file_path[MAX_FILE_PATH_SIZE - 1] = '\0';
    }
    else {
        printf("%s", message);

        if (fgets(file_path, MAX_FILE_PATH_SIZE, stdin) == NULL) {
            printf("Input error\n");
            exit(EXIT_FAILURE);
        }

        remove_newline(file_path);
    }

    remove_quotes(file_path);
}

int run_compiler(int argc, char *argv[]) {
    char source_file_path[MAX_FILE_PATH_SIZE] = "";
    char output_file_path[MAX_FILE_PATH_SIZE] = "program.bin";

    get_file_path(argc, argv, 1, "Enter path to source file: ", source_file_path);

    if (argc >= 3) {
        strncpy(output_file_path, argv[2], sizeof(output_file_path) - 1);
        output_file_path[sizeof(output_file_path) - 1] = '\0';
        remove_quotes(output_file_path);
    }

    FILE *source_file = fopen(source_file_path, "r");
    if (source_file == NULL) {
        printf("Cannot open source file: %s\n", source_file_path);
        return EXIT_FAILURE;
    }

    LABEL labels[MAX_LABELS_COUNT] = {0};
    size_t labels_count = 0;

    if (!collect_labels(source_file, labels, &labels_count)) {
        fclose(source_file);
        printf("Compilation failed\n");
        return EXIT_FAILURE;
    }

    FILE *output_file = fopen(output_file_path, "w+b");
    if (output_file == NULL) {
        printf("Cannot create output file: %s\n", output_file_path);
        fclose(source_file);
        return EXIT_FAILURE;
    }

    int result = compile_program(source_file, output_file, labels, labels_count);

    if (result) {
        if (fflush(output_file) != 0) {
            printf("Cannot finish writing compiled file\n");
            result = 0;
        }
        else {
            result = print_compiled_file(output_file);
        }
    }

    fclose(output_file);
    fclose(source_file);

    if (!result) {
        printf("Compilation failed\n");
        return EXIT_FAILURE;
    }

    printf("Compilation completed: %s\n", output_file_path);
    return EXIT_SUCCESS;
}
