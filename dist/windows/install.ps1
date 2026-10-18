<#
  Fallout 2 SuperCoop installer.

  Builds a separate co-op world folder from the player's own Fallout 2
  (Steam or GOG, US 1.02d), installs the Restoration Project into it the way
  the co-op engine loads it (mods\ + mods_order.txt), adds the co-op
  executables and launchers, and picks a screen mode for this monitor.

  The original game folder is only read, never changed. Run again on an
  existing install to update the executables; saves are kept.

  Windows PowerShell 5.1 compatible (the version every Windows 10/11 ships).
#>
param(
    [string]$GameDir = "",
    [string]$Target = "",
    [string]$RpZip = "",
    [switch]$NoShortcuts,
    [switch]$NoFirewall,
    [switch]$Yes
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue" # Invoke-WebRequest is 10x faster without the progress bar

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackageVersion = (Get-Content -LiteralPath (Join-Path $Here "VERSION.txt") -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $PackageVersion) { $PackageVersion = "dev" }

# The Restoration Project build every player must have. Pinned by hash: co-op
# needs byte-identical game data on the server and every client.
$RpVersion = "2.4.34"
$RpFile = "rpu_v$RpVersion.zip"
$RpUrl = "https://github.com/BGforgeNet/Fallout2_Restoration_Project/releases/download/v$RpVersion/$RpFile"
$RpSha256 = "B8F8A9C900A553C4E781E74BCF805F5663BC37B3633EBFBEBFC928B70470285E"
$RpSizeMb = 827

$UsExeSize = 1189888 # fallout2.exe, US 1.02d (Steam and GOG)
$MarkerName = "supercoop-install.txt"

function Say([string]$text) { Write-Host $text }
function Step([string]$text) { Write-Host ""; Write-Host "== $text" -ForegroundColor Cyan }
function Warn([string]$text) { Write-Host "WARNING: $text" -ForegroundColor Yellow }
function Fail([string]$text) { Write-Host ""; Write-Host "ERROR: $text" -ForegroundColor Red; exit 1 }

function Ask([string]$question, [string]$default) {
    if ($Yes) { return $default }
    $answer = Read-Host "$question [$default]"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $default }
    return $answer.Trim().Trim('"')
}

function AskYesNo([string]$question, [bool]$default) {
    if ($Yes) { return $default }
    $hint = "y/N"
    if ($default) { $hint = "Y/n" }
    $answer = Read-Host "$question ($hint)"
    if ([string]::IsNullOrWhiteSpace($answer)) { return $default }
    return $answer.Trim().ToLower().StartsWith("y")
}

function IsFallout2Dir([string]$dir) {
    if ([string]::IsNullOrWhiteSpace($dir)) { return $false }
    return (Test-Path -LiteralPath (Join-Path $dir "master.dat")) -and (Test-Path -LiteralPath (Join-Path $dir "critter.dat"))
}

function SteamLibraries {
    $libs = @()
    foreach ($key in @("HKCU:\Software\Valve\Steam", "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam", "HKLM:\SOFTWARE\Valve\Steam")) {
        try {
            $item = Get-ItemProperty -Path $key -ErrorAction Stop
            foreach ($name in @("SteamPath", "InstallPath")) {
                $p = $item.$name
                if ($p) { $libs += ($p -replace "/", "\") }
            }
        } catch { }
    }
    $more = @()
    foreach ($root in ($libs | Select-Object -Unique)) {
        $vdf = Join-Path $root "steamapps\libraryfolders.vdf"
        if (Test-Path -LiteralPath $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $more += ($m.Groups[1].Value -replace "\\\\", "\")
            }
        }
    }
    return ($libs + $more) | Select-Object -Unique
}

function FindFallout2 {
    $candidates = @()
    foreach ($lib in SteamLibraries) { $candidates += (Join-Path $lib "steamapps\common\Fallout 2") }
    foreach ($key in @("HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games\1440151285", "HKLM:\SOFTWARE\GOG.com\Games\1440151285")) {
        try { $candidates += (Get-ItemProperty -Path $key -ErrorAction Stop).path } catch { }
    }
    foreach ($drive in (Get-PSDrive -PSProvider FileSystem | ForEach-Object { $_.Root })) {
        $candidates += @(
            (Join-Path $drive "GOG Games\Fallout 2"),
            (Join-Path $drive "Games\Fallout 2"),
            (Join-Path $drive "SteamLibrary\steamapps\common\Fallout 2"),
            (Join-Path $drive "Program Files (x86)\Steam\steamapps\common\Fallout 2"),
            (Join-Path $drive "Program Files (x86)\GOG Galaxy\Games\Fallout 2")
        )
    }
    foreach ($c in ($candidates | Where-Object { $_ } | Select-Object -Unique)) {
        if (IsFallout2Dir $c) { return $c }
    }
    return ""
}

function PickFolder([string]$description) {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description = $description
        if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $dialog.SelectedPath }
    } catch { }
    return ""
}

