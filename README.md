# OpenGL Minecraft-like Demo

This project is a small Minecraft-inspired C game built with OpenGL and GLFW.
The custom BLOCK WORLD home screen provides Play, Join Server, Online Settings, and Exit. Movement uses frame-rate-independent smoothing.

## Build

From a Windows terminal:

```powershell
& 'C:\msys64\usr\bin\gcc.exe' main.c -O2 -std=c11 -I 'C:\msys64\clang64\include' -L 'C:\msys64\clang64\lib' -lglfw3 -lopengl32 -lglu32 -lgdi32 -limm32 -lcomdlg32 -lws2_32 -o minecraft.exe
```

Then run either:

```powershell
$env:Path = 'C:\msys64\clang64\bin;C:\msys64\usr\bin;' + $env:Path
.\minecraft.exe
```

or from PowerShell:

```powershell
.\run_game.ps1
```

or double-click `run_game.bat`.

## Build and run (Linux)

On Debian/Ubuntu, install the development dependencies and optional skin/font support:

```sh
sudo apt install build-essential pkg-config libglfw3-dev libfreetype-dev libgl1-mesa-dev libglu1-mesa-dev zenity fonts-noto-cjk
chmod +x build_game.sh run_game.sh
./build_game.sh
./run_game.sh
```

The client needs a desktop session with OpenGL support. `zenity` provides the skin-file chooser; Noto Sans CJK provides Japanese chat glyphs. Without those optional packages, skin selection or Japanese glyph rendering may be unavailable. Save files and assets are read relative to the project directory.

The Linux client can connect to either the Linux or Windows dedicated server using the same address and port in **Online Settings**.

If `user.json` is missing, invalid, or lacks required fields, the client fills missing values and rewrites the complete configuration. A regenerated username uses the format `Player####` (four digits); valid existing username and server settings are preserved when only other fields are missing. If the local world save is missing, truncated, or invalid, a fresh world and starting inventory are generated and saved to `world_saves/world.dat`. The dedicated server likewise saves a regenerated world on startup.

## Controls

- W / A / S / D: move around
- Mouse: look around
- First-person view by default; K cycles first-person, third-person rear, and third-person front
- Space: jump
- Shift while airborne: descend faster
- Creative: double-tap Space to toggle flight; flying still collides with blocks
- Creative flight: Space ascends, Shift descends
- Ctrl: sprint
- E: open / close inventory
- Move with W / A / S / D while inventory is open
- Q: drop one item from the selected hotbar slot; walk near dropped items to pick them up
- 1-9: select a hotbar slot
- Mouse wheel: switch hotbar slot
- Click a slot in the open inventory to select it
- Left click: break block
- Block breaking is limited to 3 blocks per second
- Right click: place block
- The first-person arm swings when a block is successfully placed or broken
- Block placement/breaking: limited to 8 actions per second
- HUD: hotbar with item icons and counts; crosshair stays centered while playing
- The selected hotbar item's name is shown above the hotbar
- First-person: white arm is rendered at the lower right
- Esc: open / close pause menu
- Pause menu buttons: Resume, Change Skin, or Home
- Home menu: Join Server connects to the server configured in `user.json`
- Online Settings: edit username, server address, port, and player skin; click a field or use Tab to select it, type ASCII text, use Backspace to erase, then Save (Enter saves; Esc cancels)
- T: open chat; `/` opens chat with a command prefix; Enter sends, Esc closes
- Chat input: Up / Down navigates the previous 32 sent messages and commands; they are saved to `history.json` in the game's working directory
- Chat accepts lowercase text and Windows Japanese IME input; confirmed IME text is appended to the chat field, and Enter sends only when no composition is active

The world is 64 x 64 blocks across and uses `textures/kusa.png`, `textures/dirt.png`, and `textures/stone.png` for grass, dirt, and stone blocks.
Walking backward is 70% of the normal movement speed.

## Player model and skin

The third-person player model is loaded from `model.gltf` (glTF 2.0), oriented to face the direction the player moves, with swinging arms and legs, a walking body bob, and a raised-arm airborne pose. Remote players animate from their synchronized movement. K cycles between rear and front third-person views as well as first person. Choose **Online Settings** on the home screen to edit the username and server address/port, or select a PNG skin; it is copied to `skin/skin.png`. Save stores the settings in `user.json`. The pause menu also provides **Change Skin**. The loader supports triangle meshes with `POSITION` and `TEXCOORD_0` attributes, indexed or non-indexed geometry, and node translation/rotation/scale.

## World saves

The current world is loaded from and saved to `world_saves/world.dat`. The game saves automatically when returning to the home screen or closing the game. Use `/save` in chat to save immediately. Blocks, player position and view, game mode, inventory, and selected hotbar slot are saved.

## Multiplayer

Start the dedicated server from [`server/README.md`](./server/README.md), then choose **Join Server** on the home screen. Set the username, server address, and port in **Online Settings** first. The server assigns a player ID on each connection; the ID and username are shown in-game and chat. Players, chat, shared blocks, and positions are synchronized. Returning Home disconnects and restores the local single-player save.

## Chat commands

- `/help`: show available commands
- `/gamemode survival` or `/gamemode creative` (also accepts `0` or `1`)
- `/give <block> [count]`: add grass, dirt, stone, sand, wood, leaf/leaves, or glass
- `/clear`: empty the inventory
- `/save`: save the world immediately

Creative mode allows flying with Space/Shift, does not consume blocks when placing them, and does not add broken blocks to the inventory. Normal chat and command responses appear in the chat overlay.

## Notes

This is a lightweight Minecraft-style demo focused on first-person movement, terrain generation, block placement/removal, and mouse-look controls.
The multiplayer client and dedicated server use the line protocol documented in [`server/README.md`](./server/README.md).
