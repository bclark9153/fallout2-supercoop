# Fallout 2 SuperCoop

**Fallout 2 co-op with the Restoration Project.** One PC runs a dedicated server that owns the world; every player joins it with their own character, and the whole party plays Killap's Restoration Project together: the restored towns, quests, NPCs and the sfall-powered systems that come with it.

[![Two players standing over Frank Horrigan's body](docs/shots/horrigan.jpg)](docs/shots/horrigan.jpg)

SuperCoop is built on [fallout2-coop](https://github.com/gusmarcum/fallout2-coop) (the co-op server, client and wire protocol), which is built on [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce) (the engine). This fork adds what the Restoration Project needs from sfall, and makes RP's scripts work with several players in one world.

## Install

You need Fallout 2 from **Steam or GOG** (US 1.02d), Windows 10/11 64-bit, and about 2 GB free.

1. Download the latest `Fallout2-SuperCoop-<version>-windows.zip` from [Releases](../../releases) and extract it anywhere.
2. Double-click **`install.cmd`**. It finds your Fallout 2, builds a separate co-op folder next to it, downloads the Restoration Project 2.4.34 from [its official release](https://github.com/BGforgeNet/Fallout2_Restoration_Project/releases/tag/v2.4.34) and checks it, picks a screen mode for your monitor, and makes two desktop shortcuts. Your original game is only read, never changed.
3. **Host:** run *Fallout 2 SuperCoop - Host*, press Enter for a new game, then run *Fallout 2 SuperCoop - Join* with the address left empty.
   **Join:** run *Fallout 2 SuperCoop - Join* and enter the host's address.

Play over a VPN such as ZeroTier or Tailscale. Never forward the game port (9300) to the internet: it has no password. Everyone in a game needs the same SuperCoop version. Already have the RP zip? Put `rpu_v2.4.34.zip` next to `install.cmd` and the installer uses it instead of downloading.

Updating: extract the new release and run `install.cmd` again on the same co-op folder. It replaces the program files and keeps your saves.

## What SuperCoop adds

**Restoration Project support**
- sfall data loading: `mods\mods_order.txt` with sfall's priority order, more world-map areas than vanilla, `IniConfigFolder`.
- The sfall functions RP and its bundled mods (Party Orders, NPC Armor, sfall's own scripts) call: critter base/extra stats, interface tags, fake perks, `fs_*`, `sprintf`, ini sections, extra message files, path finding, burst control and more.
- sfall hook scripts, with sfall's ordering and recursion rules: combat damage, item use, equipping, AI weapon checks, pickups, game-mode changes, global-variable writes and key presses.
- Global scripts start on a new world and on a load, and their timers run on the server, in the world loop and during world-map travel.

**Made for several players**
- A hook runs *as the player it concerns*: whoever used the item, equipped the gear, attacked or pressed the key. RP's alcoholism, for example, happens to the player who drank.
- Global-script timers are per player where the script is about "the player" (crippling, the torn password papers), and otherwise follow the player who last triggered them.
- RP's interface tags (ADDICT, CRIPPLED, NPC CRIPPLED) show on the right player's interface bar.
- Clients relay their keys, so Party Orders hotkeys and Ctrl-combos work for each player.

**Robustness**
- A script error from mod data stops that script instead of the server (out-of-range script stacks, critters placed above their max HP).
- The Linux/Docker server finds global scripts inside DAT files.

## Known limits

- The Restoration Project stores some states once for the world (alcoholism, Tragic card addiction), so one player at a time can have them.
- Not drawn yet: fake perks on the character screen, `set_iface_tag_text` labels. The Hero Appearance machine at the EPA changes nothing; sfall's party control (taking over a companion) is refused in co-op.
- Party Orders' default T (pick up) is chat in co-op; rebind it in `mods\party_orders.ini`.
- Everything in [the upstream known limits](docs/UPSTREAM_README.md#known-limits) still applies (one map at a time, no authentication, VPN recommended).

## Building

Same as upstream: see [docs/UPSTREAM_README.md](docs/UPSTREAM_README.md#building-on-windows) (MSYS2 on Windows) or [DEDICATED_HOWTO.md](DEDICATED_HOWTO.md) (Linux, Docker). `scripts/package-windows.sh <dir with the two exes>` builds the release zip; pushing a `v*` tag builds and publishes it from CI.

## Credits

- [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce) by Alexander Batalov and contributors: the engine.
- [fallout2-coop](https://github.com/gusmarcum/fallout2-coop) by gusmarcum, derived from [Cahb/fallout2-ce-coop](https://github.com/Cahb/fallout2-ce-coop): the co-op server, client and protocol. See [AUTHORS.md](AUTHORS.md).
- [Fallout 2 Restoration Project](https://github.com/BGforgeNet/Fallout2_Restoration_Project) by Killap, maintained by BGforge: downloaded by the installer, not included here.
- [sfall](https://github.com/sfall-team/sfall): the extension whose behaviour SuperCoop reimplements.
- Fallout 2 is the work of Black Isle Studios and Interplay. You need your own copy.

Licence: [Sustainable Use License](LICENSE.md) (non-commercial), inherited from Fallout 2 Community Edition.
