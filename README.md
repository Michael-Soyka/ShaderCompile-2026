# ShaderCompile
Standalone shadercompile, that doesn't depend on valve libraries and supports x64. Also removes dependencies
on external tools (no perl or DxSdk)
## Usage
```
ShaderCompile26.exe [OPTIONS] -shaderpath src_dir
```
Shaders are listed in `src_dir/shaders.toml`, grouped by tables:
```toml
[lightmappedgeneric]
version = '30'   # optional, otherwise taken from file name suffix
files = [ 'lightmappedgeneric/lightmappedgeneric_ps3x.fxc', 'lightmappedgeneric/lightmappedgeneric_vs30.fxc' ]
```
Paths are relative to `src_dir`. `#include` is resolved relative to the including file first, then relative to `src_dir`.
Without `-group` all groups are compiled.
## Options
```
-shaderpath ARG                Base path for shaders, must contain shaders.toml, required
-group ARG                     Compile only these groups from shaders.toml
-game ARG                      Copy compiled shaders of selected groups to game directory
-output ARG                    Directory for include and shaders/fxc output, defaults to -shaderpath
-crc                           Calculate crc for shader
-dynamic                       Generate only header
-force                         Skip crc check during compilation
-threads ARG                   Number of threads used, defaults to core count

-h, -help                      Shows help
-verbose                       Verbose file cache and final shader info
-verbose2                      Verbose compile commands
-verbose_preprocessor          Enables preprocessor debug printing

-disable-optimization, /Od     Disables shader optimization
-disable-preshader, /Op        Disables preshader generation
-no-flow-control, /Gfa         Directs the compiler to not use flow-control constructs where possible
-prefer-flow-control, /Gfp     Directs the compiler to use flow-control constructs where possible
-partial-precision, /Gpp       Compiles shader with partial precission
-no-validation, /Vd            Skips shader validation
```
## Shader model version support
Minimum shader model is 3.0. Supported versions: `30`, `40`, `41`, `50`, `51`.
&NewLine;  
&NewLine;  
Version from file name suffix
```
30, 40, 41, 50, 51   as is
xx, 3x               30
4x                   40
5x                   50
```
Shaders below 3.0 (`2x`, `20`, `20b`) are skipped with a warning.
## Getting started
This assumes you have "clean" Source SDK2013 project.
1. In `game_shader_dx9_base.vpc` replace `$AdditionalIncludeDirectories	"$BASE;fxctmp9;vshtmp9;"`
 with `$AdditionalIncludeDirectories	"$BASE;include"` , shader headers will be now located in more sensible place
2. Replace `cshader.h` in public/shaderlib with one from this repo, if you are using VS2013 compiler use the one from
VS2013 folder
3. Place `ShaderCompile26.exe` and `process_shaders.ps1` to devtools/bin folder where `vpc.exe` is located
4. Replace `buildshaders.bat` with one from this repo
5. Put `shaders.toml` into the shader source folder instead of `<project>.txt` lists, and in `buildsdkshaders.bat`
 pass a group name (or `all`) instead of the project name, without `-dx9_30` and `-force30`, so
    ```batch
    %BUILD_SHADER% stdshader_dx9_30 -game %GAMEDIR% -source %SOURCEDIR% -dx9_30 -force30 
    ```
    looks like
    ```batch
    %BUILD_SHADER% all -game %GAMEDIR% -source %SOURCEDIR%
    ```
6. Optionally remove all perl scripts for compiling shaders from devtools/bin, as they will be never used again
    ```
    buildshaderlist.pl
    checkshaderchecksums.pl
    copyshaderincfiles.pl
    copyshaders.pl
    fxc_prep.pl
    psh_prep.pl
    shaderinfo.pl
    uniqifylist.pl
    updateshaders.pl
    valve_perl_helpers.pl
    vsh_prep.pl
    ```