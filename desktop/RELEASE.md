SYN-ACK WIRE for Windows, macOS and Linux: the cyberpunk network game for the Atari, Apple II, MS-DOS and CoCo, for
players without a vintage machine. It plays at the same tables, so you can join friends on their 8-bit machines.

## Install

Download the file for your computer from **Assets** below. Nothing else needs installing.

**Windows** (`syn-ack-wire-windows.zip`): unzip it, open the `syn-ack-wire` folder and run `syn-ack-wire.exe`. The
game isn't signed, so the first time Windows may say "Windows protected your PC": click **More info**, then
**Run anyway**. Keep the `data` folder next to the program.

**macOS** (`syn-ack-wire-macos.zip`, Apple silicon and Intel, macOS 11 or newer): unzip it and move
`syn-ack-wire.app` to Applications if you like. The game isn't signed by Apple, so the first time:
- macOS 15 (Sequoia) or newer: open it, choose **Done** at the warning, then go to **System Settings, Privacy &
  Security**, scroll down and click **Open Anyway**.
- Older macOS: right-click (or Control-click) the app, choose **Open**, then **Open** again.

**Linux** (`syn-ack-wire-linux.tar.gz`, x86-64): `tar xzf syn-ack-wire-linux.tar.gz` and run
`syn-ack-wire/syn-ack-wire`. It needs OpenGL 3.3.

## Play

Type a handle and press **Enter** to jack in. The first player at a table is the host and starts the game with
**2**-**6** seats; bots take the empty ones. **F1** (or **?**) shows the instructions at any time; **Help, Keys**
lists every key.

- **Region**: the four servers, each with the seats free there (**Refresh Seats** counts again), and **Custom
  Server...** for a server of your own, on your network or anywhere. To run one, see
  [server/README.md](https://github.com/michaelsternberg/syn-ack-wire/blob/main/server/README.md) (Python 3.11,
  nothing else to install).
- **Options**: colour themes, fonts, CRT effects, full screen (**F11**), an auto-hiding menu bar, move hints
  (**F2**) and sound (**F3**). Your choices and handle are remembered.

If you quit in the middle of a game, a bot plays your seat until you come back: start the game again and press
**Enter**.
