# Prodva stack processor

Project folders:

- `assembler/compiler_code.c` - assembler entry point;
- `assembler/compiler_functions.c` - assembler implementation;
- `assembler/compiler_functions.h` - assembler declarations;
- `processor/processor_code.c` - processor entry point;
- `processor/processor_functions.c` - processor implementation;
- `processor/processor_functions.h` - processor declarations;
- `interpreter/interpreter.c` - interpreter entry point;
- `interpreter/interpreter_functions.c` - interpreter implementation;
- `interpreter/interpreter_functions.h` - interpreter declarations;
- `common` - shared command codes;
- `programs` - source programs and compiled `.bin` files;
- `bin` - compiled `.exe` files.

Build everything:

```bat
build_all.bat
```

Compile a program and execute it:

```bat
bin\compiler_code.exe programs\program.txt programs\program.bin
bin\processor_code.exe programs\program.bin
```

Run the old text interpreter:

```bat
bin\interpreter.exe programs\program.txt
```
