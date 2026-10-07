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

Write-Output "failed: $failed"
exit $failed
