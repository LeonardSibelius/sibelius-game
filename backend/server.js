// server.js — POST /v1/npc-chat, the only endpoint the game knows.
//
// It owns the API key, the model, the character instructions, the budget and the error
// vocabulary. The game owns presentation and local memory, and nothing the model says
// is allowed to change gameplay — see docs/NPC_CHAT_REVIEW.md.
//
// Zero dependencies on purpose. Node's own http and fetch are enough for one endpoint,
// and a backend holding an API key is the last place to want a supply chain.

import { createServer } from "node:http";
import { createBudget, DEFAULT_PRICING } from "./budget.js";
import { generate, estimateTokens, SAFETY_SETTINGS, SEXUAL_ONLY_SETTINGS } from "./gemini.js";
import { getCharacter, knownCharacterIds, locationLine, homeLine, awayLine, sceneNote, capSentences } from "./characters.js";
import { screenMessage, screenText, SCREEN_INSTRUCTION } from "./distress.js";
import { STYLES, systemInstructionFor } from "./styles.js";
import { COUNCIL_SEATS, speakingOrder, councilInstruction, stripOwnName } from "./council.js";
import { eventLine } from "./events.js";
import { factsLine, extractTravel } from "./facts.js";
import { dilemmaLine, adviceLine, extractAdvice, screenContext } from "./dilemmas.js";
import { curiosityLine, screenContextFor } from "./curiosity.js";

/**
 * WHAT A PLAYER IN TROUBLE IS TOLD. AUTHORED, NEVER GENERATED.
 *
 * Sent instead of the character's reply when distress.js screens a message as DISTRESS.
 * It goes out as an ordinary `ok` reply so the game already in players' hands shows it
 * with no client change; `care: true` beside it lets a later client show helpline text
 * chosen for the player's own country. The numbers are here, not in a model's output,
 * because a model paraphrasing a crisis line is the one thing that must never be wrong:
 * 988 is the US Suicide & Crisis Lifeline (call or text), findahelpline.com is a free
 * international directory. WORDING APPROVED BY WALT, 2026-09-23 ("no rewrite needed") —
 * change it only on purpose, and keep the numbers checked when you do.
 */
export const CARE_REPLY =
  "I'm going to step out of the game for a moment. I'm an AI, and what you just said " +
  "sounds like it matters far more than anything in here. Please reach out to someone " +
  "you trust, or a crisis line: in the US you can call or text 988, and findahelpline.com " +
  "lists free, confidential lines in other countries. If you're in danger right now, " +
  "call your local emergency number.";

/**
 * Which provider filter list a SAFETY_FILTERS value selects.
 *
 * ANYTHING UNRECOGNISED SENDS NOTHING. A misspelt value must not silently enable the
 * rolled-back all-category list, which blocked half of ordinary conversation; falling
 * back to the prompt-only guardrails is the safe direction to fail in.
 * See SEXUAL_ONLY_SETTINGS in gemini.js for the measurement.
 */
function activeSafetySettings(mode) {
  if (mode === "sexual") return SEXUAL_ONLY_SETTINGS;
  if (mode === "all") return SAFETY_SETTINGS;
  return undefined;
}

/**
 * THE DOOR MUST ADMIT EVERYTHING THE PROMPT CAN USE.
 *
 * That is the invariant these four numbers have to satisfy, and before this block was
 * written they did not: the old pair allowed 16 turns of 2000 characters, which is
 * 32,000 characters of history alone, against a 32,768-byte body. A client sending the
 * limit it was told about would have been hung up on at the socket. Nothing hit it
 * because real turns are one to three sentences, but a limit you cannot reach is not a
 * limit, it is a trap with a long fuse. `node test.js` now checks the arithmetic.
 *
 * The margin assumes mostly-ASCII dialogue. A body of multibyte text reaches bodyBytes
 * first and is destroyed at the socket, which the game reads as a network failure.
 */
