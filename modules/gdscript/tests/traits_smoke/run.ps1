# Run the trait language-feature smoke test (headless, no window).
# Usage: powershell -File run.ps1   (or pass -Engine <path to console exe>)
param(
	[string]$Engine = ""
)

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
if ([string]::IsNullOrEmpty($Engine)) {
	$Engine = Join-Path $repo "bin\godot.windows.editor.dev.x86_64.console.exe"
}

if (-not (Test-Path $Engine)) {
	Write-Error "Engine not found: $Engine (build first, or pass -Engine <path>)"
	exit 1
}

& $Engine --headless --path $PSScriptRoot --script res://run_smoke.gd
