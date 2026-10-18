# Dedicated server with Docker Compose

The final `fo2-dedicated-server` image contains only a statically linked server
binary. It does not contain Fallout 2 assets, a shell, SDL, or an operating-system
userspace. The licensed game installation is bind-mounted at runtime.

## Start

```sh
cp .env.example .env
```

Edit `.env` and set `F2_GAME_PATH` to the absolute path of a copy of your
installed **Fallout 2 SuperCoop** folder (the one `install.cmd` built on Windows:
it holds `master.dat`, `critter.dat`, `mods`, `data` and the Restoration Project).
Every player must have the same data, so copy the folder rather than assembling
one by hand. On Linux, also set `F2_UID` and `F2_GID` to the output of `id -u` and
`id -g` so the server can write its saves.

```sh
docker compose pull            # the prebuilt image from ghcr.io
docker compose up -d
docker compose logs -f fo2-dedicated-server
```

`docker compose up --build -d` builds the image from this checkout instead.

Players connect to the Docker host on TCP port 9300, the port the Windows *Join*
launcher assumes. The plain-text admin channel is TCP 9301 and is bound to
`127.0.0.1` by default because it has no authorization layer:

```sh
printf 'status\n' | nc -q1 127.0.0.1 9301
```

Set `F2_SERVER_CMD_BIND=0.0.0.0` only when a firewall or private network protects
the port.

### Client launch commands

Linux (run from the Fallout 2 installation directory):

```sh
env F2_CLIENT_CONNECT=SERVER_IP:9300 \
    F2_PLAYER_NAME=Player F2_PLAYER_CREATE=ask F2_WINDOWED=1 \
    /path/to/fallout2-ce
```

Windows: run *Fallout 2 SuperCoop - Join* and enter `SERVER_IP`. By hand, in
PowerShell from the SuperCoop folder:

```powershell
Set-Location 'D:\Games\Fallout 2 SuperCoop'
$env:F2_CLIENT_CONNECT = 'SERVER_IP:9300'
$env:F2_PLAYER_NAME = 'Player'
$env:F2_PLAYER_CREATE = 'ask'
$env:F2_WINDOWED = '1'
.\fallout2-ce.exe
```

`F2_PLAYER_CREATE=ask` is only acted on the first time the server sees that
player name, so the same command can be reused for later connections.

## Persistence

The game directory is mounted read-write at `/game`, which is also the process
working directory required by the engine. Server saves and transient map state
therefore survive image replacement and container recreation in the host's
`data/SAVEGAME` and `data/MAPS` directories. The image itself remains immutable.

The default autosave interval is five minutes. Before a planned stop, request a
save over the admin channel and wait for the success reply, then stop the service:

```sh
printf 'save 1 before container stop\n' | nc -q1 127.0.0.1 9301
docker compose down
```

`F2_SERVER_LOAD=newest` (the image default) continues the most recently written
save on every start, and starts a new game on `F2_SERVER_MAP` (default
`artemple.map`) when the folder has no saves yet, so restarts and image updates
pick up where the party left off. Set `F2_SERVER_LOAD=<slot>` to load one slot
instead. To use the lobby, set `F2_SERVER_LOAD=` (empty) and select a world
through the admin channel with `saves`, `load <slot>`, or `new <map.map>`.

Compose defaults both authoritative difficulty settings to their hardest values:
`F2_GAME_DIFFICULTY=2` (Hard) and `F2_COMBAT_DIFFICULTY=2` (Rough). Their full
range is `0` Easy/Wimpy, `1` Normal, `2` Hard/Rough. These overrides are reapplied
after loading a save; client Preferences do not change server difficulty.

## Image

Every release publishes `ghcr.io/bclark9153/fallout2-supercoop-server` tagged
`latest` and with the release version (for example `0.1.2`). Pin `F2_IMAGE_TAG`
in `.env` to the version your players' Windows release has. To build and publish
your own:

```sh
docker compose build fo2-dedicated-server
docker image save fo2-dedicated-server:latest | gzip > fo2-dedicated-server.tar.gz
docker tag fo2-dedicated-server:latest USER/fo2-dedicated-server:latest
docker push USER/fo2-dedicated-server:latest
```

Only the small final stage is tagged or exported; the Alpine compiler stage remains
in the local build cache.
