@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
python "%~dp0compile_adapter.py"
exit /b %errorlevel%
