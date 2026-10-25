@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
python "%~dp0build_quad_candidate.py" %*
exit /b %errorlevel%
