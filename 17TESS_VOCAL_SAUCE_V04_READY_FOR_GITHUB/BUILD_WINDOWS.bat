@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul || (echo ERROR: Install CMake and add it to PATH. & pause & exit /b 1)
where git >nul 2>nul || (echo ERROR: Install Git for Windows. & pause & exit /b 1)
echo [17TESS] Configuring Visual Studio 2022 x64 and downloading JUCE 8.0.7...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || (echo Configuration failed. Install Visual Studio 2022 C++ Desktop tools. & pause & exit /b 1)
echo [17TESS] Compiling official JUCE VST3 wrapper...
cmake --build build --config Release --target VocalSauce_VST3 -j 4 || (echo Build failed. & pause & exit /b 1)
echo.
echo [17TESS] Build finished. Check this directory:
echo %CD%\build\VocalSauce_artefacts\Release\VST3\
echo Copy the entire 17TESS VOCAL SAUCE.vst3 DIRECTORY into:
echo %CommonProgramFiles%\VST3\
echo.
pause
