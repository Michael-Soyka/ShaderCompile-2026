# ShaderCompile

Standalone shadercompile, that doesn't depend on valve libraries and supports x64.
*It's also removes dependencies on external tools (no perl or DxSdk)*

<img width="1080" height="575" alt="665325095-e12b26c2-7d12-4ac5-ba94-285462d2e6a9" src="https://github.com/user-attachments/assets/3313d8be-7625-4775-adbd-5803e6c6f5a9" />
<p align="center">ShaderCompile26 TUI with 6 workers</p>

All the credit for this codebase for this modification goes to SCell555! *https://github.com/SCell555/ShaderCompile*

## Usage

```batch
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
### General
| Flags                      | Description                                                           |
| -                          | -                                                                     |
| -shaderpath PATH           | Base path for shaders, must contain shaders.toml, required            |
| -group STRINGs             | Compile only these groups from shaders.toml                           |
| -game PATH                 | Copy compiled shaders of selected groups to game directory            |
| -output PATH               | Directory for include and shaders/fxc output, defaults to -shaderpath |
| -crc                       | Calculate crc for shader                                              |
| -dynamic                   | Generate only header                                                  |
| -force                     | Skip crc check during compilation                                     |
| -threads COUNT             | Number of threads used, defaults to core count                        |
| -workers COUNT             | Number of shaders compiled at once, defaults to 4                     |
| -noui                      | Plain progress output instead of the full screen display              |

### Service
| Flags                      | Description                              |
| -                          | -                                        |
| -h, -help                  | Shows help                               |
| -verbose                   | Verbose file cache and final shader info |
| -verbose2                  | Verbose compile commands                 |
| -verbose_preprocessor      | Enables preprocessor debug printing      |

### Optimization
| Flags                      | Description                                                            |
| -                          | -                                                                      |
| -disable-optimization, /Od | Disables shader optimization                                           |
| -disable-preshader, /Op    | Disables preshader generation                                          |
| -no-flow-control, /Gfa     | Directs the compiler to not use flow-control constructs where possible |
| -prefer-flow-control, /Gfp | Directs the compiler to use flow-control constructs where possible     |
| -partial-precision, /Gpp   | Compiles shader with partial precission                                |
| -no-validation, /Vd        | Skips shader validation                                                |

## Parallel build

Up to 4 by default (and specific count with `-workers COUNT`) shaders are compiled at once.

All `-threads COUNT` threads share one pool and take combos from the active shaders in turn, so a small shader or the tail of a big one does not leave cores idle.

For legacy mode use `-workers 1`, it will compile shaders one by one.

## TUI
Now, there is graphical representation of the whole compile process!

Layout guide:
- top: Queue of all shaders. White - compiled, Yellow - warnings, Red - failed, Green - compiling, Gray - waiting;
- middle: shows workers status line by line depends `-workers COUNT`;
- bottom: Status bar.

After the build the console returns to the normal screen with a `<shader> compiled in <time>` line per shader.

The TUI can be turned off by `-noui`, `-verbose`, `-verbose2` flags, redirected output with other scripts or if the terminal window smaller than 60x10.

## Shader model version support

Supported versions: `30`, `40`, `41`, `50`, `51`.

Version from file name suffix:

| Suffix             | Version |
| -                  | -       |
| 30, 40, 41, 50, 51 | same    |
| xx, 3x             | 30      |
| 4x                 | 40      |
| 5x                 | 50      |

## Getting started

Soon...
