// make_slot_symbols.mjs - the office slot's reels wear Celestial Fortune's own symbols.
//
// Walt, 2026-10-16: "because you were able to put a sun from the celestial-fortune-web project
// into the AI city, I am hoping that you can put Mars, Saturn and the other graphics from the web
// game into the slot machine that all the agents launch when they grant powers in the office".
// The web game's nine symbols are original SVG made in that project (its docs/ASSET_MANIFEST.md:
// "no downloaded artwork, fonts, tracks, or third-party symbol assets"). Copies live here in
// SourceArt/SlotSymbols. This renders each to a 512 square PNG, transparent round the art, named
// for the sprite the slot screen loads (T_sym_<id>); import_symbol_sprites.py imports them.
//
// The web game plays the same par sheet as USlotScreenWidget's model, with two symbols redrawn:
// its strip code 'c' is the Rocket (the model's Crown, paying 50/180/500) and '7' is the Sun (the
// model's Lucky 7, paying 100/300/1000). So the Crown's sprite is the rocket, the Seven's is the
// sun and the scatter's is the Earth. If the web game's art changes, copy the SVGs over these and
// run this again.
//
//   set PLAYWRIGHT_BROWSERS_PATH=C:\Users\wpark\projects\celestial-fortune-web\.playwright-browsers
//   node Tools/Scripts/make_slot_symbols.mjs

import { readFileSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const art = join(here, '..', '..', 'SourceArt', 'SlotSymbols');
const web = 'C:/Users/wpark/projects/celestial-fortune-web';
const { chromium } = await import(pathToFileURL(join(web, 'node_modules', 'playwright', 'index.mjs')).href);

const SIZE = 512;
// Sprite id (what the slot screen loads) -> the web game's SVG.
const SPRITES = {
  star: 'star', moon: 'moon', galaxy: 'galaxy', saturn: 'saturn', mars: 'mars',
  crown: 'rocket', seven: 'sun', wild: 'wild', scatter: 'earth',
};

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ viewport: { width: SIZE, height: SIZE } });
  for (const [id, file] of Object.entries(SPRITES)) {
    const svg = readFileSync(join(art, `${file}.svg`), 'utf8');
    await page.setContent(`<html><body style="margin:0;background:transparent">${svg}</body></html>`);
    await page.evaluate((size) => {
      const s = document.querySelector('svg');
      s.setAttribute('width', size);
      s.setAttribute('height', size);
      s.style.display = 'block';
    }, SIZE);
    const out = join(art, `T_sym_${id}.png`);
    await page.screenshot({ path: out, omitBackground: true, clip: { x: 0, y: 0, width: SIZE, height: SIZE } });
    console.log(`T_sym_${id}.png  <- ${file}.svg  (${statSync(out).size} bytes)`);
  }
} finally {
  await browser.close();
}
