@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_REPLAY_OUT=%~dp0..\..\build-research\native-batch-replay"
set "TASK_REPLAY_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_REPLAY_OUT%" mkdir "%TASK_REPLAY_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_REPLAY_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0replay_checks.cpp" /Fo"%TASK_REPLAY_OUT%\\" /Fe"%TASK_REPLAY_OUT%\replay_checks.exe" /link "%TASK_REPLAY_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
