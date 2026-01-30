# ESP32 Lock Picking Game Board

This project runs a lock-picking style game on an ESP32 with three cylinder sensors, a button, three LEDs, and a 20x4 I2C LCD. It provides a local web page to start a game and capture a player ID, times the run, and posts results to a Google Apps Script endpoint.

## What it does

- Calibrates three cylinder sensors at startup to determine their center ("home") position.
- Runs a start-light sequence (Formula 1 style) on the LEDs with LCD animation.
- Starts a stopwatch when the lights go out.
- Records the first time each cylinder moves out of its center band (one trigger per run).
- Ends the run when all cylinders trigger or a time limit is reached.
- Sends results to a Google Apps Script web app (Google Sheet).
- Displays status and feedback on the LCD.
- Supports a long-press reset (10 seconds) to re-initialize the game state.

## Hardware used

- ESP32
- 20x4 I2C LCD (address `0x27`)
- 3x cylinder sensors (analog input, e.g., potentiometers)
- 3x LEDs
- 1x button (momentary, uses internal pull-up)

## Main behaviors

- **Start**: Press the button or use the web UI `/` page to start.
- **Start lights**: LEDs light in sequence, then turn off to signal "go".
- **Timing**: The timer shows on LCD line 1. Cylinder lap times show on lines 2-4.
- **Finish**: Stops when all cylinders trigger or time limit expires.
- **Reset**: Hold the button for 10 seconds to reset the game.

## Web control

- The ESP32 hosts a page at `http://<device-ip>/` where a player ID can be entered.
- Pressing "Start" sends a `/command?command=start&playerID=...` request.

## Data logging

Results are POSTed as JSON to the Apps Script URL in `sendDataToDatabase()`:

- `totalTime`
- `cylinder1`, `cylinder2`, `cylinder3`
- `boardID`
- `gameID`
- `playerID`

## Files

- `lockpickking_game_code` — main firmware source
- `WIRING.md` — wiring reference

## Notes

- Button uses `INPUT_PULLUP` (pressed = LOW).
- Cylinder center threshold is configurable via `THRESHOLD`.
- The LCD uses a fixed 20x4 layout.
