// news-candidates.js — harvests candidates for news.js. A TOOL, NOT A DEPENDENCY.
//
//     node news-candidates.js
//
// Nothing in the running server calls this. It exists so refreshing THE_WORLD_OUTSIDE is
// ten minutes instead of an afternoon, and so the next person knows where those six lines
// came from. Run it, read the list, pick three or four, rewrite each as one plain sentence
// with no source and no company name, paste into news.js, deploy.
//
// WHY SHOW HN AND NOT "AI NEWS". Walt's brief is that the player should feel they could be
// a Leonard — one person, empowered. Measured on 2026-09-25: a general AI-news harvest of
// high-scoring stories returned court rulings, an NSA procurement leak, funding rounds and
// two pieces about AI replacing workers. Aimed at Show HN instead, the same harvester
// returned people who had trained a model on one machine, shrunk one to fourteen megabytes,
// and taught one to keep learning on a gaming GPU. Every Show HN is one person who built a
// thing, which IS the brief. The feed choice is doing the editorial work.
//
// THE FILTER IS DELIBERATELY OPINIONATED. It drops what would invert the game's message at
// a man the industry retired: layoffs, replacement, litigation, weapons, funding. Those are
// real AI news and they are not this game's.

const WINDOW_DAYS = 150;
const MIN_POINTS = 100;
const PAGES = 4;

/** Titles that touch the machine learning to be interesting here. */
const RELEVANT =
  /\b(AI|LLM|GPT|model|agent|neural|diffusion|transcri|voice|vision|semantic|embedding|inference|fine-?tun)\b/i;

/** Dropped on sight — see the note above about inverting the message. */
const EXCLUDE =
  /lawsuit|sues?\b|court|copyright|layoff|fired|job loss|replace.*(worker|job)|NSA|surveillanc|militar|weapon|funding|valuation|raises \$|IPO|acqui|CEO|doom|extinction|regulat|election/i;

async function harvest() {
  const since = Math.floor(Date.now() / 1000) - WINDOW_DAYS * 86400;
  const seen = new Map();

  for (let page = 0; page < PAGES; page++) {
    const url =
      `https://hn.algolia.com/api/v1/search?tags=show_hn` +
      `&numericFilters=points%3E${MIN_POINTS},created_at_i%3E${since}` +
      `&hitsPerPage=50&page=${page}`;
    try {
      const response = await fetch(url);
      if (!response.ok) continue;
      const payload = await response.json();
      for (const hit of payload.hits ?? []) {
        if (hit?.title) seen.set(hit.objectID, hit);
      }
    } catch (err) {
      // A harvester that dies on one bad page is worse than one that returns fewer rows.
      console.error(`  (page ${page} failed: ${err.message})`);
    }
  }
  return seen;
}

const seen = await harvest();
const candidates = [...seen.values()]
  .filter((h) => RELEVANT.test(h.title) && !EXCLUDE.test(h.title))
  .sort((a, b) => b.points - a.points);

console.log(`scanned ${seen.size} Show HN posts over ${WINDOW_DAYS} days`);
console.log(`${candidates.length} candidates after filtering\n`);
console.log(`THE TEST: does this show ONE PERSON doing what used to need an institution?\n`);

for (const hit of candidates.slice(0, 25)) {
  const days = Math.round((Date.now() - new Date(hit.created_at)) / 86400000);
  console.log(`[${String(hit.points).padStart(4)}pt ${String(days).padStart(3)}d] ${hit.title}`);
  console.log(`            https://news.ycombinator.com/item?id=${hit.objectID}`);
}

console.log(`\nPick 3-4. Rewrite each as one plain sentence: no source, no company name,`);
console.log(`no link. Then replace ITEMS in news.js and redeploy.`);
