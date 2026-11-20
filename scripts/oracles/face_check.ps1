# face_check.ps1 - host_pc_retreat evidence: does a RUNNING body face where it goes?
# Reads the "SCENARIO FACE" 10 Hz series (pos + yaw, own body and peer copy) from
# host.log/join.log of one run and reports, per leader and view, the share of
# running samples (speed >= 30 u/s over 0.3 s) whose facing is more than 60 deg
# off the direction of travel. The yaw->travel offset (engine axis convention) is
# calibrated from the OWN bodies' samples, so the copy is judged against how a
# body the engine itself drives faces while running.
param([Parameter(Mandatory = $true)][string]$RunDir, [double]$MinSpeed = 30)

function Read-Face([string]$file, [string]$side) {
    $rows = @()
    foreach ($l in (Select-String -Path $file -Pattern 'SCENARIO FACE t=')) {
        if ($l.Line -match 't=(\d+) pc=(\d+),(\d+) view=(\w+) pos=([-\d.]+),([-\d.]+) yaw=([-\d.]+) fight=(\d) run=(\d)') {
            $rows += [pscustomobject]@{ side = $side; t = [int]$matches[1]; pc = "$($matches[2])"; view = $matches[4]
                x = [double]$matches[5]; z = [double]$matches[6]; yaw = [double]$matches[7]
                fight = [int]$matches[8]; run = [int]$matches[9] }
        }
    }
    return $rows
}
function Wrap([double]$a) { while ($a -gt [math]::PI) { $a -= 2 * [math]::PI }; while ($a -lt -[math]::PI) { $a += 2 * [math]::PI }; return $a }

$all = @(Read-Face (Join-Path $RunDir 'host.log') 'host') + @(Read-Face (Join-Path $RunDir 'join.log') 'join')
$samples = @()
foreach ($g in ($all | Group-Object side, pc, view)) {
    $r = @($g.Group | Sort-Object t)
    for ($i = 3; $i -lt $r.Count; $i++) {
        $a = $r[$i - 3]; $b = $r[$i]
        $dt = ($b.t - $a.t) / 1000.0
        if ($dt -le 0.05 -or $dt -gt 0.6) { continue }
        $dx = $b.x - $a.x; $dz = $b.z - $a.z
        $v = [math]::Sqrt($dx * $dx + $dz * $dz) / $dt
        if ($v -lt $MinSpeed) { continue }
        $samples += [pscustomobject]@{ side = $b.side; pc = $b.pc; view = $b.view; run = $b.run; fight = $b.fight
            raw = (Wrap ($b.yaw - [math]::Atan2($dx, $dz))) }
    }
}
$own = @($samples | Where-Object view -eq 'own')
if ($own.Count -eq 0) { "no running own samples"; exit 1 }
# circular mean of the own bodies' raw offset = the axis convention
$sx = ($own | ForEach-Object { [math]::Sin($_.raw) } | Measure-Object -Sum).Sum
$cx = ($own | ForEach-Object { [math]::Cos($_.raw) } | Measure-Object -Sum).Sum
$off = [math]::Atan2($sx, $cx)
"offset(rad)={0:N2} from {1} own running samples" -f $off, $own.Count
foreach ($g in ($samples | Group-Object side, pc, view)) {
    $n = $g.Group.Count
    $bad = @($g.Group | Where-Object { [math]::Abs((Wrap ($_.raw - $off))) -gt ([math]::PI / 3) }).Count
    $badRun = @($g.Group | Where-Object { $_.run -eq 1 -and [math]::Abs((Wrap ($_.raw - $off))) -gt ([math]::PI / 3) }).Count
    $nRun = @($g.Group | Where-Object run -eq 1).Count
    "{0,-22} running={1,4} offFacing={2,4} ({3:P0})  retreat: {4}/{5}" -f $g.Name, $n, $bad,
        ($(if ($n) { $bad / $n } else { 0 })), $badRun, $nRun
}
