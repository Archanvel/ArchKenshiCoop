# Arch Kenshi Coop

A bug-fix fork of [Savas07/KenshiCoop](https://github.com/Savas07/KenshiCoop)
v0.59.1, which itself continues [nhoral/KenshiCoop](https://github.com/nhoral/KenshiCoop).
All credit for the mod goes to its original authors; this fork adds fixes found
while playing two-player sessions, each one checked on the two-client test
harness. Version string: `0.59.1-fixN`. **Wire protocol 67** - not compatible
with v0.59.1 (66), so every player needs the same build.

Форк с исправлениями поверх Savas07/KenshiCoop v0.59.1. У всех игроков должна
стоять одна и та же сборка.

### Changes since v0.59.1 (fix1 - fix10)

- **Host crash** when a peer picked up the copy of an item the host had sold to
  a trader: a sale is no longer treated as a ground drop, and the item pointer
  is re-validated before use.
- **Role set-up for players joining from the menu / F2 panel.** The plugin
  started every game as host and did not fully switch roles on connect, so a
  joined player's hits did no damage on the host, their containers and shop
  stock overwrote the host's, and traders restocked on both sides. Roles are
  now re-applied on connect and reset on disconnect.
- **Stuck world transfer:** a join whose save download failed waited in the
  menu forever; it now re-requests, and the host does not re-save mid-transfer.
- **Unconscious timer:** a knocked-out copy showed a frozen "8 s"; it now shows
  and counts down the owner's real timer (protocol 67). Carried or bedded
  bodies no longer wake early.
- **Beds:** a copy that got up next to the bed it lay in is put back into that
  exact bed (beds were matched by English names, which fails on localised games).
- **Trader money** growing by itself (one shop's cash was counted twice; NPC
  copies "bought" goods on the join).
- **Corpses lying in different places** for host and join: ragdolled bodies
  are now actually moved (kill ragdoll, teleport, re-ragdoll in one frame).
- **Crawling NPC flickering** knocked out / revived on the join.
- Desync fixes: negative limb health, healing rolled back during bandaging,
  stuck knock-out state, NPCs vanishing for 20-30 s, guards hidden after the
  host leaves, frozen copy of a carried NPC that woke up, attack-order spam in
  crowds, combat rubber-banding, bodies dropping out of mid-range sync, zone
  hand-over grace period, stale records after a rejoin, stuck speech bubbles.
- **New options** in `coop_config.json`: `speedPolicy`, `speedCombatCap`,
  `healMult` (see below); the world pauses while a friend is loading in.

Upstream README follows.

---

Setup + Demo: [https://www.youtube.com/watch?v=OqwVRRZEYGM](https://www.youtube.com/watch?v=OqwVRRZEYGM)

Experimental **co-op multiplayer for [Kenshi](https://lofigames.com/)**, built as an
[RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) /
[KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) plugin.

One player hosts their world; a friend connects (LAN, direct UDP, or Steam P2P)
and plays their own squad inside it. The plugin replicates squads, NPCs, combat,
inventory and equipment, direct trades between the players' squads, items
dropped on the ground (both directions), base building and container contents,
one shared money pool, game speed, and more - and saves are coordinated: any save
either player makes becomes one shared save, streamed to both machines
automatically.

> **Status: work in progress.** This is a hobby project under active
> development. Expect rough edges, desyncs, and crashes. Sessions support up
> to 8 players (host + 7); 2 players is the most-tested setup.

## How it works

- `KenshiCoop.dll` is loaded into the game by RE_Kenshi. It hooks the engine via
  KenshiLib and drives all game mutation on the main thread.
- Networking is [ENet](https://github.com/lsalzman/enet) over UDP, with an
  optional Steam P2P tunnel (no port forwarding needed).
- The host is authoritative for the world; each client is authoritative for its
  own squad. See `docs/API_REFERENCE.md` for the full engine-control surface and
  wire protocol.

```
src/plugin/       The KenshiCoop plugin (net, sync/replication, engine facade, scenarios)
src/netproto/     Shared wire-protocol headers (plain C++03, compiled by everything)
src/nettest/      Standalone ENet console app (transport de-risking)
src/netsim/       Protocol simulator
src/prototest/    Wire-protocol unit tests
src/tunneltest/   Steam-tunnel socket-hook tests
scripts/          Build, deploy, session, and automated-test tooling (PowerShell)
docs/             Build guide, engine/API reference, replication pitfalls
third_party/      ENet patches, VC10 compat shim (deps are fetched, not committed)
```

## Try it (play with a friend)

Up to 8 players, one machine each. Everything happens **inside the game**:
Kenshi's main menu gets a **MULTIPLAYER** button that opens the co-op lobby. The
host opens a lobby, invites Steam friends (listed with their Steam names and
avatars) and starts a new game or loads a save from there; friends accept the
Steam invite and land in the host's world. Nobody types or swaps a Steam ID,
edits a config file or runs a launcher script. (A tiny `coop_config.json` is
only needed for LAN / direct-UDP games.)

### Before you start (both players)

1. **Kenshi 1.0.65 (Steam)**.
2. **[RE_Kenshi 0.3.1+](https://www.nexusmods.com/kenshi/mods/847)** installed
   (free Nexus mod - it loads the co-op plugin into the game).
3. **Steam running and online** on both machines. That's the whole network
   setup: the connection is Steam P2P, so there's no port forwarding, no
   router configuration, and no IP addresses. (A direct-UDP mode is also
   available for LAN / port-forwarded games.)

### 1. Install the mod

Grab `KenshiCoop-kit-v0.57.3.zip` from the
[latest release](https://github.com/Savas07/KenshiCoop/releases/latest) and
unzip it anywhere (every player). You do not need to clone this repository -
but if you did, the same kit is in [dist/mod-kit](dist/mod-kit).

The zip contains a single **`KenshiCoop`** folder. Copy that folder into your
Kenshi `mods` directory so you end up with
`<Kenshi>\mods\KenshiCoop\KenshiCoop.dll` (default Steam path:
`C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\`). Then launch
Kenshi and enable **KenshiCoop** in the Mods menu.

### 2. Play together: the MULTIPLAYER lobby

1. **Host:** on Kenshi's main menu click **MULTIPLAYER**, then **HOST GAME**.
   The lobby shows everyone in it (Steam name + avatar) and your online Steam
   friends (friends already in Kenshi first; page with **<** / **>**). Click
   **INVITE** next to each friend - they get a Steam invite and appear under
   *Players* when they accept. Then either:
   - **NEW GAME** - opens Kenshi's own New Game window with the
     **Multiplayer (Wanderer xN)** start for your lobby size already selected
     (you can still pick any start), or
   - **LOAD GAME** - lists your saves; click **LOAD** next to the one to play.

   Friends in the lobby wait there until your world is running (after your own
   character creation on a new game), then join it automatically.
2. **Join:** accept the Steam invite, or on the main menu click **MULTIPLAYER**
   and **JOIN** next to a friend who is hosting, or use **Join Game** on the
   host in your Steam friends list. The lobby opens and shows who is in it; once
   the host's world is up it is streamed to you and you load right into it - no
   save needed. If Kenshi isn't running when you accept the invite, Steam starts
   it and you join once the main menu is up. (If you already have an identical
   copy of the host's save on disk, it's used as-is instead of transferring.)
3. In-game, **F2** opens the co-op panel: live status, **Invite** more friends
   (the list stays available while you play) and **Leave co-op session**.

If the host runs a different KenshiCoop version, their entry reads *different
mod version* and joining is refused - every player needs the same release.

**Advanced (manual Steam ID / LAN):** press **F2** and click **Advanced** for the
original controls - Role / Transport / Connection toggles plus **Copy my Steam
ID** / **Paste friend's Steam ID** (a per-session clipboard swap; nothing is
written to disk). Use it when Steam invites aren't available, or for
LAN / direct-UDP: open `<Kenshi>\mods\KenshiCoop\coop_config.json`, set
`"transport": "udp"`, and put the host's address in `"ip"` / `"port"`. Then set
**Transport: UDP** and toggle **Connection** to **ONLINE**. The `ip`/`port` are
re-read whenever you go ONLINE, so no restart is needed after an edit.

### Good to know

- **You each control your own squad.** One squad tab per player: the host runs
  squad 1, the first friend to join squad 2, the next squad 3, and so on. Each
  squad tab is named after the Steam name of the player who controls it, and the
  save remembers who had which squad - next session everyone gets their own
  squad back, whatever order you join in. Someone new playing instead of a
  regular takes over the absent player's squad. Other players' squads are visible and
  synced on your screen, but answer only to them. Your save needs at least as
  many squad tabs as players - start with the matching Multiplayer start (the
  lobby's NEW GAME picks it for you), or split units into extra squad tabs.
  Tabs you renamed yourself keep your name.
- **More than two players.** The host relays every player's state to the
  others, so the host's upload carries everyone - the host should have the
  best connection. A friend joining mid-game loads into the host's world; the
  players already in the game keep playing.
- **Co-op starts included.** The KenshiCoop mod ships four game starts (New
  Game -> pick one from the list), all the vanilla Wanderer start with the
  wanderers already split into separate squads, so the host gets squad 1 and the
  joining player squad 2 with no manual tab-splitting:
  - **"Multiplayer (Wanderer x2)"** - the plain version, vanilla in every other
    way. Authored by [zeroit789](https://github.com/zeroit789).
  - **"Multiplayer+ (Wanderer x2)"** - the same start with **500,000 cats** and both
    characters at **50 in every stat**, for skipping the early grind. Kenshi has
    a single player wallet and co-op shares it, so the 500,000 is the pair's
    combined purse, not 500,000 each. The stats are a floor applied when the
    world loads, so training past 50 sticks normally.
  - **"Multiplayer (Wanderer x8)"** / **"Multiplayer+ (Wanderer x8)"** - the
    same two starts with **eight** wanderers, one per squad: the host gets squad 1
    and joining players take squads 2-8 in join order. Fewer than eight players is
    fine - unclaimed squads simply stay with the host. The "+" version gives every
    character the same 50-in-every-stat floor and the shared 500,000 cats.
- **The joining player doesn't need the host's save.** The host picks the save
  (or starts a new game); when the join connects from the menu, the host's world
  is streamed over automatically. Already having an identical copy on disk just
  skips the transfer.
- **Saving just works.** Any save either player makes during a session becomes
  one shared save on both machines, streamed to the other side automatically.
  To resume next time, the host loads that save from the lobby (**LOAD GAME**),
  and the friends join again from the main menu.
- **New games get their own save.** When a friend joins a new game, the host's
  world is first saved as **KenshiCoop YYYY-MM-DD HHMM** - it never overwrites
  one of your existing saves.
- **Character creation for joining players.** On a new game started with a
  Multiplayer start, each joining player designs their own character in Kenshi's
  character editor right after loading in (about 8 s after their world appears;
  the game stays paused for everyone until they confirm). Loading an existing
  save never opens the editor - established characters are never re-rolled.
- **Game speed and healing rate are yours to tune.** Three optional keys in
  `<Kenshi>\mods\KenshiCoop\coop_config.json` (read when Kenshi starts):
  - `"speedPolicy"` - who sets the speed. `"min"` (default) is the consensus
    rule: the world runs at the slowest speed any player has selected, so
    everyone has to click fast-forward. `"last"` makes the most recent click by
    any player the speed for everybody. `"host"` gives the buttons to the host.
    Only the **host's** value matters. Pausing is the same in all three: anyone
    can pause, and the world resumes when nobody is holding a pause.
  - `"speedCombatCap"` - `true` (default) pins the world to 1x while any
    player squad is fighting; `false` lets you fast-forward through it.
  - `"healMult"` - how fast player-squad characters heal, `1` = vanilla, e.g.
    `3` for three times faster (0.1 - 20). It changes healing only - bleeding,
    hunger and the clock are untouched. Each machine applies its own value to
    the squads it simulates, so all players should use the same number.

  On a join's screen the speed buttons keep showing that player's own last
  click, which under `"last"` / `"host"` may not be the speed in force.

### If something goes wrong

- **"The co-op plugin has not started"** - RE_Kenshi didn't load it. Check
  `<Kenshi>\RE_Kenshi_log.txt` for `KenshiCoop`; reinstalling
  [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) usually fixes it.
- **No connection (Steam)** - both Steams must be online (not offline mode).
  A friend only shows up under *Friends hosting* in the lobby (and *Join a
  friend* in F2) while they are hosting (clicked **HOST GAME** / **Host co-op**)
  and Steam reports them in Kenshi; the lists refresh every few seconds. With the
  Advanced manual flow, each side must have **Pasted** the *other* player's ID.
  Look for `[invite]` / `[lobby]` lines and `[steam] session ... active=1` in
  `<Kenshi>\KenshiCoop_*.log`.
- **"protocol mismatch" in the log** - one of you has an older build; both
  players should re-install from the same release.

The kit's `README.txt` has the full setup + troubleshooting list.

## Building

The plugin must be compiled with the **Visual C++ 2010 (v100) x64 toolset** (a
KenshiLib requirement). Full toolchain setup, gotchas, and install steps are in
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md). Short version, once prerequisites
are in place:

```bash
cmd //c scripts/build_plugin.cmd
```

Dependencies are fetched, not committed:

- KenshiLib + precompiled libs: clone
  [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps)
  into `third_party/KenshiLib_deps/`
- ENet: clone [lsalzman/enet](https://github.com/lsalzman/enet) into
  `third_party/enet/enet/` and apply the patches in `third_party/enet/patches/`
  (see `third_party/enet/README.md`)

## Development and testing

`scripts/` contains an automated two-client test harness: `dev_cycle.ps1`
rebuilds, deploys to two local installs, launches host + join, runs a named
scenario, and produces a numeric PASS/FAIL verdict from the two logs.
`regress.ps1` runs the scenario regression suite. See
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md) Parts D-E for details.

Sync debugging tools (in-world markers, live dashboard, desync watchdog,
packet recording and replay) are built into every DLL and switched on with
`"devtools": 1` in `coop_config.json` - see [docs/DEVTOOLS.md](docs/DEVTOOLS.md).

## Credits

- [nhoral](https://github.com/nhoral) - original KenshiCoop
- [Savas07](https://github.com/Savas07) - this fork: Steam lobby, Workshop
  release, sync fixes (2026)
- [BFrizzleFoShizzle](https://github.com/BFrizzleFoShizzle) - RE_Kenshi and
  KenshiLib, which make plugins like this possible
- [lsalzman/enet](https://github.com/lsalzman/enet) - UDP networking library
- [zeroit789](https://github.com/zeroit789) - the "Multiplayer (Wanderer x2)"
  co-op game start ([#15](https://github.com/nhoral/KenshiCoop/pull/15))
- Lo-Fi Games - Kenshi

## License

[AGPL-3.0](LICENSE). KenshiLib and RE_Kenshi are GPLv3; this plugin links
KenshiLib under GPLv3 section 13 (GPL/AGPL combination). Not affiliated with
Lo-Fi Games. Non-commercial fan project.
