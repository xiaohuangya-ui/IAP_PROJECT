param([Parameter(Mandatory=$true)][string]$EvidenceDir, [ValidateSet('Bootloader','Application')][string]$Project = 'Bootloader', [string]$Target = 'BOOT_RELEASE', [string]$ProjectStem = '', [string]$KeilRoot = $env:KEIL_ROOT, [string]$ProjectPath = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$output = Join-Path $projectRoot $EvidenceDir
New-Item -ItemType Directory -Path $output -Force | Out-Null
if (!$ProjectStem) { $ProjectStem = if ($Project -eq 'Application') {'Application_Phase9'} else {'Bootloader'} }
if (!$KeilRoot) { throw 'Set KEIL_ROOT or pass -KeilRoot (directory containing UV4 and ARM).' }
$uv4 = Join-Path $KeilRoot 'UV4\UV4.exe'
$fromelf = Join-Path $KeilRoot 'ARM\ARMCC\bin\fromelf.exe'
$projectFile = Join-Path $projectRoot "$Project\MDK-ARM\$ProjectStem.uvprojx"
if ($ProjectPath) {
    $projectFile = [IO.Path]::GetFullPath((Join-Path $projectRoot $ProjectPath))
    $ProjectStem = [IO.Path]::GetFileNameWithoutExtension($projectFile)
}
[xml]$configuration = Get-Content -LiteralPath $projectFile -Raw
$selected = @($configuration.Project.Targets.Target | Where-Object {$_.TargetName -eq $Target})
if ($selected.Count -ne 1) { throw 'Target must exist exactly once' }
foreach ($stage in @('BeforeCompile','BeforeMake','AfterMake')) {
    $hook = $selected[0].TargetOption.TargetCommonOption.$stage
    foreach ($index in @(1,2)) {
        if ($hook."RunUserProg$index" -eq '1' -and [string]$hook."UserProg${index}Name") { throw "Review required for enabled build command: $stage" }
    }
}
$folder = [IO.Path]::GetFullPath((Join-Path (Split-Path $projectFile -Parent) ([string]$selected[0].TargetOption.TargetCommonOption.OutputDirectory)))
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$log = Join-Path $output "$Target`_rebuild.log"
if (Test-Path -LiteralPath $log) { throw 'Refusing to overwrite existing build evidence' }
& (Join-Path $projectRoot '.venv\Scripts\python.exe') (Join-Path $PSScriptRoot 'snapshot_build_sources.py') $projectFile $output
if ($LASTEXITCODE -ne 0) { throw 'Source snapshot failed before Rebuild' }
Copy-Item -LiteralPath $projectFile -Destination $output
$process = Start-Process -FilePath $uv4 -ArgumentList @('-r', ('"'+$projectFile+'"'), '-t', ('"'+$Target+'"'), '-o', ('"'+$log+'"')) -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) {
    foreach ($ext in @('map','lnp')) {
        if (Test-Path -LiteralPath "$folder\$Target.$ext") { Copy-Item -LiteralPath "$folder\$Target.$ext" -Destination $output }
    }
    throw "Rebuild failed: $($process.ExitCode)"
}
$content = Get-Content -LiteralPath $log -Raw
if ($content -notmatch '0 Error\(s\), 0 Warning\(s\)' -or $content -notmatch 'V5.06 update 7 \(build 960\)') { throw 'Compiler/errors/warnings rejected' }
foreach ($ext in @('axf','map','hex','htm','lnp')) { Copy-Item -LiteralPath "$folder\$Target.$ext" -Destination $output }
Copy-Item -LiteralPath "$folder\$ProjectStem`_$Target.dep" -Destination "$output\$Target`_compiler.dep"
Copy-Item -LiteralPath $projectFile -Destination $output
$responses = Join-Path $output 'responses'
New-Item -ItemType Directory -Path $responses -Force | Out-Null
Get-ChildItem -LiteralPath $folder -File | Where-Object {$_.Name -match '\.(__i|_ia)$'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $responses}
& $fromelf --text -c "--output=$output\$Target`_disassembly.txt" "$output\$Target.axf"
if ($LASTEXITCODE -ne 0) { throw 'fromelf failed' }
& $fromelf --bin "--output=$output\$Target.bin" "$output\$Target.axf"
if ($LASTEXITCODE -ne 0) { throw 'fromelf BIN failed' }
Get-Content -LiteralPath $log -Tail 6
