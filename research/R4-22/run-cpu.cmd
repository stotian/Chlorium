@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_PROGRAM_OUT=%~dp0..\..\build-research\owned-identity-program"
set "TASK_PROGRAM_FLAGS="
if not "%~1"=="" if not "%~1"=="--no-validation-mutant" if not "%~1"=="--borrow-input-mutant" exit /b 2
if "%~1"=="--no-validation-mutant" set "TASK_PROGRAM_OUT=%TASK_PROGRAM_OUT%-mutant"
if "%~1"=="--no-validation-mutant" set "TASK_PROGRAM_FLAGS=/DCHLORIUM_PROGRAM_SKIP_VALIDATION_MUTANT=1"
if "%~1"=="--borrow-input-mutant" set "TASK_PROGRAM_OUT=%TASK_PROGRAM_OUT%-borrow-mutant"
if "%~1"=="--borrow-input-mutant" set "TASK_PROGRAM_FLAGS=/DCHLORIUM_PROGRAM_BORROW_INPUT_MUTANT=1"
if not exist "%TASK_PROGRAM_OUT%" mkdir "%TASK_PROGRAM_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_PROGRAM_FLAGS% "%~dp0program_checks.cpp" /Fo"%TASK_PROGRAM_OUT%\\" /Fe"%TASK_PROGRAM_OUT%\program_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_PROGRAM_OUT%\program_checks.exe"
exit /b %errorlevel%
