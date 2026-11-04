// budget.js — what stops the spending.
//
// THE ONLY CONTROL THAT STOPS AT *OUR* NUMBER. Google enforces two ceilings above this
// one: the prepay balance ($25, auto-reload off) and the billing-account tier cap. Both
// are hard stops, and both are the wrong place to find out you have run out — a player
// hits them as an upstream error in the middle of a conversation. This counter stops
// first and lets the game say something in Kaia's voice instead.
//
// RESERVE BEFORE DISPATCH, RECONCILE AFTER. A request's real cost is only known when the
// reply comes back with its token counts, so we reserve an estimate up front and correct
// it afterwards. Checking only after the call would let a burst spend past the cap and
// find out later.
//
// SINGLE INSTANCE. State is a JSON file, which is correct for one small process serving
// roughly a thousand requests a day. Run two of these and the accounting silently
// becomes advisory — see README.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname } from "node:path";

/** Gemini 3.1 Flash-Lite, USD per million tokens. Overridable: prices move. */
export const DEFAULT_PRICING = { inputPerM: 0.25, outputPerM: 1.5 };

const emptyState = () => ({ day: "", daySpent: 0, month: "", monthSpent: 0, calls: 0 });

export function createBudget({
  file,
  monthlyCapUsd,
  dailyCapUsd,
  pricing = DEFAULT_PRICING,
  maxOutputTokens,
  now = () => new Date(),
}) {
  let state = emptyState();

  try {
    state = { ...emptyState(), ...JSON.parse(readFileSync(file, "utf8")) };
  } catch {
    // Absent or unreadable is a fresh ledger, not an error. A backend that refuses to
    // start because it has never spent anything would be its own outage.
  }

  const keys = () => {
    const d = now();
    const day = d.toISOString().slice(0, 10);
    return { day, month: day.slice(0, 7) };
  };

  const roll = () => {
    const { day, month } = keys();
    if (state.day !== day) { state.day = day; state.daySpent = 0; }
    if (state.month !== month) { state.month = month; state.monthSpent = 0; }
  };

  const persist = () => {
    try {
      mkdirSync(dirname(file), { recursive: true });
      writeFileSync(file, JSON.stringify(state), "utf8");
    } catch (err) {
      // Losing the ledger is bad; refusing to answer because we could not write it is
      // worse. Log loudly and keep the in-memory count, which still holds for this run.
      console.error(`[budget] could not persist: ${err.message}`);
    }
  };

  const cost = (inputTokens, outputTokens) =>
    (inputTokens / 1e6) * pricing.inputPerM + (outputTokens / 1e6) * pricing.outputPerM;

  /** The worst a single call can cost, used as the up-front reservation. */
  const estimate = (inputTokens) => cost(inputTokens, maxOutputTokens);

  return {
    /**
     * Can we afford one more call? Returns null when yes, or the reason when no.
     *
     * The DAILY cap exists because a monthly one cannot tell a launch day from an
     * attack: without it, one bad afternoon empties the month.
     */
    check(estimatedInputTokens) {
      roll();
      const projected = estimate(estimatedInputTokens);
      if (state.monthSpent + projected > monthlyCapUsd) return "month";
      if (state.daySpent + projected > dailyCapUsd) return "day";
      return null;
    },

    /** Take the estimate now; returns a handle to settle later. */
    reserve(estimatedInputTokens) {
      roll();
      const amount = estimate(estimatedInputTokens);
      state.monthSpent += amount;
      state.daySpent += amount;
      state.calls += 1;
      persist();
      return { amount };
    },

    /**
     * Replace the reservation with the real cost once the usage metadata arrives.
     *
     * A failed call still costs whatever the provider charged, which for a blocked or
     * empty response is usually the input tokens. Settling with zero on failure would
     * quietly under-count exactly the requests worth watching.
     */
    settle(handle, usage) {
      roll();
      const actual = usage ? cost(usage.inputTokens ?? 0, usage.outputTokens ?? 0) : 0;
      const delta = actual - handle.amount;
      state.monthSpent = Math.max(0, state.monthSpent + delta);
      state.daySpent = Math.max(0, state.daySpent + delta);
      persist();
      return actual;
    },

    /** Give the reservation back when the call never happened. */
    release(handle) {
      roll();
      state.monthSpent = Math.max(0, state.monthSpent - handle.amount);
      state.daySpent = Math.max(0, state.daySpent - handle.amount);
      state.calls = Math.max(0, state.calls - 1);
      persist();
    },

    status() {
      roll();
      return {
        day: state.day,
        daySpentUsd: Number(state.daySpent.toFixed(6)),
        dailyCapUsd,
        month: state.month,
        monthSpentUsd: Number(state.monthSpent.toFixed(6)),
        monthlyCapUsd,
        calls: state.calls,
      };
    },
  };
}
