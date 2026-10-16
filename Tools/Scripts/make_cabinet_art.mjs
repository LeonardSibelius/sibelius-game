// make_cabinet_art.mjs - the painted parts of the Celestial Fortune cabinet.
//
// Walt, 2026-10-16: "ok, build the custom cabinet in blender" - the cathedral's machine, made
// for the game rather than bought (a store cabinet would carry someone else's theme, and often
// a real manufacturer's look). The cabinet's shape is build_cabinet_blender.py; this paints the
// three panels it carries, to docs/MARQUEE_SPEC.md:
//
//   T_Marquee_CelestialFortune  2048 x 1024  the lit top box: "Celestial" in a heavy casino
//                               script (Script MT Bold) rising 8 degrees, gold over a hard red
//                               shadow, its swash underlining the word; "FORTUNE" in Roman
//                               inscriptional capitals (Felix Titling), tracked wide; 24 thin
//                               rays behind; the star and Saturn flanking, small, gold-tinted.
//   T_CabScreen                 2048 x 2048  the attract screen: the reels at rest in the web
//                               game's own symbols. Painted at the screen's 4:3 and stretched
//                               to a square; the screen's UVs un-stretch it.
//   T_CabBadge                  1024 x 256   the maker's plate, low on the front, where a real
//                               cabinet's manufacturer goes: Walt's name, engraved brass.
//
// Fonts are Windows' own, drawn into pictures here (no font ships with the game). No real
// manufacturer's mark anywhere (the spec's rule). Symbols are SourceArt/SlotSymbols - the web
// game's original SVG.
//
//   set PLAYWRIGHT_BROWSERS_PATH=C:\Users\wpark\projects\celestial-fortune-web\.playwright-browsers
//   node Tools/Scripts/make_cabinet_art.mjs

import { mkdirSync, readFileSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..', '..');
const out = join(root, 'SourceArt', 'SlotCabinet');
const symbols = join(root, 'SourceArt', 'SlotSymbols');
mkdirSync(out, { recursive: true });
const web = 'C:/Users/wpark/projects/celestial-fortune-web';
const { chromium } = await import(pathToFileURL(join(web, 'node_modules', 'playwright', 'index.mjs')).href);

// Embedded as data: URLs - a page set with setContent cannot load file: fonts.
const font = (name, file) =>
  `@font-face { font-family: '${name}'; src: url(data:font/ttf;base64,${readFileSync(`C:/Windows/Fonts/${file}`).toString('base64')}); }`;
const FONTS = font('CasinoScript', 'SCRIPTBL.TTF') + font('RomanCaps', 'FELIXTI.TTF');
const svg = (name) => `data:image/svg+xml;base64,${readFileSync(join(symbols, `${name}.svg`)).toString('base64')}`;

// The spec's palette, matched to SlotScreenWidget.cpp.
const FIELD = '#0A0A1A', GOLD = '#FFD24D', PALE = '#FFEB9E', BRASS = '#D4A836', HEAT = '#D92E10', COOL = '#C7EEFF';

function rays(cx, cy, n, inner, outer, w) {
  let s = '';
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2;
    // Longest at the horizontal, shortest straight up and down.
    const len = outer * (0.55 + 0.45 * Math.abs(Math.cos(a)));
    s += `<line x1="${cx + Math.cos(a) * inner}" y1="${cy + Math.sin(a) * inner}" ` +
         `x2="${cx + Math.cos(a) * len}" y2="${cy + Math.sin(a) * len}" stroke="url(#ray)" stroke-width="${w}"/>`;
  }
  return s;
}

const MARQUEE = `
<style>${FONTS}
  body { margin: 0; width: 2048px; height: 1024px; overflow: hidden; background: ${FIELD}; }
  .field { position: absolute; inset: 0;
    background: radial-gradient(ellipse 62% 70% at 50% 44%, #1c1a3a 0%, #121130 38%, ${FIELD} 78%); }
  svg.rays { position: absolute; inset: 0; }
  .script { position: absolute; left: 0; right: 0; top: 70px; text-align: center;
    font-family: 'CasinoScript'; font-size: 440px; line-height: 1; color: ${GOLD};
    transform: rotate(-8deg); text-shadow: 14px 15px 0 ${HEAT}; letter-spacing: 2px; }
  .caps { position: absolute; left: 0; right: 0; top: 742px; text-align: center;
    font-family: 'RomanCaps'; font-size: 150px; letter-spacing: 0.25em; color: ${PALE};
    padding-left: 0.25em; text-shadow: 0 0 18px rgba(255, 210, 77, 0.35); }
  .flank { position: absolute; top: 74px; width: 120px; height: 120px; opacity: 0.85;
    filter: sepia(1) saturate(2.2) hue-rotate(-12deg) brightness(1.05); }
</style>
<div class="field"></div>
<svg class="rays" width="2048" height="1024" viewBox="0 0 2048 1024">
  <defs><linearGradient id="ray" gradientUnits="userSpaceOnUse" x1="1024" y1="430" x2="2048" y2="430">
    <stop offset="0" stop-color="${PALE}" stop-opacity="0.32"/><stop offset="1" stop-color="${COOL}" stop-opacity="0"/>
  </linearGradient></defs>
  <g style="mix-blend-mode: screen">${rays(1024, 430, 24, 260, 980, 3)}</g>
  <!-- the script's terminal swash, sweeping back under the word -->
  <path d="M 1640 470 C 1500 640, 980 690, 430 640 C 360 634, 330 625, 318 612"
        fill="none" stroke="${HEAT}" stroke-width="30" stroke-linecap="round" transform="translate(14 15)"/>
  <path d="M 1640 470 C 1500 640, 980 690, 430 640 C 360 634, 330 625, 318 612"
        fill="none" stroke="${GOLD}" stroke-width="30" stroke-linecap="round"/>
</svg>
<div class="script">Celestial</div>
<div class="caps">FORTUNE</div>
<img class="flank" style="left: 96px" src="${svg('star')}">
<img class="flank" style="right: 96px" src="${svg('saturn')}">
`;

