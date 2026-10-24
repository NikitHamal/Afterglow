# HUD / window notes (branch `agent-models-hud`, track A)

The user's report (2026-10-24): "the HUD is totally broken, the buttons and all are flawed, the window is flawed, every
push breaks it". This file is the state of that fix: what was wrong, what is in place, how to check it.

## What was wrong

1. **The window model (the real "everything is broken").** The game opened its window with `FLAG_WINDOW_HIGHDPI` on
   every platform. raylib 5.5 (as fetched by CMake) supports that flag on Windows/Linux only until the first resize:
   the window opens in logical units with a 2D scale matrix (`screenScale`) and a scaled mouse, then
   `WindowSizeCallback` sets the screen size to *physical* pixels while the scale matrix and the mouse scale stay at
   the display scale. Any resize — main.cpp fitting the window to a laptop screen at start, maximise, borderless,
   F11, dragging the border — left the UI drawn 1.25-2x too big, the SDF boxes (computed in framebuffer pixels)
   detached from their quads and text, the action bar off screen, and clicks beside the buttons. The QA harness
   opened its window with flags 0, so CI shots never showed it: every push "looked fine" and was broken on a
   Windows laptop with display scaling. Reproduced: `--legacy-dpi --res 1280x720 --resize 1920x1080` under
   `Xft.dpi: 144` (board: before_after/hud_dpi_*).
2. **Layouts without fit guarantees.** UI scale was sub-linear (`pow(min(W/1920,H/1080), 0.8)`), so below 1080p
   the design canvas shrank below 1920x1080 and fixed layouts ran out of room (960x540: Customize's hair row on
   top of Reset/Done). Fixed panel heights (Load: 7 slots in 720 units overlapped Close), labels that never shrank
   or ellipsised, a segmented control that could exceed its row, an acts picker that could exceed the screen width.
3. Small bugs: `keyHint` measured its label with Inter Regular but drew Inter Medium (hints overlapped by a few
   px); the chapter card kept drawing under the story menu / ending backdrop (ghost text); frosted glass let lamps
   punch through panels as white smudges; story log rows used an estimated height (long lines could overlap).

## What is in place

- `app/window.*` — one coordinate space: no `FLAG_WINDOW_HIGHDPI` on Windows/Linux (screen = framebuffer = mouse
  pixels; GLFW is per-monitor DPI aware so it stays crisp), Retina kept on macOS. `window::beginFrame()` after
  `BeginDrawing()` drops any DPI matrix / mouse scale on non-Apple platforms. Window placement is monitor-aware
  (secondary monitors, title bar on screen, fits the work area, a default-sized window grows on a big HiDPI
  monitor), mode switches (windowed / borderless / fullscreen, F11, Alt+Enter) go through `window::applyDisplay`,
  the remembered size ignores maximised / borderless / fullscreen states.
- `ui/ui.*` — linear scale (`s = min(W/1920, H/1080)`): a layout that fits the 1920x1080 design fits every window.
  Scale scopes (`pushScale` / `ui::ScaleScope`), `fitScale(natW, natH)` for fixed panels, measured fits
  (`fitGet` / `fitReport`) for panels whose content varies (Customize, Settings tabs). Text that fits
  (`textIn` / `fitPx` / `ellipsize`): shrink a little, then ellipsis. Widgets measure themselves (buttons, chips,
  tabs, segmented choices, sliders, toggles, swatches, menu items, key hints).
- `ui/layout.*` — `ScaleScope`, `ClipScope` (a container for the guard), `cell`, `flow` / `chipFlow`.
- `game/game_hud.cpp` (split out of game_play.cpp) — meters, name, subtitles (wrap upwards), hints, toasts, the
  action bar (measured; shrinks as a whole if the window is narrower), the acts picker (cards wrap onto more rows
  when a group has more acts than fit; shrinks if too tall), the intro card (measured, centred hint row).
- Customize uses a column + `chipRow()`: adding a row of choices is one call (track D's "Model" row:
  `col.section("MODEL"); int m = chipRow(U, col, "model#", names, active);`).
- Story UI: chapter card / ending titles fit, top bar title gives way to the controls, choices wrap to two lines
  and the list fits between the letterbox bars, save/load and gallery rows share the room above Close, real log
  heights, story cards no taller than they need on 5:4 screens.
- **QA guard** (`ui_fit.cpp`, on in every harness run): every text and widget is checked against the screen and its
  container (`UI-OVERFLOW ...`) and texts against each other (`UI-OVERLAP ...`); the harness logs the lines and
  `ui-issues N` per shot plus a total.

## How to check (molab)

```bash
# UI matrix: every screen (actors=0) at 10 sizes + per-screen contact sheets; guard lines per size
bash /root/uimatrix.sh /root/build_hud /root/qa/ui_shots.txt /root/qa/uim_after after   # (tools: task/tools/uimatrix.sh)
grep -h "UI-OVERFLOW\|UI-OVERLAP" /root/qa/uim_after/*.log | sort | uniq -c
# display scaling: legacy model (broken after a resize) vs the new one
xvfb-run -a -s "-screen 0 3840x2160x24 -noreset" bash -c "echo Xft.dpi: 144 | xrdb -nocpp -merge; \
  ./afterglow --shotlist list.txt --outdir out --res 1280x720 --resize 1920x1080 --hidpi"      # or --legacy-dpi
```
Harness flags: `--hidpi` (the game's own window flags), `--legacy-dpi` (old model), `--resize WxH` (after open).
Spec key `actors=0` hides both characters (deterministic, cheap, non-explicit UI shots); list: `tools/qa/ui_shots.txt`.
Xvfb needs `-noreset` or the X resources set by xrdb vanish before the game connects.

## Next

- An "Interface size" option (85-130 %) on the Display tab: the fit machinery already guarantees it cannot overflow.
- Run the guard on CI (a small UI-only shot list at 3 sizes is ~1 min under llvmpipe) once a CI QA job is back.