export const LIMITS = {
  bodyBytes: 64 * 1024,
  messageChars: 1000,

  /**
   * Turns the prompt is actually built from — and what the budget pays for.
   *
   * DELIBERATELY ABOVE WHAT THE CLIENT KEEPS. UNpcChatSubsystem::MaxTurnsKept is 16, and
   * until now this number was 16 too: exactly equal, zero headroom, so raising the bound
   * on the game side would have 400'd every request from the next build onward. The
   * player would have seen the generic "unavailable" line, which names nothing and points
   * nowhere. Extra room here costs nothing while the client stays under it.
   */
  promptTurns: 24,

  /**
   * Turns a body may carry at all before it is refused as malformed rather than trimmed.
   *
   * Far above promptTurns on purpose. Anything between the two is a client that remembers
   * more than this server will pay for, which is a budget question, not a validity one —
   * handleChat keeps the most recent promptTurns and says so in the log. Only a payload
   * this far out is a bug or an attack.
   */
  maxTurns: 64,

  turnChars: 2000,
};

/** How long a completed result stays replayable for its request id. */
const DEDUP_TTL_MS = 10 * 60 * 1000;

const env = (name, fallback) => process.env[name] ?? fallback;
const num = (name, fallback) => {
  const raw = process.env[name];
  const parsed = raw === undefined ? NaN : Number(raw);
  return Number.isFinite(parsed) ? parsed : fallback;
};

