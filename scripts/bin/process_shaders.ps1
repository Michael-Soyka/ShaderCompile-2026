[CmdletBinding()]
param (
    [Parameter(Mandatory=$true)][string]$ShaderPath,
    [Parameter(Mandatory=$false)][string[]]$Group,
    [Parameter(Mandatory=$false)][switch]$Dynamic,
    [Parameter(Mandatory=$false)][System.UInt32]$Threads
)

$arguments = @("-shaderpath", $ShaderPath)
foreach ($name in $Group) {
    $arguments += @("-group", $name)
}
if ($Dynamic) {
    $arguments += "-dynamic"
}
if ($Threads -ne 0) {
    $arguments += @("-threads", $Threads)
}

& "$PSScriptRoot\ShaderCompile" @arguments
exit $LASTEXITCODE