function SetIniValue([string]$path, [string]$section, [string]$key, [string]$value) {
    # Rewrites "key=..." inside [section], adding the key (and section) when missing.
    $lines = @()
    if (Test-Path -LiteralPath $path) { $lines = @(Get-Content -LiteralPath $path) }
    $out = New-Object System.Collections.Generic.List[string]
    $inSection = $false; $done = $false; $sawSection = $false
    foreach ($line in $lines) {
        if ($line -match '^\s*\[(.+?)\]') {
            if ($inSection -and -not $done) { $out.Add("$key=$value"); $done = $true }
            $inSection = ($Matches[1] -ieq $section)
            if ($inSection) { $sawSection = $true }
        } elseif ($inSection -and -not $done -and $line -match ('^\s*' + [regex]::Escape($key) + '\s*=')) {
            $out.Add("$key=$value"); $done = $true; continue
        }
        $out.Add($line)
    }
    if (-not $done) {
        if (-not $sawSection) { $out.Add(""); $out.Add("[$section]") }
        elseif (-not $inSection) {
            # section exists but was not last: append at its end
            $idx = -1
            for ($i = 0; $i -lt $out.Count; $i++) { if ($out[$i] -match '^\s*\[(.+?)\]' -and $Matches[1] -ieq $section) { $idx = $i } }
            $j = $idx + 1
            while ($j -lt $out.Count -and $out[$j] -notmatch '^\s*\[') { $j++ }
            $out.Insert($j, "$key=$value"); $done = $true
        }
        if (-not $done) { $out.Add("$key=$value") }
    }
    [System.IO.File]::WriteAllLines($path, $out)
}

function ScreenSize {
    try {
        Add-Type -Namespace SuperCoop -Name Dpi -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();' -ErrorAction SilentlyContinue
        [SuperCoop.Dpi]::SetProcessDPIAware() | Out-Null
    } catch { }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        return @($b.Width, $b.Height)
    } catch { return @(1920, 1080) }
}

function CopyTree([string]$from, [string]$to) {
    if (-not (Test-Path -LiteralPath $from)) { return }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    & robocopy $from $to /E /NFL /NDL /NJH /NJS /NP /R:1 /W:1 | Out-Null
    if ($LASTEXITCODE -ge 8) { Fail "Copying $from failed (robocopy code $LASTEXITCODE)." }
}

Write-Host "Fallout 2 SuperCoop installer $PackageVersion" -ForegroundColor Green
Write-Host "Co-op Fallout 2 with the Restoration Project $RpVersion."

# --- 1. The player's Fallout 2 --------------------------------------------------
Step "Finding your Fallout 2"
if (-not $GameDir) { $GameDir = FindFallout2 }
if ($GameDir) {
    Say "Found: $GameDir"
    $GameDir = Ask "Fallout 2 folder" $GameDir
} else {
    Say "Fallout 2 was not found automatically. Pick the folder that contains master.dat."
    $GameDir = PickFolder "Select your Fallout 2 folder (the one with master.dat)"
    if (-not $GameDir) { $GameDir = Ask "Fallout 2 folder" "" }
}
if (-not (IsFallout2Dir $GameDir)) { Fail "'$GameDir' has no master.dat / critter.dat. Point the installer at your Fallout 2 install." }
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$exe = Join-Path $GameDir "fallout2.exe"
if ((Test-Path -LiteralPath $exe) -and ((Get-Item -LiteralPath $exe).Length -ne $UsExeSize)) {
    Warn "This does not look like the US 1.02d release (Steam/GOG). Everyone in a game must use the same version, or the world will not match."
}