// The screen, at rest: five reels of three in the web game's symbols, a title band and a meter
// row, framed in brass like USlotScreenWidget's own frame. Painted at 4:3 (2048 x 1536).
const REELS = [
  ['moon', 'seven', 'saturn'], ['star', 'galaxy', 'mars'], ['seven', 'wild', 'seven'],
  ['mars', 'saturn', 'scatter'], ['galaxy', 'star', 'moon'],
];
const SPRITE = { seven: 'sun', crown: 'rocket', scatter: 'earth' };
const cells = REELS.map((reel) => `<div class="reel">${reel.map((s) =>
  `<div class="cell"><img src="${svg(SPRITE[s] ?? s)}"></div>`).join('')}</div>`).join('');
const SCREEN = `
<style>${FONTS}
  body { margin: 0; width: 2048px; height: 1536px; overflow: hidden; background: #060614; }
  .glass { position: absolute; inset: 0; background: radial-gradient(ellipse 80% 70% at 50% 45%, #15143a, #070718 75%); }
  .title { position: absolute; top: 40px; left: 0; right: 0; text-align: center; color: ${GOLD};
    font-family: 'RomanCaps'; font-size: 112px; letter-spacing: 0.18em; text-shadow: 0 0 22px rgba(255, 210, 77, 0.5); }
  .frame { position: absolute; left: 104px; right: 104px; top: 220px; height: 1056px; display: flex; gap: 14px;
    padding: 14px; background: ${BRASS}; border-radius: 18px; box-shadow: 0 0 60px rgba(212, 168, 54, 0.35); }
  .reel { flex: 1; display: flex; flex-direction: column; gap: 14px; }
  .cell { flex: 1; background: radial-gradient(circle at 50% 40%, #1a1940, #0b0b22 80%); border-radius: 8px;
    display: flex; align-items: center; justify-content: center; }
  .cell img { width: 84%; height: 84%; object-fit: contain; }
  .meters { position: absolute; left: 104px; right: 104px; bottom: 70px; display: flex; justify-content: space-between;
    font-family: 'RomanCaps'; font-size: 64px; letter-spacing: 0.12em; color: ${PALE}; }
  .meters b { color: ${GOLD}; font-weight: normal; }
</style>
<div class="glass"></div>
<div class="title">CELESTIAL FORTUNE</div>
<div class="frame">${cells}</div>
<div class="meters"><span>CREDITS <b>1000</b></span><span>BET <b>10</b></span><span>WIN <b>0</b></span></div>
`;

const BADGE = `
<style>${FONTS}
  body { margin: 0; width: 1024px; height: 256px; overflow: hidden;
    background: linear-gradient(180deg, #e6c561 0%, ${BRASS} 45%, #9c7a22 100%); }
  .plate { position: absolute; inset: 14px; border: 6px solid #7a5d16; border-radius: 10px;
    box-shadow: inset 0 0 0 3px #f2d77e; }
  .name { position: absolute; left: 0; right: 0; top: 84px; text-align: center; font-family: 'RomanCaps';
    font-size: 80px; letter-spacing: 0.12em; padding-left: 0.12em; color: #3b2a06;
    text-shadow: 0 2px 0 #f6e19a, 0 -1px 0 #4a3608; }
</style>
<div class="plate"></div>
<div class="name">WALT PARKMAN</div>
`;

const PANELS = [
  { name: 'T_Marquee_CelestialFortune', html: MARQUEE, w: 2048, h: 1024 },
  { name: 'T_CabScreen', html: SCREEN, w: 2048, h: 1536, square: true },
  { name: 'T_CabBadge', html: BADGE, w: 1024, h: 256 },
];

const browser = await chromium.launch();
try {
  for (const p of PANELS) {
    const page = await browser.newPage({ viewport: { width: p.w, height: p.h } });
    await page.setContent(`<html><body>${p.html}</body></html>`);
    await page.evaluate(() => document.fonts.ready);
    const file = join(out, `${p.name}.png`);
    if (p.square) {
      // Stretch the 4:3 picture to a square texture: the screen's UVs restore its shape.
      const shot = await page.screenshot({ clip: { x: 0, y: 0, width: p.w, height: p.h } });
      const sq = await browser.newPage({ viewport: { width: p.w, height: p.w } });
      await sq.setContent(`<html><body style="margin:0"><img style="display:block;width:${p.w}px;height:${p.w}px"
        src="data:image/png;base64,${shot.toString('base64')}"></body></html>`);
      await sq.screenshot({ path: file, clip: { x: 0, y: 0, width: p.w, height: p.w } });
      await sq.close();
    } else {
      await page.screenshot({ path: file, clip: { x: 0, y: 0, width: p.w, height: p.h } });
    }
    await page.close();
    console.log(`${p.name}.png  (${statSync(file).size} bytes)`);
  }
} finally {
  await browser.close();
}
