# posefid.ps1 - pose-fidelity summary of one run (join side, KENSHICOOP_POSE_FID=1).
# Sums the "[posefid]" bins per regime group and prints, per group: samples,
# horizontal position error p50/p95 (bin upper bounds, u), share over 5 u, yaw
# error p95 (deg) and share over 15 deg, and visible jumps per body-minute; plus
# the snap/park/freeze counts of the same log. Lower is better everywhere.
param([Parameter(Mandatory = $true)][string]$RunDir, [string]$Side = 'join')

$log = Join-Path $RunDir "$Side.log"
$E = @(1, 2, 5, 10, 20, 50, 999); $Y = @(5, 15, 45, 90, 180)
$acc = @{}
foreach ($l in (Select-String -Path $log -Pattern '\[posefid\] grp=')) {
    if ($l.Line -match 'grp=(\w+) n=(\d+) e=([\d,]+) y=([\d,]+) jumps=(\d+) bodyMs=(\d+)(?: r=([\d,]+))?') {
        $g = $matches[1]
        if (-not $acc.ContainsKey($g)) { $acc[$g] = @{ n = 0; e = @(0,0,0,0,0,0,0); y = @(0,0,0,0,0); r = @(0,0,0,0,0); j = 0; ms = 0 } }
        $a = $acc[$g]; $a.n += [int]$matches[2]; $a.j += [int]$matches[5]; $a.ms += [double]$matches[6]
        $ev = $matches[3] -split ','; for ($i = 0; $i -lt 7; $i++) { $a.e[$i] += [int]$ev[$i] }
        $yv = $matches[4] -split ','; for ($i = 0; $i -lt 5; $i++) { $a.y[$i] += [int]$yv[$i] }
        if ($matches[7]) { $rv = $matches[7] -split ','; for ($i = 0; $i -lt 5; $i++) { $a.r[$i] += [int]$rv[$i] } }
    }
}
function Pct($bins, $edges, $p) {
    $tot = ($bins | Measure-Object -Sum).Sum; if ($tot -eq 0) { return 0 }
    $run = 0; for ($i = 0; $i -lt $bins.Count; $i++) { $run += $bins[$i]; if ($run / $tot -ge $p) { return $edges[$i] } }
    return $edges[-1]
}
foreach ($g in ($acc.Keys | Sort-Object)) {
    $a = $acc[$g]; $n = [math]::Max(1, $a.n)
    $over5 = ($a.e[3] + $a.e[4] + $a.e[5] + $a.e[6]) / $n
    $yover = ($a.y[2] + $a.y[3] + $a.y[4]) / $n
    $jpm = if ($a.ms -gt 0) { $a.j / ($a.ms / 60000.0) } else { 0 }
    $rn = [math]::Max(1, ($a.r | Measure-Object -Sum).Sum)
    $rover = ($a.r[2] + $a.r[3] + $a.r[4]) / $rn
    "{0,-7} n={1,6}  pos p50<={2}u p95<={3}u over5u={4:P1}  oriYaw over15={5:P1}  renderYaw p50<={6} p95<={7}deg over15={8:P1}  jumps/body-min={9:N2}" -f `
        $g, $a.n, (Pct $a.e $E 0.5), (Pct $a.e $E 0.95), $over5, $yover, (Pct $a.r $Y 0.5), (Pct $a.r $Y 0.95), $rover, $jpm
}
$cnt = Select-String -Path $log -Pattern '\[snap\] (\w+) |\[combat\] snap |\[census\] park |\[census\] FREEZE|\[puppet\] tp ' |
    ForEach-Object { if ($_.Line -match 'INFO: (\[\w+\] \w+)') { $matches[1] } } | Group-Object | ForEach-Object { "$($_.Name)=$($_.Count)" }
"events: " + ($cnt -join ', ')
$pl = Select-String -Path $log -Pattern '\[puppet\] mode=' | Select-Object -Last 1
if ($pl) { $pl.Line -replace '^.*INFO: ', '' }
