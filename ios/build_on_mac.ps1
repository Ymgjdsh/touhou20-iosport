param(
    [ValidateSet('iphoneos','iphonesimulator')][string]$Sdk='iphoneos',
    [ValidateSet('th20_ios_platform','th20_ios_archive','th20_ios_engine','th20_ios_host_probe','th20_ios_game','th20_ios_game_smoke')][string]$Target='th20_ios_game',
    [string]$ConnectionFile=$env:TH20_MAC_CONNECTION_FILE
)
$ErrorActionPreference='Stop'
$ConnectionFile=if($ConnectionFile){$ConnectionFile}else{Join-Path $PSScriptRoot 'mac_build.local.psd1'}
if(-not (Test-Path -LiteralPath $ConnectionFile)){throw 'Set TH20_MAC_CONNECTION_FILE or pass -ConnectionFile with a local Mac connection file'}
$root=Split-Path -Parent $PSScriptRoot
$connection=Import-PowerShellDataFile -LiteralPath $ConnectionFile
$destination="$($connection.MacUser)@$($connection.MacHost)"
$key=Join-Path $env:USERPROFILE '.ssh\th07_mac'
$options=@('-i',$key,'-o','IdentitiesOnly=yes','-o','BatchMode=yes','-o','ConnectTimeout=10')
$artifactDirectory=Join-Path $root 'build-native-reports'
New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
$package=Join-Path $artifactDirectory 'th20-native-source.tar.gz'
& python (Join-Path $PSScriptRoot 'tools\package_source.py') --output $package
if($LASTEXITCODE){throw 'Source packaging failed'}
$remote='/Users/'+$connection.MacUser+'/th20-native-port'
if($remote -notmatch '^/Users/[a-zA-Z0-9_-]+/th20-native-port$'){throw 'Invalid isolated remote build path'}
& ssh @options $destination "mkdir -p '$remote/incoming' '$remote/source' '$remote/reports'"
if($LASTEXITCODE){throw 'Remote directory setup failed'}
& scp @options $package "${destination}:$remote/incoming/source.tar.gz"
if($LASTEXITCODE){throw 'Source upload failed'}
# Only overlay this project's named source directory. Never delete old projects.
$command="tar -xzf '$remote/incoming/source.tar.gz' -C '$remote/source' && TH20_SDK=$Sdk TH20_TARGET=$Target bash '$remote/source/ios/build_ios.sh' > '$remote/reports/$Sdk-$Target.log' 2>&1"
& ssh @options $destination $command
$buildExit=$LASTEXITCODE
& scp @options "${destination}:$remote/reports/$Sdk-$Target.log" $artifactDirectory
if($LASTEXITCODE){throw 'Cannot retrieve build log'}
Write-Output "Build log: $(Join-Path $artifactDirectory "$Sdk-$Target.log")"
if($buildExit){throw "Native target $Target failed; inspect saved compiler diagnostics"}
Write-Output "Native target $Target compiled. This alone is not a playable-game or IPA validation."
