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
import { generate, estimateTokens } from "./gemini.js";
import { getCharacter, knownCharacterIds } from "./characters.js";

const LIMITS = {
  bodyBytes: 32 * 1024,
  messageChars: 1000,
  recentTurns: 16,
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
    monthlyCapUsd: num("MONTHLY_CAP_USD", 15),
    dailyCapUsd: num("DAILY_CAP_USD", 0.75),
    perIpPerMinute: num("PER_IP_PER_MINUTE", 12),
    upstreamTimeoutMs: num("UPSTREAM_TIMEOUT_MS", 20000),
    stateFile: env("BUDGET_FILE", "./.state/budget.json"),
    ...options,
  };

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
    if (turns.length > LIMITS.recentTurns) return "too many recentTurns";
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
    const turns = (body.recentTurns ?? []).map((turn) => ({
      role: turn.role,
      parts: [{ text: turn.text }],
    }));
    const contents = [...turns, { role: "user", parts: [{ text: body.message }] }];

    const estimatedInput =
      estimateTokens(character.systemInstruction) +
      contents.reduce((sum, item) => sum + estimateTokens(item.parts[0].text), 0);

    const overspent = budget.check(estimatedInput);
    if (overspent) {
      // Refused without calling upstream: an exhausted budget must cost nothing.
      console.log(`[chat] refused, ${overspent} cap reached`);
      return { http: 200, payload: unavailable("quota") };
    }

    const caller = callers.get(ip);
    caller.inFlight += 1;
    caller.count += 1;
    const reservation = budget.reserve(estimatedInput);
    const started = Date.now();

    try {
      const result = config.mock
        ? mockGenerate(body)
        : await generate({
            apiKey: config.apiKey,
            model: config.model,
            systemInstruction: character.systemInstruction,
            contents,
            maxOutputTokens: config.maxOutputTokens,
            thinkingBudget: config.thinkingBudget,
            timeoutMs: config.upstreamTimeoutMs,
          });

      const spent = budget.settle(reservation, result.usage);
      const ms = Date.now() - started;

      /* METRICS, NEVER BODIES. Token counts and latency are what the review asks to be
         recorded; the conversation itself stays on the player's machine. The cost graph
         in AI Studio lags by up to a day, so these numbers are the only same-day view
         of what a run cost. */
      console.log(
        `[chat] ${body.characterId} ${result.ok ? "ok" : result.failure} ` +
          `in=${result.usage?.inputTokens ?? 0} out=${result.usage?.outputTokens ?? 0} ` +
          `usd=${spent.toFixed(6)} ${ms}ms`
      );

      const payload = result.ok
        ? { status: "ok", requestId: body.requestId, reply: result.text }
        : unavailable(result.failure);

      seen.set(body.requestId, { fingerprint: fingerprint(body), response: payload, at: Date.now() });
      return { http: 200, payload };
    } catch (err) {
      budget.release(reservation);
      console.error(`[chat] unhandled: ${err.message}`);
      return { http: 200, payload: unavailable("service") };
    } finally {
      caller.inFlight = Math.max(0, caller.inFlight - 1);
    }
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
    return { ok: true, text: `Mock reply to: ${body.message.slice(0, 80)}`, usage };
  }

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
        budget: budget.status(),
      });
    }

    if (req.method !== "POST" || !req.url.startsWith("/v1/npc-chat")) {
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
      const ip = req.socket.remoteAddress ?? "unknown";
      const result = await handleChat(body, ip);
      if (result.detail) console.log(`[chat] rejected: ${result.detail}`);
      send(result.http ?? 200, result.payload);
    });
  });

  return { server, config, budget, handleChat };
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
