# Resume: Afterglow 2D realism revamp (handoff from failed session `session_1789406904820_ng4p5`)

> Paste this as the first message of a NEW session (provider `cline`, model
> `cline-free/deepseek-v4.1-flash`, mode Act, cwd `F:/Afterglow`).
> The old session died with Hub 1006 on continue because its 10.9MB transcript
> chokes the Hub — do NOT resume it, use this compact state instead.

## Objective
- Make side-view (`js/side.js`) and FPV (`js/fpv.js`) 2D web art dramatically more realistic/amazing.
- C++/Android/workflows/sim-HTML work is CANCELLED and removed.

## Repo facts
- Branch `hoplite/naukratis-0f7262b7`, remote `https://github.com/NikitHamal/Afterglow.git`, PR #1.
- 2D realism rules: single `herT()` ramp, warm lamp upper-left + cool moon right,
  `FPV_HI=255,240,222`, keep perf budget. User reviews `.shots/*.png`.
- Harness: `scratch/serve.py` port 5500, Chrome at
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, `puppeteer-core`, `.shots/`.
- Perf baseline: side avg 28.9/p50 7.5/p95 106.1/max 670ms; FPV avg ~7.8/p50 3.8/p95 ~6.8-7.7/max ~226ms; `shade()` ~222 calls/frame.
- `scratch/cam-bone-*.js` belongs to a parallel agent — leave untouched.

## Already done (uncommitted)
- Deleted: `native/`, `android/`, `.github/workflows/build-windows.yml`,
  `build-android.yml`, `.shot/shot.html`, `.shots/fpv-before/after-*.png`.
- `js/app.js`: headless helper `?play=1&char=goatchan&view=fpv|side&pleasure=&ar=&depth=&solo=` sets `G.state='play'`, hides `#intro`.
- `js/skin.js` polish (`node --check` clean): softer `limbS` crest + core shadow,
  `fSSS` rgba(255,135,105,0.32), `handS` dual highlights + nails/lunula,
  `breastS`/`areola/nipple` gradients + Montgomery dots.
- `js/fpv.js` realism (`node --check` clean): neck slim/flare/AO/SCM, face radial
  + zygomatic/masseter shading, detailed iris (striations, limbal ring, catchlights),
  torso waist/rib/flank + linea alba, softened breast domes.
- `js/side.js` realism (`node --check` clean): torso light/AO/waist/iliac/linea alba,
  face radial + cheek, detailed iris.
- Verified: `node --check` on skin/fpv/side/gfx/app all OK.

## Blocked
- Headless screenshots come out BLACK (`.shots/fpv-after2.png`, `side-after2.png`):
  `domcontentloaded` + 3000ms succeeds but canvas not painting; `--screenshot` +
  `networkidle0` times out; `virtual-time-budget` also dark. Intro bypass works.

## Next move
1. Fix headless paint capture (rAF/virtual-time, WebGL/fonts, `page.on('console'/'pageerror')`),
   retake `fpv-after2.png` / `side-after2.png` for user review.
2. Then continue side/her-torso/hip/him/vulva + FPV hands/fluids realism; commit + push.

## Key files
- `F:\Afterglow\js\fpv.js`, `js\side.js`, `js\skin.js`, `js\app.js`, `js\gfx.js`,
  `js\core.js`, `js\chars.js`, `index.html`
- `F:\Afterglow\scratch\serve.py`, `scratch\verify2d.mjs`
- `.shots/fpv-after2.png`, `.shots/side-after2.png` (current black captures)