# --- 2. Where the co-op world goes -----------------------------------------------
Step "Choosing the co-op folder"
if (-not $Target) {
    $Target = Join-Path (Split-Path -Parent $GameDir) "Fallout 2 SuperCoop"
    $Target = Ask "Install the co-op world to" $Target
}
$Target = [System.IO.Path]::GetFullPath($Target)
if ($Target.TrimEnd("\").StartsWith($GameDir.TrimEnd("\") + "\", [StringComparison]::OrdinalIgnoreCase) -or $Target.TrimEnd("\") -ieq $GameDir.TrimEnd("\")) {
    Fail "The co-op folder must be outside your Fallout 2 folder."
}
$marker = Join-Path $Target $MarkerName
$upgrade = Test-Path -LiteralPath $marker
if (-not $upgrade -and (Test-Path -LiteralPath $Target) -and (Get-ChildItem -LiteralPath $Target -Force | Select-Object -First 1)) {
    Fail "'$Target' already exists and is not a SuperCoop install. Choose an empty or new folder."
}
New-Item -ItemType Directory -Force -Path $Target | Out-Null

if ($upgrade) {
    Say "Existing SuperCoop install found: updating it. Your saves and settings are kept."
} else {
    # --- 3. Base game files (read from the original, never modified) ------------
    Step "Copying the base game files"
    foreach ($f in @("master.dat", "critter.dat", "fallout2.cfg")) {
        $src = Join-Path $GameDir $f
        if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination $Target -Force }
    }
    # The Restoration Project replaces patch000.dat, f2_res.dat and the loose data
    # files; it keeps only the music. Same rule as RP's own installer.
    CopyTree (Join-Path $GameDir "sound\music") (Join-Path $Target "sound\music")
    CopyTree (Join-Path $GameDir "data\sound\music") (Join-Path $Target "data\sound\music")
    Say "Done (about 500 MB)."

    # --- 4. The Restoration Project ------------------------------------------------
    Step "Getting the Restoration Project $RpVersion"
    if (-not $RpZip) {
        foreach ($c in @((Join-Path $Here $RpFile), (Join-Path ([Environment]::GetFolderPath("UserProfile")) "Downloads\$RpFile"))) {
            if (Test-Path -LiteralPath $c) { $RpZip = $c; break }
        }
    }
    $downloaded = $false
    if (-not $RpZip) {
        $RpZip = Join-Path $env:TEMP $RpFile
        if (-not (Test-Path -LiteralPath $RpZip) -or ((Get-FileHash -LiteralPath $RpZip -Algorithm SHA256).Hash -ne $RpSha256)) {
            Say "Downloading $RpFile ($RpSizeMb MB) from the Restoration Project's GitHub releases..."
            try {
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                if (Get-Command Start-BitsTransfer -ErrorAction SilentlyContinue) {
                    Start-BitsTransfer -Source $RpUrl -Destination $RpZip -DisplayName "Restoration Project $RpVersion"
                } else {
                    Invoke-WebRequest -Uri $RpUrl -OutFile $RpZip -UseBasicParsing
                }
            } catch {
                Fail "Download failed: $($_.Exception.Message)`nDownload $RpUrl yourself, put it next to install.cmd and run the installer again."
            }
            $downloaded = $true
        } else {
            Say "Using the copy already downloaded to $RpZip."
        }
    } else {
        Say "Using $RpZip"
    }
    Say "Checking the file..."
    $hash = (Get-FileHash -LiteralPath $RpZip -Algorithm SHA256).Hash
    if ($hash -ne $RpSha256) {
        if ($downloaded) { Remove-Item -LiteralPath $RpZip -Force -ErrorAction SilentlyContinue }
        Fail "$RpFile does not match the expected file (SHA-256 $hash). Every player needs exactly RP $RpVersion."
    }

    Say "Unpacking..."
    $stage = Join-Path $env:TEMP ("supercoop-rp-" + [Guid]::NewGuid().ToString("N"))
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($RpZip, $stage)

    # RP's layout, minus its Windows-only engine (sfall's ddraw.dll, fallout2.exe,
    # the hi-res patch): the co-op engine loads mods\ itself.
    CopyTree (Join-Path $stage "mods") (Join-Path $Target "mods")
    CopyTree (Join-Path $stage "data") (Join-Path $Target "data")
    Copy-Item -LiteralPath (Join-Path $stage "mods_order.txt") -Destination (Join-Path $Target "mods\mods_order.txt") -Force
    Copy-Item -LiteralPath (Join-Path $stage "sfall.dat") -Destination $Target -Force
    Copy-Item -LiteralPath (Join-Path $stage "ddraw.ini") -Destination $Target -Force
    Copy-Item -LiteralPath (Join-Path $stage "f2_res.ini") -Destination $Target -Force
    New-Item -ItemType Directory -Force -Path (Join-Path $Target "data\scripts") | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage "gl_ammomod.int") -Destination (Join-Path $Target "data\scripts") -Force
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue

    $cfg = Join-Path $Target "fallout2.cfg"
    SetIniValue $cfg "sound" "music_path1" "data\sound\music\"
    SetIniValue $cfg "sound" "music_path2" "sound\music\"

    # Co-op settings (sfall config file, read by the co-op engine).
    SetIniValue (Join-Path $Target "ddraw.ini") "Coop" "PerPlayerGlobalScripts" "gl_k_crippl,gl_k_fipswd"

    # --- 5. Screen ------------------------------------------------------------------
    Step "Setting the screen mode"
    $size = ScreenSize
    $w = [int]$size[0]; $h = [int]$size[1]
    $scale = 1
    foreach ($s in @(3, 2)) { if (($w / $s) -ge 640 -and ($h / $s) -ge 480) { $scale = $s; break } }
    $res = Join-Path $Target "f2_res.ini"
    SetIniValue $res "MAIN" "SCR_WIDTH" "$w"
    SetIniValue $res "MAIN" "SCR_HEIGHT" "$h"
    SetIniValue $res "MAIN" "SCALE_2X" "$($scale - 1)"
    SetIniValue $res "MAIN" "WINDOWED" "0"
    Say "${w}x${h}, pixels scaled ${scale}x. Change SCALE_2X in f2_res.ini (0 = 1x, 1 = 2x, 2 = 3x) to taste."
}

