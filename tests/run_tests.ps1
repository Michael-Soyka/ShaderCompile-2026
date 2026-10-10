param( [string]$Exe = ( Join-Path $PSScriptRoot '..\build\Release\ShaderCompile26.exe' ) )

$failed = 0
$src = Join-Path $PSScriptRoot 'compute'
$out = Join-Path $PSScriptRoot 'out'

function Check( [string]$Name, [bool]$Ok, [string]$Reason )
{
	if ( $Ok )
	{
		Write-Output "PASS $Name"
	}
	else
	{
		Write-Output "FAIL ${Name}: $Reason"
		$script:failed++
	}
}

function Run( [string]$Groups, [string]$Exe )
{
	Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
	$log = & $Exe -shaderpath $src -output $out -group $Groups -force -noui -workers 4 -threads 8 2>&1 | Out-String
	$result = [pscustomobject]@{ Code = $LASTEXITCODE; Log = $log }
	return $result
}

function RunIn( [string]$Source, [string]$Groups, [string]$Exe )
{
	Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
	$log = & $Exe -shaderpath $Source -output $out -group $Groups -force -noui -workers 4 -threads 8 2>&1 | Out-String
	$result = [pscustomobject]@{ Code = $LASTEXITCODE; Log = $log }
	return $result
}

# compute and SM3 together
$r = Run 'compute,sm3' $Exe
Check 'compute+sm3 exit code' ( $r.Code -eq 0 ) "exit $($r.Code)`n$($r.Log)"
Check 'sm3 vcs' ( Test-Path ( Join-Path $out 'shaders\fxc\test_sm3_ps30.vcs' ) ) 'missing'
foreach ( $case in @( @( 'test_fill_cs60', 3, 'main' ), @( 'test_include_cs60', 1, 'CSMain' ) ) )
{
	$vcs = Join-Path $out "shaders\spirv\$($case[0]).vcs"
	Check "$($case[0]) vcs" ( Test-Path $vcs ) 'missing'
	if ( Test-Path $vcs )
	{
		python ( Join-Path $PSScriptRoot 'verify_spirv.py' ) $vcs $case[1] $case[2] | Write-Output
		Check "$($case[0]) spirv" ( $LASTEXITCODE -eq 0 ) 'verify_spirv failed'
	}
}

$inc = Join-Path $out 'include\test_fill_cs60.inc'
$incText = if ( Test-Path $inc ) { Get-Content $inc -Raw } else { '' }
Check 'cs inc without cshader.h' ( ( $incText -ne '' ) -and ( $incText -notmatch 'cshader\.h' ) -and ( $incText -match 'test_fill_cs60_Static_Index\(\s*\)' ) ) 'inc content'

# warning only compute shader
$r = Run 'compute_warn' $Exe
Check 'warn exit code' ( $r.Code -eq 0 ) "exit $($r.Code)`n$($r.Log)"
Check 'warn vcs' ( Test-Path ( Join-Path $out 'shaders\spirv\test_warn_cs60.vcs' ) ) 'missing'
Check 'warn reported as warning' ( ( $r.Log -match 'warning' ) -and ( $r.Log -notmatch 'ERROR\(S\)' ) ) "log`n$($r.Log)"

# two headers with one file name in a compile
$r = Run 'compute_dupname' $Exe
Check 'dupname exit code' ( $r.Code -ne 0 ) 'exit 0'
Check 'dupname message' ( $r.Log -match 'have the same file name' ) "message`n$($r.Log)"
Check 'dupname no vcs' ( -not ( Test-Path ( Join-Path $out 'shaders/spirv/test_stack_cs60.vcs' ) ) ) 'vcs written'

# broken compute shader
$r = Run 'compute_broken,sm3' $Exe
Check 'broken exit code' ( $r.Code -ne 0 ) 'exit 0'
Check 'broken message' ( $r.Log -match 'g_Undefined' ) "no DXC error text`n$($r.Log)"
Check 'broken no vcs' ( -not ( Test-Path ( Join-Path $out 'shaders\spirv\test_broken_cs60.vcs' ) ) ) 'vcs written'
Check 'broken sm3 still built' ( Test-Path ( Join-Path $out 'shaders\fxc\test_sm3_ps30.vcs' ) ) 'missing'

