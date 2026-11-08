# Block World Dedicated Server

This is a standalone TCP server for the Block World game. It runs independently of the OpenGL client, accepts up to 16 simultaneous connections, tracks player positions, relays chat and player updates, validates block edits, and persists the shared world in `world.dat`.

## Build and run (Linux)

Install GCC (for example, `sudo apt install build-essential` on Debian/Ubuntu), then from this directory run:

```sh
chmod +x build_server.sh run_server.sh
./build_server.sh
./run_server.sh
```

By default the server listens on TCP port `25565`. Pass a port as the first argument to use another port:

```sh
./run_server.sh 25566
```

The server stores `world.dat` in this directory. Allow the chosen TCP port through your Linux firewall. The server binds to all network interfaces; press Ctrl+C to stop it.

## Build and run (Windows)

Install MSYS2 with GCC, then run:

```bat
build_server.bat
run_server.bat
```

By default the server listens on TCP port `25565`. The game client joins the host and port configured in the project-root `user.json` (default `127.0.0.1:25565`) using **Join Server** on the home screen. Pass another port as the server's first argument and set the same port in `user.json`:

```bat
run_server.bat 25566
```

Allow the chosen TCP port through Windows Firewall. The server binds to all network interfaces. Press Ctrl+C in the server console to stop it. On either platform the world is saved on block changes and at shutdown.

## Text protocol

Messages are UTF-8/ASCII-compatible lines terminated by `\n`. A client must send `HELLO <name>` first. Names are 1-24 printable ASCII characters.

### Client messages

| Message | Meaning |
|---|---|
| `HELLO <name>` | Join; sent once after connecting |
| `POS <x> <y> <z> <yaw> <pitch>` | Update the player's position and view |
| `CHAT <text>` | Send a chat line |
| `PLACE <x> <y> <z> <block>` | Place block ID 1-7 |
| `BREAK <x> <y> <z>` | Break a block |
| `PING` | Request a liveness response |

### Server messages

| Message | Meaning |
|---|---|
| `WELCOME <id> <width> <height> <depth> <max_players>` | Accepted connection, server-assigned player ID, and world dimensions |
| `WORLD_BEGIN <count>` | Start of the non-air block snapshot |
| `BLOCK <x> <y> <z> <block>` | World block record or block-change broadcast; ID 0 means air |
| `WORLD_END` | End of initial snapshot |
| `PLAYER <id> <name> <x> <y> <z> <yaw> <pitch>` | Existing player or position update |
| `JOIN <id> <name> <x> <y> <z>` | Another player joined |
| `LEAVE <id>` | Player disconnected |
| `CHAT <id> <name> <text>` | Chat broadcast |
| `PONG` | Response to `PING` |
| `ERROR <reason>` | Rejected request |
| `EDIT_OK BREAK <x> <y> <z> <block>` | Accepted block break, including the block ID for inventory |
| `EDIT_OK PLACE <x> <y> <z> <block>` | Accepted block placement |

Block IDs match the game: air 0, grass 1, dirt 2, stone 3, sand 4, wood 5, leaves 6, glass 7.

The OpenGL client connects using the TCP protocol above. It loads the shared world snapshot, synchronizes player positions and block edits, displays player IDs/names and relayed chat, and disconnects when returning to the home screen.
