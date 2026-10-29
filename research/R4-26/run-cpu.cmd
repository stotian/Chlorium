@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_SCALE_OUT=%~dp0..\..\build-research\owned-scale-program"
set "TASK_SCALE_FLAGS="
if not "%~1"=="" if not "%~1"=="--benchmark" if not "%~1"=="--borrow-input-mutant" exit /b 2
if "%~1"=="--borrow-input-mutant" set "TASK_SCALE_OUT=%TASK_SCALE_OUT%-mutant"
if "%~1"=="--borrow-input-mutant" set "TASK_SCALE_FLAGS=/DCHLORIUM_SCALE_BORROW_INPUT_MUTANT=1"
if not exist "%TASK_SCALE_OUT%" mkdir "%TASK_SCALE_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_SCALE_FLAGS% "%~dp0scale_checks.cpp" /Fo"%TASK_SCALE_OUT%\\" /Fe"%TASK_SCALE_OUT%\scale_checks.exe"
if errorlevel 1 exit /b %errorlevel%
if "%~1"=="--benchmark" (
    "%TASK_SCALE_OUT%\scale_checks.exe" --benchmark "%TASK_SCALE_OUT%\timings.csv"
) else (
    "%TASK_SCALE_OUT%\scale_checks.exe"
)
exit /b %errorlevel%