# no dxcompiler.dll next to the exe
$copy = Join-Path $PSScriptRoot 'out_nodxc'
Remove-Item $copy -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $copy | Out-Null
Copy-Item $Exe $copy
$r = Run 'compute,sm3' ( Join-Path $copy ( Split-Path $Exe -Leaf ) )
Check 'no dxc message' ( $r.Log -match 'dxcompiler\.dll not found next to ShaderCompile26\.exe' ) "message`n$($r.Log)"
Check 'no dxc sm3 built' ( Test-Path ( Join-Path $out 'shaders\fxc\test_sm3_ps30.vcs' ) ) 'missing'
Remove-Item $copy -Recurse -Force -ErrorAction SilentlyContinue

# vs/ps 6.x: SPIR-V in an SM3 shell, shaders/fxc
$r = RunIn ( Join-Path $PSScriptRoot 'spirv' ) 'spirv' $Exe
Check 'spirv exit code' ( $r.Code -eq 0 ) "exit $($r.Code)`n$($r.Log)"
$vs = Join-Path $out 'shaders\fxc\test_shell_vs60.vcs'
$ps = Join-Path $out 'shaders\fxc\test_shell_ps60.vcs'
Check 'spirv vs in fxc' ( Test-Path $vs ) 'missing'
Check 'spirv ps in fxc' ( Test-Path $ps ) 'missing'
if ( Test-Path $vs )
{
	python ( Join-Path $PSScriptRoot 'verify_spirv.py' ) $vs 1 vs '0' | Write-Output
	Check 'spirv vs shell' ( $LASTEXITCODE -eq 0 ) 'verify_spirv failed'
}
if ( Test-Path $ps )
{
	# g_Probe is c5: DXC must put it at 5 * 16
	python ( Join-Path $PSScriptRoot 'verify_spirv.py' ) $ps 4 ps '10,13,36' 'g_Probe=80' | Write-Output
	Check 'spirv ps shell' ( $LASTEXITCODE -eq 0 ) 'verify_spirv failed'
}
$psInc = Join-Path $out 'include\test_shell_ps60.inc'
$psIncText = if ( Test-Path $psInc ) { Get-Content $psInc -Raw } else { '' }
Check 'spirv ps inc with cshader.h' ( $psIncText -match 'cshader\.h' ) 'inc content'

# VS with hull and domain stages: shell v2 for TESS=1, v1 for TESS=0
$r = RunIn ( Join-Path $PSScriptRoot 'spirv' ) 'spirv_tess' $Exe
Check 'tess exit code' ( $r.Code -eq 0 ) "exit $($r.Code)`n$($r.Log)"
$tess = Join-Path $out 'shaders\fxc\test_tess_vs60.vcs'
Check 'tess vcs' ( Test-Path $tess ) 'missing'
if ( Test-Path $tess )
{
	python ( Join-Path $PSScriptRoot 'verify_spirv.py' ) $tess 2 vs-stages 1 '0' | Write-Output
	Check 'tess shell v2' ( $LASTEXITCODE -eq 0 ) 'verify_spirv failed'
}

# combo larger than the engine unpack block
$r = RunIn ( Join-Path $PSScriptRoot 'spirv' ) 'spirv_big' $Exe
Check 'big combo exit code' ( $r.Code -ne 0 ) 'exit 0'
Check 'big combo message' ( $r.Log -match 'combo is \d+ bytes, the engine limit is 131055 bytes' ) "message`n$($r.Log)"
Check 'big combo no vcs' ( -not ( Test-Path ( Join-Path $out 'shaders\fxc\test_big_ps60.vcs' ) ) ) 'vcs written'

Write-Output "failed: $failed"
exit $failed
