@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
python "%~dp0prepare_reference.py"
if errorlevel 1 exit /b %errorlevel%
set "TASK_SLOT_OUT=%~dp0..\..\build-research\native-slot-cache-r52"
set "TASK_SLOT_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_SLOT_OUT%" /I"%TASK_SLOT_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0slot_cost.cpp" /Fo"%TASK_SLOT_OUT%\\" /Fe"%TASK_SLOT_OUT%\slot_cost.exe" /link "%TASK_SLOT_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_SLOT_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0slot_oracle.cpp" /Fo"%TASK_SLOT_OUT%\\" /Fe"%TASK_SLOT_OUT%\slot_oracle.exe" /link "%TASK_SLOT_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
