**Fallout 2 co-op with the Restoration Project 2.4.34.** Built on fallout2-coop v1.4.2.

### Install
1. You need Fallout 2 from **Steam or GOG** (US 1.02d) and Windows 10/11 64-bit.
2. Download `Fallout2-SuperCoop-*-windows.zip` below and extract it anywhere.
3. Double-click **`install.cmd`**. It finds your Fallout 2, builds a separate co-op folder, downloads the Restoration Project (827 MB, hash-checked) and makes *Host* and *Join* desktop shortcuts. Your original game is not changed.

**Host:** run *Fallout 2 SuperCoop - Host*, press Enter for a new game, then *Fallout 2 SuperCoop - Join* with the address empty.
**Join:** run *Fallout 2 SuperCoop - Join* and enter the host's address. Use ZeroTier or Tailscale; never forward port 9300.

Everyone in a game needs this same release. To update later, run the new release's `install.cmd` on the same folder; saves are kept.

### What's in it
- The Restoration Project running on the co-op server: sfall mod loading, the sfall functions and hook scripts RP and its bundled mods use, and RP's global scripts (with their timers) on the server.
- RP made multiplayer-aware: scripts act on the player involved, per-player interface tags (ADDICT, CRIPPLED, NPC CRIPPLED), Party Orders hotkeys from every player.
- Fixes for RP data that stopped the server (a script stack overrun, a critter above max HP).

Known limits and full details: [README](https://github.com/bclark9153/fallout2-supercoop#known-limits) and [CHANGELOG](https://github.com/bclark9153/fallout2-supercoop/blob/main/CHANGELOG.md).
