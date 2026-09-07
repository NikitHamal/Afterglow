# Afterglow 3D — headless capture pipeline.
# Renders posed screenshots with system Chrome (software GL) so the agent
# can SEE the scene, judge it, and iterate.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .shots\shoot.ps1
#   powershell -ExecutionPolicy Bypass -File .shots\shoot.ps1 -Only pose0,oral
param(
  [string]$OutDir = ".shots",
  [string[]]$Only = @(),
  [int]$Budget = 14000
)
$ErrorActionPreference = 'Stop'
$root = 'F:\Afterglow'
$chrome = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
if (-not (Test-Path -LiteralPath $chrome)) {
  $chrome = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
}
New-Item -ItemType Directory -Force -Path (Join-Path $root $OutDir) | Out-Null

# pose survey: name -> URL query (clean UI, full depth, settled physics)
$shots = @(
  @{ n = 'pose0-missionary';  q = 'pose=0&clean=1' },
  @{ n = 'pose0-close';       q = 'pose=0&clean=1&dist=0.65&yaw=0.42&pitch=0.10&tx=0&ty=0.30&tz=-0.50' },
  @{ n = 'pose0-foot';        q = 'pose=0&clean=1&dist=3.0&yaw=0.0&pitch=0.05' },
  @{ n = 'pose0-crotch';      q = 'pose=0&clean=1&dist=0.7&yaw=0.0&pitch=-0.03&tx=0&ty=0.28&tz=-0.48' },
  @{ n = 'pose0-side';        q = 'pose=0&clean=1&dist=1.0&yaw=0.9&pitch=-0.02&tx=0&ty=0.26&tz=-0.45' },
  @{ n = 'pose1-legsup';      q = 'pose=1&clean=1' },
  @{ n = 'pose2-doggy';       q = 'pose=2&clean=1' },
  @{ n = 'pose2-behind';      q = 'pose=2&clean=1&dist=1.0&yaw=3.14&pitch=0.15&tx=0&ty=0.50&tz=-0.30' },
  @{ n = 'pose3-prone';       q = 'pose=3&clean=1' },
  @{ n = 'pose4-cowgirl';     q = 'pose=4&clean=1' },
  @{ n = 'pose5-revcow';      q = 'pose=5&clean=1' },
  @{ n = 'pose6-spoon';       q = 'pose=6&clean=1' },
  @{ n = 'oral';              q = 'pose=0&oral=1&clean=1' },
  @{ n = 'oral-close';        q = 'pose=0&oral=1&clean=1&dist=0.9&yaw=1.2&pitch=0.15&tx=0&ty=0.32&tz=-0.42' },
  @{ n = 'fpv-spot';          q = 'pose=0&view=fpv&focus=hips&clean=1' },
  @{ n = 'fpv-full';          q = 'pose=0&view=fpv&focus=full&clean=1' },
  @{ n = 'fpv-breasts';       q = 'pose=0&view=fpv&focus=breasts&clean=1' },
  @{ n = 'fpv-face';          q = 'pose=0&view=fpv&focus=face&clean=1' }
)

foreach ($s in $shots) {
  if ($Only.Count -gt 0 -and -not ($Only -contains $s.n)) { continue }
  $url = "file:///F:/Afterglow/3d.html?$($s.q)"
  $out = Join-Path $root (Join-Path $OutDir ($s.n + '.png'))
  Write-Host "shooting $($s.n) ..."
  & $chrome --headless=new --no-sandbox --disable-dev-shm-usage `
    --use-angle=swiftshader --enable-unsafe-swiftshader `
    --hide-scrollbars --window-size=1280,720 `
    "--virtual-time-budget=$Budget" "--screenshot=$out" "$url"
  if (Test-Path -LiteralPath $out) {
    $kb = [math]::Round((Get-Item -LiteralPath $out).Length / 1KB)
    Write-Host "  ok $kb KB"
  } else {
    Write-Host '  FAILED (no output — WebGL unavailable headless?)'
  }
}
Write-Host 'done.'
