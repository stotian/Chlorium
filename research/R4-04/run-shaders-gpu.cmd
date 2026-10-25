@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_SHADER_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
set "TASK_SHADER_OUT=%~dp0..\..\build-research\shader-gpu"
set "TASK_SHADER_DEPS=C:\Users\stoti\Bismuth-2.208\build\_deps"
set "TASK_SHADER_LIBS=C:\Users\stoti\Chlorium\build\_deps"
if not exist "%TASK_SHADER_OUT%" mkdir "%TASK_SHADER_OUT%"
copy /Y "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\glew32.dll" "%TASK_SHADER_OUT%\glew32.dll" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /MD /W3 /utf-8 /I"%~dp0..\..\tests\fakes" /I"%~dp0..\..\src" /I"%TASK_SHADER_DEPS%\glm-src" /I"%TASK_SHADER_DEPS%\glslang-src" /I"%TASK_SHADER_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0..\..\tests\gpu_checks.cpp" "%~dp0..\..\src\renderer\ShaderPreprocessor.cpp" /Fo"%TASK_SHADER_OUT%\\" /Fe"%TASK_SHADER_OUT%\shader_checks.exe" /link opengl32.lib user32.lib gdi32.lib "%TASK_SHADER_SDK%\link\win64\glew32.lib" "%TASK_SHADER_LIBS%\glslang-build\glslang\glslang.lib" "%TASK_SHADER_LIBS%\glslang-build\glslang\glslang-default-resource-limits.lib" "%TASK_SHADER_LIBS%\glm-build\glm\glm.lib"
if errorlevel 1 exit /b %errorlevel%
"%TASK_SHADER_OUT%\shader_checks.exe" "%~dp0..\..\."
exit /b %errorlevel%