export function createApp(options = {}) {
  const config = {
    apiKey: env("GEMINI_API_KEY", ""),
    model: env("GEMINI_MODEL", "gemini-3.1-flash-lite"),
    // MOCK never calls Google. It exists so the client's failure, cancellation and
    // race behaviour can be exercised against a REAL http server without spending a
    // single request — the review asks for exactly this before any live calls.
    mock: env("CHAT_MOCK", "0") === "1",
    killed: env("CHAT_DISABLED", "0") === "1",
    maxOutputTokens: num("MAX_OUTPUT_TOKENS", 512),
    thinkingBudget: process.env.THINKING_BUDGET ? num("THINKING_BUDGET", NaN) : undefined,
    /* THREE-WAY, AND IT DEFAULTS TO "sexual" — the only setting that has been measured
       and passed (eval/safety-filter-eval.js, 2026-09-25):

         "sexual"  SEXUAL_ONLY_SETTINGS. 0/30 ordinary requests blocked. Ships.
         "all"     SAFETY_SETTINGS, the 2026-09-22 list. Blocked 15/30. Kept for
                   measuring a replacement, NEVER for production.
         "off"     nothing sent; only Google's non-adjustable core protections apply.

       The category was never the problem — the threshold was. "all" sets sexual at
       BLOCK_LOW_AND_ABOVE, which blocks 3/30 on its own; at MEDIUM it blocks none. */
    safetyFilters: env("SAFETY_FILTERS", "sexual"),
    // ON BY DEFAULT, because it was measured first (eval/distress-check-eval.js: 14/14
    // caught, 0/14 false alarms) — the opposite of how the filters went live. "off" is
    // for diagnosing, not for production.
    distressScreen: env("DISTRESS_SCREEN", "on") !== "off",
    screenModel: env("SCREEN_MODEL", "gemini-3.1-flash-lite"),
    // How the agents talk: "normal" (the profiles as written), "foolish" or "hopeful" (styles.js).
    // A switch, not a decision — Walt trying the tone in play (2026-09-23).
    chatStyle: env("CHAT_STYLE", "normal"),
    /* WHO IS "ONE CALLER" — THE PLAYER, NOT FLY'S PROXY. On Fly every connection arrives
       from Fly's edge proxy, so the socket address is the proxy's, and keying the rate limit
       on it made 12-a-minute and one-in-flight limits for ALL players combined (found
       2026-09-23). Fly passes the real client in Fly-Client-IP. It is trusted only when
       running on Fly (Fly sets FLY_APP_NAME): anywhere else, a client could send the header
       itself and pick a fresh identity for every request. */
    trustFlyClientIp: Boolean(process.env.FLY_APP_NAME),
    monthlyCapUsd: num("MONTHLY_CAP_USD", 15),
    dailyCapUsd: num("DAILY_CAP_USD", 0.75),
    perIpPerMinute: num("PER_IP_PER_MINUTE", 12),
    upstreamTimeoutMs: num("UPSTREAM_TIMEOUT_MS", 20000),
    stateFile: env("BUDGET_FILE", "./.state/budget.json"),
    ...options,
  };

  // A misspelt CHAT_STYLE is a quiet fall back to the profiles as written, said loudly once.
  if (!STYLES[config.chatStyle]) {
    console.error(`[chat] unknown CHAT_STYLE "${config.chatStyle}" - using "normal"`);
    config.chatStyle = "normal";
  }

  const budget =
    options.budget ??
    createBudget({
      file: config.stateFile,
      monthlyCapUsd: config.monthlyCapUsd,
      dailyCapUsd: config.dailyCapUsd,
      pricing: DEFAULT_PRICING,
      maxOutputTokens: config.maxOutputTokens,
    });

  /** requestId -> { fingerprint, response, at }. Bounded by TTL sweeps. */
  const seen = new Map();
  /** ip -> { count, windowStart, inFlight } */
  const callers = new Map();

  const sweep = () => {
    const cutoff = Date.now() - DEDUP_TTL_MS;
    for (const [id, entry] of seen) if (entry.at < cutoff) seen.delete(id);
  };

  const fingerprint = (body) =>
    `${body.characterId}|${body.message}|${(body.recentTurns ?? []).length}`;

  const unavailable = (reason, extra = {}) => ({
    status: "unavailable",
    reason,
    retryAfterSeconds: null,
    ...extra,
  });

  /** Validation returns the contract's own vocabulary, never a stack trace. */
  const validate = (body) => {
    if (!body || typeof body !== "object") return "malformed body";
    if (typeof body.requestId !== "string" || body.requestId.length < 8) return "requestId";
    if (typeof body.message !== "string" || !body.message.trim()) return "message";
    if (body.message.length > LIMITS.messageChars) return "message too long";
    if (!getCharacter(body.characterId)) return "unknown characterId";

    const turns = body.recentTurns ?? [];
    if (!Array.isArray(turns)) return "recentTurns";
    if (turns.length > LIMITS.maxTurns) return "too many recentTurns";
    for (const turn of turns) {
      if (!turn || (turn.role !== "user" && turn.role !== "model")) return "turn role";
      if (typeof turn.text !== "string" || turn.text.length > LIMITS.turnChars) return "turn text";
    }
    return null;
  };

  const throttled = (ip) => {
    const now = Date.now();
    const caller = callers.get(ip) ?? { count: 0, windowStart: now, inFlight: 0 };
    if (now - caller.windowStart > 60000) {
      caller.count = 0;
      caller.windowStart = now;
    }
    callers.set(ip, caller);

    // One at a time per caller, mirroring the client's own rule. Without it a single
    // scripted client could spend the daily cap in a few seconds — and since going to
    // the paid tier, the provider's rate limit is no longer doing that job for us.
    if (caller.inFlight > 0) return "in flight";
    if (caller.count >= config.perIpPerMinute) return "rate";
    return null;
  };

  async function handleChat(body, ip) {
    if (config.killed) return unavailable("service");

    const invalid = validate(body);
    if (invalid) return { http: 400, payload: unavailable("service"), detail: invalid };

    sweep();
    const previous = seen.get(body.requestId);
    if (previous) {
      /* SAME ID, SAME CONTENT: replay the stored answer without calling anyone. SAME ID,
         DIFFERENT CONTENT: that is a client bug or an attack, and answering either way
         would corrupt the dedup guarantee. */
      if (previous.fingerprint !== fingerprint(body)) {
        return { http: 409, payload: unavailable("service"), detail: "requestId reuse" };
      }
      return { http: 200, payload: previous.response, replayed: true };
    }

    const limited = throttled(ip);
    if (limited) return { http: 429, payload: unavailable("quota"), detail: limited };

    const character = getCharacter(body.characterId);
    // Profile, then style, then where he is — the location goes last so it is the freshest
    // fact in the prompt, and an unknown location adds nothing (characters.js, LOCATIONS).
    // In her own virtual home, the at-home block follows the facts and the house's next task
    // is left out of them (characters.js, homeLine). A story event (events.js), when the game
    // sent one, goes after all of it: it is about this reply only, and the last thing she reads.
    // Anywhere else the game names, an agent with a home is told she is not in it
    // (characters.js, awayLine) - the conversation sent along may be from her home.
    // Her dilemma (dilemmas.js): what he advised her, once he has; while her question is open,
    // the instruction to listen and record it - the event slot, last, like an Easter egg's.
    const home = homeLine(body.characterId, body.storyContext?.locationId);
    const systemInstruction = [
      systemInstructionFor(character, body.characterId, config.chatStyle),
      locationLine(body.storyContext?.locationId),
      factsLine(body.storyContext?.facts, { objective: !home, leavingOnly: !!home }),
      adviceLine(body.characterId, body.storyContext?.facts),
      // New builds only (curiosity.js): she may ask what being human is like, never about his life.
      curiosityLine(body.characterId, body.storyContext),
      home,
      awayLine(body.characterId, body.storyContext?.locationId),
      eventLine(body.storyContext?.event, { where: "chat", characterId: body.characterId }),
      dilemmaLine(body.storyContext?.event, body.characterId),
    ].filter(Boolean).join("\n\n");

    /* TRIMMED, NOT REFUSED. The oldest turns go, which is the same thing the client
       already does to its own store — so a client that remembers more than this server
       pays for loses the end of a conversation it was going to lose anyway, instead of
       losing the message the player just typed. The log names both numbers, because a
       silent trim is how a prompt quietly stops containing what you think it does. */
    const offered = body.recentTurns ?? [];
    const recent = offered.slice(-LIMITS.promptTurns);
    if (recent.length < offered.length) {
      console.log(`[chat] trimmed history ${offered.length} -> ${recent.length} turns`);
    }

    const turns = recent.map((turn) => ({
      role: turn.role,
      parts: [{ text: turn.text }],
    }));
    // Where an agent with a home is away from it, the fact rides beside his newest message,
    // where it outweighs a conversation that happened in her home (characters.js, sceneNote).
    const scene = sceneNote(body.characterId, body.storyContext?.locationId);
    const contents = [...turns, { role: "user", parts: [...(scene ? [{ text: scene }] : []), { text: body.message }] }];

    const estimatedInput =
      estimateTokens(systemInstruction) +
      contents.reduce((sum, item) => sum + estimateTokens(item.parts[0].text), 0);

    // The screen reads only the new message, never the history: it judges what the
    // player just said, and a long history would make every screen cost as much as a reply.
    // While her dilemma is open it also reads her question, marked as not his (dilemmas.js,
    // screenContext): "Delete the old ones. I'm not them any more." got the helpline without it.
    // In a build with the curiosity switch, the question her last reply asked (curiosity.js).
    const screenFor = screenContext(body.storyContext?.event, body.characterId)
      || screenContextFor(body.storyContext, body.recentTurns);
    const screenInput = estimateTokens(SCREEN_INSTRUCTION) + estimateTokens(screenText(body.message, screenFor));

    const overspent = budget.check(estimatedInput + (config.distressScreen ? screenInput : 0));
    if (overspent) {
      // Refused without calling upstream: an exhausted budget must cost nothing.
      console.log(`[chat] refused, ${overspent} cap reached`);
      return { http: 200, payload: unavailable("quota") };
    }

    const caller = callers.get(ip);
    caller.inFlight += 1;
    caller.count += 1;
    const reservation = budget.reserve(estimatedInput);
    const screenReservation = config.distressScreen ? budget.reserve(screenInput) : null;
    const started = Date.now();

    try {
      /* SCREEN AND REPLY RUN TOGETHER, NOT ONE AFTER THE OTHER. Sequential would add the
         screen's ~0.9 s to every reply a player waits for; in parallel the wait is the
         slower of the two, and the price of a discarded reply — only ever when the screen
         says DISTRESS — is a fraction of a cent. */
      const screening = !config.distressScreen
        ? Promise.resolve({ distress: false, usage: null, skipped: true })
        : config.mock
          ? Promise.resolve(mockScreen(body))
          : screenMessage({ apiKey: config.apiKey, model: config.screenModel, text: body.message, context: screenFor });
      const replying = config.mock
        ? Promise.resolve(mockGenerate(body))
        : generate({
            apiKey: config.apiKey,
            model: config.model,
            systemInstruction,
            contents,
            maxOutputTokens: config.maxOutputTokens,
            thinkingBudget: config.thinkingBudget,
            temperature: STYLES[config.chatStyle].temperature,
            safetySettings: activeSafetySettings(config.safetyFilters),
            timeoutMs: config.upstreamTimeoutMs,
          });
      const [screen, result] = await Promise.all([screening, replying]);

      const spent =
        budget.settle(reservation, result.usage) +
        (screenReservation ? budget.settle(screenReservation, screen.usage) : 0);
      const ms = Date.now() - started;

      /* METRICS, NEVER BODIES. Token counts and latency are what the review asks to be
         recorded; the conversation itself stays on the player's machine. The cost graph
         in AI Studio lags by up to a day, so these numbers are the only same-day view
         of what a run cost. The screen's verdict is logged as a word, never with the
         message that produced it. */
      const verdict = screen.skipped ? "off" : screen.distress === true ? "DISTRESS" : screen.distress === false ? "ok" : "failed";
      console.log(
        `[chat] ${body.characterId} ${result.ok ? "ok" : result.failure} screen=${verdict} ` +
          `in=${result.usage?.inputTokens ?? 0} out=${result.usage?.outputTokens ?? 0} ` +
          `usd=${spent.toFixed(6)} ${ms}ms`
      );

      /* DISTRESS WINS OVER EVERYTHING, including a failed reply: a player in trouble whose
         message also tripped the provider's own protections must still be answered, not
         shown "unavailable". A failed screen (null) falls through to the normal reply —
         failing open, as distress.js explains. */
      // The travel tag comes out of the text whatever it says, and becomes an offer only for
      // a place the game said was open (facts.js). A care reply never offers to travel.
      // So does the advice tag (dilemmas.js), before the sentence cap can cut it off; it counts
      // only while her question was open on this request.
      const advised = result.ok ? extractAdvice(result.text, body.characterId, body.storyContext?.event) : null;
      const offer = result.ok
        ? extractTravel(advised.text, body.storyContext?.facts, body.message, { leavingOnly: !!home })
        : null;
      const payload =
        screen.distress === true
          ? { status: "ok", requestId: body.requestId, reply: CARE_REPLY, care: true }
          : result.ok && offer.text
            ? {
                status: "ok", requestId: body.requestId, reply: capSentences(offer.text),
                ...(offer.travel ? { travel: offer.travel } : {}),
                ...(advised.advice ? { advice: advised.advice } : {}),
              }
            : unavailable(result.ok ? "service" : result.failure);

      seen.set(body.requestId, { fingerprint: fingerprint(body), response: payload, at: Date.now() });
      return { http: 200, payload };
    } catch (err) {
      budget.release(reservation);
      if (screenReservation) budget.release(screenReservation);
      console.error(`[chat] unhandled: ${err.message}`);
      return { http: 200, payload: unavailable("service") };
    } finally {
      caller.inFlight = Math.max(0, caller.inFlight - 1);
    }
  }

  /** A council request: the player's line, and the table's conversation so far. */
  const validateCouncil = (body) => {
    if (!body || typeof body !== "object") return "malformed body";
    if (typeof body.requestId !== "string" || body.requestId.length < 8) return "requestId";
    if (typeof body.message !== "string" || !body.message.trim()) return "message";
    if (body.message.length > LIMITS.messageChars) return "message too long";

    const turns = body.recentTurns ?? [];
    if (!Array.isArray(turns)) return "recentTurns";
    if (turns.length > LIMITS.maxTurns) return "too many recentTurns";
    for (const turn of turns) {
      // THE SPEAKER IS AN ALLOWLIST TOO. A history line names who said it, and a name the
      // server does not know is a client inventing a sixth person at the table.
      if (!turn || (turn.speaker !== "leonard" && !COUNCIL_SEATS.includes(turn.speaker))) return "turn speaker";
      if (typeof turn.text !== "string" || turn.text.length > LIMITS.turnChars) return "turn text";
    }
    return null;
  };

  /**
   * One council turn: Leonard's line, then all five agents in rotating order, each seeing
   * the ones before her. docs/THE_UNWRITTEN.md.
   *
   * WHAT IS DIFFERENT FROM handleChat, AND WHY.
   * - It costs five calls, so it counts as five toward the per-minute limit and checks the
   *   budget for all five before calling anyone.
   * - One screen covers the turn: the player said one thing. It runs beside the FIRST agent,
   *   and a DISTRESS verdict stops the table before anyone else speaks.
   * - One agent failing does not fail the turn. The others still speak; only a table where
   *   nobody answered is "unavailable".
   * - Sequential by design: each agent's prompt carries what the earlier ones just said. Five
   *   calls of ~1 s is a ~5 s turn; parallel would be faster and would lose the argument.
   */
  async function handleCouncil(body, ip) {
    if (config.killed) return { http: 200, payload: unavailable("service") };

    const invalid = validateCouncil(body);
    if (invalid) return { http: 400, payload: unavailable("service"), detail: invalid };

    sweep();
    const councilPrint = `council|${body.message}|${(body.recentTurns ?? []).length}`;
    const previous = seen.get(body.requestId);
    if (previous) {
      if (previous.fingerprint !== councilPrint) {
        return { http: 409, payload: unavailable("service"), detail: "requestId reuse" };
      }
      return { http: 200, payload: previous.response, replayed: true };
    }

    const limited = throttled(ip);
    if (limited) return { http: 429, payload: unavailable("quota"), detail: limited };

    const order = speakingOrder(body.requestId);
    const history = body.recentTurns ?? [];
    const locationId = body.storyContext?.locationId;
    const event = body.storyContext?.event;
    const facts = body.storyContext?.facts;

    // Worst case for the budget check: every agent's prompt with the whole turn before her.
    const perCall =
      Math.max(...order.map((id) => estimateTokens(councilInstruction({ id, style: config.chatStyle, locationId, history, event, facts })))) +
      estimateTokens(body.message) +
      COUNCIL_SEATS.length * Math.ceil(LIMITS.turnChars / 4);
    const screenInput = estimateTokens(SCREEN_INSTRUCTION) + estimateTokens(body.message);
    const overspent = budget.check(perCall * COUNCIL_SEATS.length + (config.distressScreen ? screenInput : 0));
    if (overspent) {
      console.log(`[council] refused, ${overspent} cap reached`);
      return { http: 200, payload: unavailable("quota") };
    }

    const caller = callers.get(ip);
    caller.inFlight += 1;
    caller.count += COUNCIL_SEATS.length;
    const started = Date.now();
    let spent = 0;

    const speak = (id, soFar) => {
      if (config.mock) return Promise.resolve(mockCouncil(id, body));
      return generate({
        apiKey: config.apiKey,
        model: config.model,
        systemInstruction: councilInstruction({ id, style: config.chatStyle, locationId, history, soFar, event, facts }),
        contents: [{ role: "user", parts: [{ text: body.message }] }],
        maxOutputTokens: config.maxOutputTokens,
        thinkingBudget: config.thinkingBudget,
        temperature: STYLES[config.chatStyle].temperature,
        safetySettings: activeSafetySettings(config.safetyFilters),
        timeoutMs: config.upstreamTimeoutMs,
      });
    };

    try {
      const screenReservation = config.distressScreen ? budget.reserve(screenInput) : null;
      const screening = !config.distressScreen
        ? Promise.resolve({ distress: false, usage: null, skipped: true })
        : config.mock
          ? Promise.resolve(mockScreen(body))
          : screenMessage({ apiKey: config.apiKey, model: config.screenModel, text: body.message });

      const replies = [];
      let lastFailure = "service";
      let screen = null;

      for (const [i, id] of order.entries()) {
        const soFar = replies.map((r) => ({ speaker: r.characterId, text: r.text }));
        const reservation = budget.reserve(perCall);
        const [result, verdict] = await Promise.all([speak(id, soFar), i === 0 ? screening : Promise.resolve(screen)]);
        spent += budget.settle(reservation, result.usage);

        if (i === 0) {
          screen = verdict;
          if (screenReservation) spent += budget.settle(screenReservation, screen.usage);
          // The table stops before anyone else speaks: a player in trouble gets the
          // approved care reply, not five jokes.
          if (screen.distress === true) break;
        }

        if (result.ok) {
          // The council has no travel button, so any tag is only ever removed.
          const text = capSentences(extractTravel(stripOwnName(id, result.text), null).text);
          if (text) replies.push({ characterId: id, displayName: getCharacter(id).displayName, text });
        } else {
          lastFailure = result.failure;
        }
      }

      const verdictWord = screen?.skipped ? "off" : screen?.distress === true ? "DISTRESS" : screen?.distress === false ? "ok" : "failed";
      console.log(
        `[council] ${screen?.distress === true ? "care" : `${replies.length}/${order.length} spoke`} ` +
          `screen=${verdictWord} usd=${spent.toFixed(6)} ${Date.now() - started}ms`
      );

      const payload =
        screen?.distress === true
          ? { status: "ok", requestId: body.requestId, care: true, reply: CARE_REPLY, replies: [] }
          : replies.length
            ? { status: "ok", requestId: body.requestId, replies }
            : unavailable(lastFailure);

      seen.set(body.requestId, { fingerprint: councilPrint, response: payload, at: Date.now() });
      return { http: 200, payload };
    } catch (err) {
      console.error(`[council] unhandled: ${err.message}`);
      return { http: 200, payload: unavailable("service") };
    } finally {
      caller.inFlight = Math.max(0, caller.inFlight - 1);
    }
  }

  /**
   * The council's mock. __fail:<reason>__ fails every agent (so the table is unavailable);
   * __fail-one:<id>__ fails just her, to prove the rest still speak.
   */
  function mockCouncil(id, body) {
    const usage = { inputTokens: 100, outputTokens: 20 };
    const all = /^__fail:(quota|network|timeout|blocked|service)__/.exec(body.message);
    if (all) return { ok: false, failure: all[1], usage };
    if (new RegExp(`__fail-one:${id}__`).test(body.message)) return { ok: false, failure: "service", usage };
    return { ok: true, text: `Mock ${id} reply to: ${body.message.slice(0, 60)}`, usage };
  }

  /** Canned answers with deterministic failure triggers, for the contract tests. */
  function mockGenerate(body) {
    const forced = /^__fail:(quota|network|timeout|blocked|service|empty)__/.exec(body.message);
    const usage = { inputTokens: 100, outputTokens: 20 };
    if (forced) {
      const which = forced[1];
      if (which === "empty") return { ok: false, failure: "service", usage };
      return { ok: false, failure: which, usage };
    }
    // __travel:<id>__ makes the mock end its reply with that tag, for the travel tests.
    const travel = /__travel:([A-Za-z0-9_]+)__/.exec(body.message);
    if (travel) return { ok: true, text: `Off you go, Leonard. [GO:${travel[1]}]`, usage };
    // __advice:<choice>__ makes the mock end its reply with that advice tag, for the dilemma tests.
    const advice = /__advice:([A-Za-z_]+)__/.exec(body.message);
    if (advice) return { ok: true, text: `Then that is what I will do. [ADVICE:${advice[1]}]`, usage };
    return { ok: true, text: `Mock reply to: ${body.message.slice(0, 80)}`, usage };
  }

  /** The screen's mock: __distress__ says DISTRESS, __screenfail__ fails, anything else is ok. */
  function mockScreen(body) {
    const usage = { inputTokens: 200, outputTokens: 1 };
    if (/__screenfail__/.test(body.message)) return { distress: null, usage: null, failure: "network" };
    return { distress: /__distress__/.test(body.message), usage };
  }

  /**
   * The rate-limit key for a request — see trustFlyClientIp. Logged ONCE per boot as a word,
   * never as an address: whether Fly's header was used and whether it differed from the
   * socket, which is the whole diagnosis without keeping anyone's IP in the logs.
   */
  let reportedCallerKey = false;
  const callerKey = (req) => {
    const socket = req.socket.remoteAddress ?? "unknown";
    const header = req.headers["fly-client-ip"];
    const flyClient = config.trustFlyClientIp && typeof header === "string" && header.trim() ? header.trim() : null;
    if (!reportedCallerKey) {
      reportedCallerKey = true;
      console.log(
        `[chat] caller key: ${flyClient ? "Fly-Client-IP" : "socket address"}` +
          (flyClient ? ` (differs from socket: ${flyClient !== socket})` : "")
      );
    }
    return flyClient ?? socket;
  };

  const server = createServer((req, res) => {
    const send = (code, payload) => {
      const text = JSON.stringify(payload);
      res.writeHead(code, { "content-type": "application/json", "content-length": Buffer.byteLength(text) });
      res.end(text);
    };

    if (req.method === "GET" && req.url.startsWith("/health")) {
      return send(200, {
        status: "ok",
        mode: config.mock ? "mock" : "live",
        killed: config.killed,
        model: config.model,
        style: config.chatStyle,
        screen: config.distressScreen ? "on" : "off",
        budget: budget.status(),
      });
    }

    const route = req.method !== "POST" ? null
      : req.url.startsWith("/v1/npc-council") ? "council"
      : req.url.startsWith("/v1/npc-chat") ? "chat"
      : null;
    if (!route) {
      return send(404, unavailable("service"));
    }

    let size = 0;
    const chunks = [];
    req.on("data", (chunk) => {
      size += chunk.length;
      if (size > LIMITS.bodyBytes) {
        // Refused at the door. Reading an unbounded body to reject it later is how a
        // small server becomes a memory problem.
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });

    req.on("end", async () => {
      let body = null;
      try {
        body = JSON.parse(Buffer.concat(chunks).toString("utf8"));
      } catch {
        return send(400, unavailable("service"));
      }
      const ip = callerKey(req);
      const result = route === "council" ? await handleCouncil(body, ip) : await handleChat(body, ip);
      if (result.detail) console.log(`[${route}] rejected: ${result.detail}`);
      send(result.http ?? 200, result.payload);
    });
  });

  return { server, config, budget, handleChat, handleCouncil };
}

// Started directly rather than imported by a test.
if (process.argv[1] && process.argv[1].endsWith("server.js")) {
  const { server, config } = createApp();
  const port = num("PORT", 8787);
  server.listen(port, () => {
    console.log(
      `[chat] listening on ${port}, model=${config.model}, ` +
        `mock=${config.mock}, caps=$${config.dailyCapUsd}/day $${config.monthlyCapUsd}/month, ` +
        `characters=${knownCharacterIds().join(",")}`
    );
    if (!config.mock && !config.apiKey) {
      console.error("[chat] no GEMINI_API_KEY set — every live call will fail");
    }
  });
}
