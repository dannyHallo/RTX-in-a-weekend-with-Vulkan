@echo off
setlocal EnableDelayedExpansion

if "%1"=="debug" (
    set BUILD_TYPE=debug
) else if "%1"=="release" (
    set BUILD_TYPE=release
) else (
    echo invalid build type "%1". Exiting...
    goto :eof
)

if "%2"=="skipcpp" (
    set SKIP_CPP=1
) else (
    set SKIP_CPP=0
)

set GLSLC=%VULKAN_SDK%/Bin/glslc.exe

set GLSLC_PATH=%GLSLC%
set SHADERS=svo postProcessing 
@REM set SHADERS=gradientProjection rtx screenSpaceGradient stratumFilter temporalFilter variance aTrous postProcessing 

@REM ---------------------------------------------------------------------------------------

echo compiling shaders...
@REM wiping out the generated shaders
rd /s /q "resources/shaders/generated"
mkdir "resources/shaders/generated"
for %%s in (%SHADERS%) do (
   echo compiling resources/shaders/source/%%s.comp to resources/shaders/generated/%%s.spv
    "%GLSLC_PATH%" resources/shaders/source/%%s.comp -o resources/shaders/generated/%%s.spv
    if !errorlevel! neq 0 (
        echo Build failed with error %errorlevel%. Exiting... 
        goto :eof
    )
)

@REM ---------------------------------------------------------------------------------------

echo:
if %SKIP_CPP%==0 (
    echo compiling cpp ...

    xmake f -p windows -a x64 -m %BUILD_TYPE%
    xmake -w

    if !errorlevel! neq 0 (
       echo Build failed with error !errorlevel!. Exiting... 
       goto :eof
    )
    echo xmake success
) else (
    echo skipping cpp compilation
)

@REM ---------------------------------------------------------------------------------------

echo:
echo copy resources ...
@REM Create the directory if it doesn't exist
if not exist "build/windows/x64/%BUILD_TYPE%/resources" mkdir "build/windows/x64/%BUILD_TYPE%/resources"
@REM Use robocopy instead of xcopy for better performance
robocopy "resources" "build/windows/x64/%BUILD_TYPE%/resources" /E /IS /NFL /NDL /NJH /NJS /nc /ns /np
@REM wiping out the source shaders
rd /s /q "build/windows/x64/%BUILD_TYPE%/resources/shaders/source"

@REM ---------------------------------------------------------------------------------------

echo:
echo run app ...
echo ---------------------------------------------------------------------------------------
echo:

@REM run the application
@REM /b means to stay in the command line below, 
@REM /wait blocks the terminal to wait for the application to exit
start /wait /b /d "build/windows/x64/%BUILD_TYPE%" main.exe
