@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_AREA_OUT=%~dp0..\..\build-research\stamped-area"
set "TASK_AREA_FLAGS="
if not "%~1"=="" if not "%~1"=="--last-writer-mutant" if not "%~1"=="--object-major-mutant" if not "%~1"=="--callback-slot-mutant" exit /b 2
if "%~1"=="--last-writer-mutant" set "TASK_AREA_OUT=%~dp0..\..\build-research\stamped-area-last-mutant"
if "%~1"=="--last-writer-mutant" set "TASK_AREA_FLAGS=/DCHLORIUM_AREA_LAST_WRITER_MUTANT=1"
if "%~1"=="--object-major-mutant" set "TASK_AREA_OUT=%~dp0..\..\build-research\stamped-area-order-mutant"
if "%~1"=="--object-major-mutant" set "TASK_AREA_FLAGS=/DCHLORIUM_AREA_OBJECT_MAJOR_MUTANT=1"
if "%~1"=="--callback-slot-mutant" set "TASK_AREA_OUT=%~dp0..\..\build-research\stamped-area-slot-mutant"
if "%~1"=="--callback-slot-mutant" set "TASK_AREA_FLAGS=/DCHLORIUM_AREA_CALLBACK_SLOT_MUTANT=1"
if not exist "%TASK_AREA_OUT%" mkdir "%TASK_AREA_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_AREA_FLAGS% "%~dp0stamped_area_checks.cpp" /Fo"%TASK_AREA_OUT%\\" /Fe"%TASK_AREA_OUT%\stamped_area_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_AREA_OUT%\stamped_area_checks.exe"
exit /b %errorlevel%
