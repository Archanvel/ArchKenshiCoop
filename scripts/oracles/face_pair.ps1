# face_pair.ps1 - host_pc_retreat evidence: does the COPY face the way its OWNER faces?
# Pairs each copy's 10 Hz "SCENARIO FACE" yaw on one client with its owner's yaw on
# the other client LAG_MS earlier (scenario clocks start within a few ms of each
# other; LAG covers the stream/render delay), and reports the share of samples
# where they differ by more than 60 deg - overall, while the owner moves, and
# during the retreat window. Facing is compared directly, so no axis convention
# is needed.
param([Parameter(Mandatory = $true)][string]$RunDir, [int]$LagMs = 200)

function Read-Face([string]$file) {
    $rows = @()
    foreach ($l in (Select-String -Path $file -Pattern 'SCENARIO FACE t=')) {
        if ($l.Line -match 't=(\d+) pc=(\d+),(\d+) view=(\w+) pos=([-\d.]+),([-\d.]+) yaw=([-\d.]+) fight=(\d) run=(\d)') {
            $rows += [pscustomobject]@{ t = [int]$matches[1]; pc = "$($matches[2]),$($matches[3])"; view = $matches[4]
                x = [double]$matches[5]; z = [double]$matches[6]; yaw = [double]$matches[7]
                fight = [int]$matches[8]; run = [int]$matches[9] }
        }
    }
    return $rows
}
function Wrap([double]$a) { while ($a -gt [math]::PI) { $a -= 2 * [math]::PI }; while ($a -lt -[math]::PI) { $a += 2 * [math]::PI }; return $a }

$side = @{ host = Read-Face (Join-Path $RunDir 'host.log'); join = Read-Face (Join-Path $RunDir 'join.log') }
foreach ($pair in @(@('host', 'join'), @('join', 'host'))) {
    $viewer = $pair[0]; $owner = $pair[1]
    $copies = @($side[$viewer] | Where-Object view -eq 'copy')
    foreach ($g in ($copies | Group-Object pc)) {
        $own = @($side[$owner] | Where-Object { $_.view -eq 'own' -and $_.pc -eq $g.Name } | Sort-Object t)
        if ($own.Count -lt 10) { continue }
        $n = 0; $bad = 0; $nMove = 0; $badMove = 0; $nRun = 0; $badRun = 0
        $j = 0
        foreach ($c in ($g.Group | Sort-Object t)) {
            $tt = $c.t - $LagMs
            while ($j + 1 -lt $own.Count -and $own[$j + 1].t -le $tt) { $j++ }
            if ($j -lt 3 -or [math]::Abs($own[$j].t - $tt) -gt 150) { continue }
            $o = $own[$j]; $p = $own[$j - 3]
            $dt = ($o.t - $p.t) / 1000.0
            $v = if ($dt -gt 0) { [math]::Sqrt(($o.x - $p.x) * ($o.x - $p.x) + ($o.z - $p.z) * ($o.z - $p.z)) / $dt } else { 0 }
            $off = [math]::Abs((Wrap ($c.yaw - $o.yaw))) -gt ([math]::PI / 3)
            $n++; if ($off) { $bad++ }
            if ($v -ge 30) { $nMove++; if ($off) { $badMove++ } }
            if ($c.run -eq 1 -and $v -ge 30) { $nRun++; if ($off) { $badRun++ } }
        }
        "{0}'s copy of {1} pc {2}: all {3}/{4}  ownerMoving {5}/{6} ({7:P0})  retreatMoving {8}/{9} ({10:P0})" -f `
            $viewer, $owner, $g.Name, $bad, $n, $badMove, $nMove, $(if ($nMove) { $badMove / $nMove } else { 0 }),
            $badRun, $nRun, $(if ($nRun) { $badRun / $nRun } else { 0 })
    }
}
