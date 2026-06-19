param(
  [string]$ToolsRoot,
  [string]$CertificateBase64 = $env:WINDOWS_CERT_PFX_BASE64
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
  if ([string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    $ToolsRoot = Join-Path ([System.IO.Path]::GetTempPath()) "strata-windows-signing"
  } else {
    $ToolsRoot = Join-Path $env:RUNNER_TEMP "strata-windows-signing"
  }
}

function Add-CiEnvironment {
  param(
    [string]$Name,
    [string]$Value
  )

  Set-Item -Path "Env:$Name" -Value $Value

  if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_ENV)) {
    "$Name=$Value" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
  }
}

function Add-CiOutput {
  param(
    [string]$Name,
    [string]$Value
  )

  if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_OUTPUT)) {
    "$Name=$Value" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
  }
}

function Get-NuGetExe {
  $existing = Get-Command nuget.exe -ErrorAction SilentlyContinue
  if ($existing) {
    return $existing.Source
  }

  $nugetExe = Join-Path $ToolsRoot "nuget.exe"
  if (-not (Test-Path -LiteralPath $nugetExe)) {
    Write-Host "Downloading nuget.exe..."
    Invoke-WebRequest -Uri "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $nugetExe
  }

  return $nugetExe
}

function Install-NuGetPackage {
  param(
    [string]$NuGetExe,
    [string]$PackageId
  )

  $packagesRoot = Join-Path $ToolsRoot "packages"
  New-Item -ItemType Directory -Force -Path $packagesRoot | Out-Null

  & $NuGetExe install $PackageId -OutputDirectory $packagesRoot -NonInteractive -DirectDownload | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw "NuGet package install failed for '$PackageId'."
  }

  $package = Get-ChildItem -LiteralPath $packagesRoot -Directory |
    Where-Object { $_.Name -like "$PackageId.*" -or $_.Name -eq $PackageId } |
    Sort-Object Name -Descending |
    Select-Object -First 1

  if (-not $package) {
    throw "NuGet package '$PackageId' was installed but could not be found under '$packagesRoot'."
  }

  return $package.FullName
}

function Resolve-LatestFile {
  param(
    [string]$Root,
    [string]$Filter,
    [string]$RequiredPathFragment
  )

  $candidates = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $Filter |
    Where-Object {
      [string]::IsNullOrWhiteSpace($RequiredPathFragment) -or
      $_.FullName -like "*$RequiredPathFragment*"
    } |
    Sort-Object FullName -Descending)

  if (($candidates | Measure-Object).Count -eq 0) {
    throw "Could not find '$Filter' under '$Root'."
  }

  return $candidates[0].FullName
}

New-Item -ItemType Directory -Force -Path $ToolsRoot | Out-Null

if ([string]::IsNullOrWhiteSpace($CertificateBase64)) {
  throw "Missing required environment variable 'WINDOWS_CERT_PFX_BASE64' for Windows code signing."
}

# Resolve signtool.exe from the Windows SDK Build Tools (kept on NuGet so the
# runner image version does not matter).
$nugetExe = Get-NuGetExe
$sdkPackage = Install-NuGetPackage -NuGetExe $nugetExe -PackageId "Microsoft.Windows.SDK.BuildTools"
$signToolPath = Resolve-LatestFile -Root $sdkPackage -Filter "signtool.exe" -RequiredPathFragment "\x64\"

# Materialize the PFX from its base64 secret to a temp file. The path is not
# secret; the password is passed separately at signing time and never persisted.
$certificatePath = Join-Path $ToolsRoot "strata-codesign.pfx"
try {
  $certificateBytes = [System.Convert]::FromBase64String($CertificateBase64.Trim())
} catch {
  throw "WINDOWS_CERT_PFX_BASE64 is not valid base64: $($_.Exception.Message)"
}
[System.IO.File]::WriteAllBytes($certificatePath, $certificateBytes)

Add-CiEnvironment -Name "STRATA_SIGNTOOL_PATH" -Value $signToolPath
Add-CiEnvironment -Name "STRATA_WINDOWS_CERT_PATH" -Value $certificatePath
Add-CiEnvironment -Name "STRATA_WINDOWS_CODESIGN_READY" -Value "true"

Add-CiOutput -Name "signtool" -Value $signToolPath
Add-CiOutput -Name "certificate" -Value $certificatePath

Write-Host "Windows code signing tools ready."
Write-Host "SignTool: $signToolPath"
Write-Host "Certificate: $certificatePath"
