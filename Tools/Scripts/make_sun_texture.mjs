// make_sun_texture.mjs - the AI city's sun is Celestial Fortune's sun, in layers.
//
// Walt, 2026-10-15: "could the sun here be made to look like my sun in
// https://leonardsibelius.com/celestial-fortune/ ? I like the sun in that slot game". The slot's
// sun is a picture, not code: celestial-fortune-web/src/render/symbols/sun.svg - original art
// made in that project ("no downloaded artwork", its docs/ASSET_MANIFEST.md). A copy lives here
// as SourceArt/Genesis/sun.svg. Then, of the animation: "let's do the medium please" - each part
// of it moving on its own. So this renders the SVG whole (T_Sun.png) and in layers, each a
// 2048 square with everything else transparent, in the same frame so they stack back exactly:
//
//   T_SunCorona     the glow behind it all
//   T_SunLoopNE/SE/SW/N   one prominence each: its red bloom, its fill, its hot core
//   T_SunDisc       the photosphere: surface, plasma, granules, the bright active regions,
//                   the fibrils, the limb, the rim - everything on the face but the spots and
//                   the flare
//   T_SunSpots      the three sunspots, clipped to the disc as in the SVG
//   T_SunFlare      the flare kernel
//
// make_genesis_assets.py imports them; M_GenesisSunLive stacks and moves them. If the sun is ever
// changed, copy the new SVG over this one and run this again (the layer picks below follow the
// SVG's order: corona, bloom, wisps, cores, surface, the clipped face, rim).
//
//   set PLAYWRIGHT_BROWSERS_PATH=C:\Users\wpark\projects\celestial-fortune-web\.playwright-browsers
//   node Tools/Scripts/make_sun_texture.mjs

import { readFileSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const art = join(here, '..', '..', 'SourceArt', 'Genesis');
const web = 'C:/Users/wpark/projects/celestial-fortune-web';
const { chromium } = await import(pathToFileURL(join(web, 'node_modules', 'playwright', 'index.mjs')).href);

const SIZE = 2048;
const svg = readFileSync(join(art, 'sun.svg'), 'utf8');

// Which parts each layer keeps, by the SVG's own structure (see the header).
const LOOPS = ['NE', 'SE', 'SW', 'N'];
const LAYERS = {
  T_Sun: null,                                     // the whole picture
  T_SunCorona: { corona: true },
  ...Object.fromEntries(LOOPS.map((name, i) => [`T_SunLoop${name}`, { loop: i }])),
  T_SunDisc: { disc: true },
  T_SunSpots: { spots: true },
  T_SunFlare: { flare: true },
};

const browser = await chromium.launch();
try {
  const page = await browser.newPage({ viewport: { width: SIZE, height: SIZE } });
  await page.setContent(`<html><body style="margin:0;background:transparent">${svg}</body></html>`);
  await page.evaluate((size) => {
    const s = document.querySelector('svg');
    s.setAttribute('width', size);
    s.setAttribute('height', size);
    s.style.display = 'block';
  }, SIZE);

  for (const [name, keep] of Object.entries(LAYERS)) {
    await page.evaluate((keep) => {
      const s = document.querySelector('svg');
      const top = [...s.children].filter((e) => !['title', 'desc', 'defs'].includes(e.tagName));
      // corona, bloom group, wisp group, core group, surface, clipped face group, rim
      const [corona, bloom, wisps, cores, surface, face, rim] = top;
      const show = (e, on) => { e.style.display = on ? '' : 'none'; };
      for (const e of top) show(e, keep === null);
      for (const g of [bloom, wisps, cores]) {
        [...g.children].filter((e) => e.tagName === 'path').forEach((p, i) => show(p, keep === null || keep.loop === i));
      }
      // The face: rect plasma, rect granules, two active regions, the flare group, three spots,
      // the fibril group, the limb.
      const parts = [...face.children];
      const flare = parts.find((e) => e.tagName === 'g' && e.getAttribute('transform'));
      const spots = parts.filter((e) => e.tagName === 'ellipse' && (e.getAttribute('fill') || '').includes('sun-spot'));
      for (const e of parts) {
        const isFlare = e === flare;
        const isSpot = spots.includes(e);
        show(e, keep === null || (keep.disc && !isFlare && !isSpot) || (keep.spots && isSpot) || (keep.flare && isFlare));
      }
      if (keep !== null) {
        show(corona, !!keep.corona);
        for (const g of [bloom, wisps, cores]) show(g, keep.loop !== undefined);
        show(surface, !!keep.disc);
        show(face, !!(keep.disc || keep.spots || keep.flare));
        show(rim, !!keep.disc);
      }
    }, keep);
    const out = join(art, `${name}.png`);
    await page.screenshot({ path: out, omitBackground: true });
    console.log(`${name}: ${statSync(out).size} bytes`);
  }
} finally {
  await browser.close();
}
