@echo off

set TTEXE=..\..\devtools\bin\timeprecise.exe
if not exist %TTEXE% goto no_ttexe
goto no_ttexe_end

:no_ttexe
set TTEXE=time /t
:no_ttexe_end

echo.
echo ==================== buildshaders %* ==================
%TTEXE% -cur-Q
set tt_start=%ERRORLEVEL%
set tt_chkpt=%tt_start%


REM ****************
REM usage: buildshaders [group] [-game gameDir -source sourceDir]
REM ****************

setlocal
set arg_filename=%1
set shadercompilecommand=ShaderCompile26.exe
set targetdir=shaders
set SrcDirBase=..\..
set shaderDir=shaders
set SDKArgs=-local

set inputbase=
set firstarg=%~1
if not defined firstarg goto build_shaders
if "%firstarg:~0,1%" == "-" goto check_mod_args
set inputbase=%~1
shift

:check_mod_args
if /i "%1" == "-game" goto set_mod_args
goto build_shaders

REM ****************
REM USAGE
REM ****************
:usage
echo.
echo "usage: buildshaders [group] [-game] [gameDir if -game was specified] [-source sourceDir]"
echo "       group is a table name from shaders.toml, without it every group is built."
echo "       gameDir is where gameinfo.txt is (where it will store the compiled shaders)."
echo "       sourceDir is where the source code is (where it will find scripts and compilers)."
echo "ex   : buildshaders"
echo "ex   : buildshaders lightmappedgeneric -game c:\steam\steamapps\sourcemods\mymod -source c:\mymod\src"
goto :end

REM ****************
REM MOD ARGS - look for -game or the vproject environment variable
REM ****************
:set_mod_args

if not exist "..\..\devtools\bin\ShaderCompile26.exe" goto NoShaderCompile
set ChangeToDir=%SrcDirBase%\devtools\bin\

if /i "%3" NEQ "-source" goto NoSourceDirSpecified
set SrcDirBase=%~4

REM ** use the -game parameter to tell us where to put the files
set targetdir=%~2\shaders

if not exist "%~2\gameinfo.txt" goto InvalidGameDirectory

if not exist "shaders.toml" goto InvalidInputFile

goto build_shaders

REM ****************
REM ERRORS
REM ****************
:InvalidGameDirectory
echo Error: "%~2" is not a valid game directory.
echo (The -game directory must have a gameinfo.txt file)
goto end

:InvalidInputFile
echo Error: shaders.toml not found in %CD%.
goto end

:NoSourceDirSpecified
echo ERROR: If you specify -game on the command line, you must specify -source.
goto usage
goto end

:NoShaderCompile
echo - ERROR: ShaderCompile26.exe doesn't exist in devtools\bin
goto end

REM ****************
REM BUILD SHADERS
REM ****************
:build_shaders

rem echo --------------------------------
rem echo %inputbase%
rem echo --------------------------------
REM make sure that target dirs exist
REM files will be built in these targets and copied to their final destination
if not exist include mkdir include
if not exist %shaderDir% mkdir %shaderDir%
if not exist %shaderDir%\fxc mkdir %shaderDir%\fxc
REM Nuke some files that we will add to later.

title buildshaders %inputbase%

echo Building inc files and worklist for %inputbase%...

set DYNAMIC=
if "%dynamic_shaders%" == "1" set DYNAMIC=-Dynamic
set GROUPARG=
if defined inputbase set GROUPARG=-Group '%inputbase%'
powershell -NoLogo -ExecutionPolicy Bypass -Command "%SrcDirBase%\devtools\bin\process_shaders.ps1 %DYNAMIC% -ShaderPath '%CD%' %GROUPARG%"

REM ****************
REM PC Shader copy
REM Publish the generated files to the output dir using XCOPY
REM This batch file may have been invoked standalone or slaved (master does final smart mirror copy)
REM ****************
:DoXCopy
if not "%dynamic_shaders%" == "1" (
if not exist "%targetdir%" md "%targetdir%"
if not "%targetdir%"=="%shaderDir%" xcopy %shaderDir%\*.* "%targetdir%" /e /y
)
goto end

REM ****************
REM END
REM ****************
:end


%TTEXE% -diff %tt_start%
echo.