# --- 6. Co-op engine and launchers (always refreshed) --------------------------
Step "Installing the co-op engine"
$files = Join-Path $Here "files"
foreach ($f in @("f2_server.exe", "fallout2-ce.exe", "start-server.cmd", "join.cmd", "QUICKSTART.txt")) {
    $src = Join-Path $files $f
    if (-not (Test-Path -LiteralPath $src)) { Fail "The package is incomplete: files\$f is missing." }
    Copy-Item -LiteralPath $src -Destination $Target -Force
}
foreach ($f in @("README.txt", "LICENSE.md", "AUTHORS.md")) {
    $src = Join-Path $Here $f
    if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination $Target -Force }
}
# Files unzipped from a download carry the internet zone mark; clear it so the
# launchers run without a SmartScreen prompt for every file.
Get-ChildItem -LiteralPath $Target -File | Where-Object { $_.Extension -in @(".exe", ".cmd") } | ForEach-Object {
    try { Unblock-File -LiteralPath $_.FullName -ErrorAction Stop } catch { }
}

@(
    "Fallout 2 SuperCoop $PackageVersion",
    "Restoration Project $RpVersion",
    "Installed from: $GameDir",
    "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
) | Set-Content -LiteralPath $marker

# --- 7. Shortcuts and firewall ----------------------------------------------------
if (-not $NoShortcuts) {
    try {
        $shell = New-Object -ComObject WScript.Shell
        $desktop = [Environment]::GetFolderPath("Desktop")
        foreach ($pair in @(@("Fallout 2 SuperCoop - Host", "start-server.cmd"), @("Fallout 2 SuperCoop - Join", "join.cmd"))) {
            $lnk = $shell.CreateShortcut((Join-Path $desktop ($pair[0] + ".lnk")))
            $lnk.TargetPath = Join-Path $Target $pair[1]
            $lnk.WorkingDirectory = $Target
            $lnk.IconLocation = (Join-Path $Target "fallout2-ce.exe") + ",0"
            $lnk.Save()
        }
        Say "Desktop shortcuts: 'Fallout 2 SuperCoop - Host' and '... - Join'."
    } catch { Warn "Could not create desktop shortcuts: $($_.Exception.Message)" }
}

if (-not $NoFirewall -and -not $upgrade) {
    if (AskYesNo "Will you HOST games on this PC? (adds a Windows Firewall rule for the server; needs admin)" $false) {
        $server = Join-Path $Target "f2_server.exe"
        $cmd = "New-NetFirewallRule -DisplayName 'Fallout 2 SuperCoop server' -Direction Inbound -Program '$server' -Action Allow -Profile Private,Domain | Out-Null"
        try {
            Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $cmd
            Say "Firewall rule added (private networks only, which includes ZeroTier/Tailscale)."
        } catch { Warn "Firewall rule not added. Windows will ask the first time the server starts." }
    }
}

Step "Done"
Say "Co-op world: $Target"
Say ""
Say "  Host: run 'Fallout 2 SuperCoop - Host', press Enter for a new game, then run '... - Join'."
Say "  Join: run 'Fallout 2 SuperCoop - Join' and enter the host's address (e.g. their ZeroTier IP)."
Say ""
Say "Everyone needs this same package version and the same Fallout 2 release."
Say "Play over a VPN such as ZeroTier or Tailscale; never forward the game port to the internet."
