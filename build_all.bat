@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "BIN_DIR=%PROJECT_DIR%bin"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

gcc "%PROJECT_DIR%assembler\compiler_code.c" "%PROJECT_DIR%assembler\compiler_functions.c" -std=c11 -Wall -Wextra -o "%BIN_DIR%\compiler_code.exe"
if errorlevel 1 goto build_error

gcc "%PROJECT_DIR%processor\processor_code.c" "%PROJECT_DIR%processor\processor_functions.c" -std=c11 -Wall -Wextra -o "%BIN_DIR%\processor_code.exe"
if errorlevel 1 goto build_error

gcc "%PROJECT_DIR%interpreter\interpreter.c" "%PROJECT_DIR%interpreter\interpreter_functions.c" -std=c11 -Wall -Wextra -o "%BIN_DIR%\interpreter.exe"
if errorlevel 1 goto build_error

echo All programs were built successfully.
exit /b 0

:build_error
echo Build failed.
exit /b 1
