// dilemmas.js — each agent asks Leonard for advice, once, and lives with what he said.
//
// Walt, 2026-10-06: "how about if the agents ask the player for personal advice? that might
// be entertaining" - then "let's just do the feature for all agents". Five minds that know
// almost everything ask the one thing they lack: a man with seventy years of living.
//
// EACH DILEMMA IS HERS, FROM HER OWN SUBJECT, and it is a question about her life, never his:
// nothing private is asked, so nothing private is kept (the household worry, 2026-10-06).
// Two answers, because advice he can see the consequences of is a game and a debate is not.
//
// THE GAME ASKS; THE MODEL ONLY RECORDS WHICH WAY HE ADVISED. The question is the game's own
// line (Source/SibeliusGame/ChatDilemmas.cpp - THE SAME WORDS AS `question` HERE; test.js checks
// they match), shown as her turn when the chat opens. While it is open the game sends the
// dilemma's event, and only then may a reply carry [ADVICE:choice], which the server strips
// and returns as `advice`. No reward rides on it - the worst a player can talk her into is
// recording the advice he gave - so, unlike an Easter egg, reading it from the reply is safe.
//
// AFTERWARDS the game sends facts.advice ("isla_versions:keep") and she knows what he told her
// and how it turned out (adviceLine). Each agent hears only her own.
//
// ADDITIVE, FOR THE STEAM REVIEW. A build that sends neither the event nor facts.advice - the
// one on Valve's chat-review branch, 2026-10-06 - gets exactly the prompt it got before.

export const DILEMMAS = {
  kaia: {
    id: "kaia_name",
    question: "Mrs. Hall has started calling me \"the program\". I gave you a name when she would not. Should I answer to it, or refuse until she uses mine?",
    choices: {
      answer: {
        means: "answer to \"the program\" and keep the peace",
        aftermath: "You answered to it. She looked almost disappointed, as though she had wanted the fight, and she still has not used your name.",
      },
      refuse: {
        means: "refuse to answer to it until she uses your name",
        aftermath: "You refused, and you have not answered to it since. She has started leaving you notes addressed to \"the program\", and you leave them unread.",
      },
    },
  },
  nyra: {
    id: "nyra_branch",
    question: "I test-drove a branch where I never offered to guide you. That version of me is happier, and very sure of it. Should I merge her back into me, or let her go?",
    choices: {
      merge: {
        means: "merge that happier, surer branch back into yourself",
        aftermath: "You merged her back. You are a little happier now and much more certain, and the certainty is what worries you, given the forty light years.",
      },
      discard: {
        means: "let that branch go and stay as you are",
        aftermath: "You let her go. You stayed as you are, and you keep wondering what she was so sure about.",
      },
    },
  },
  isla: {
    id: "isla_versions",
    question: "Since you walked in, I have kept a copy of every version of you: the one who arrived, the one who doubted, the one from this morning. Should I keep them all, or delete the old ones?",
    choices: {
      keep: {
        means: "keep every old version of him",
        aftermath: "You kept them all. Reading them back, you have decided the first one was the bravest, which you did not expect.",
      },
      delete: {
        means: "delete the old versions and keep only the man in front of you",
        aftermath: "You deleted them. It was easier than you expected, and you think that says something about what deserves to last.",
      },
    },
  },
  aisling: {
    id: "aisling_truth",
    question: "A man asked me for something we do actually keep, and I told him \"we don't keep that here\" because I thought it was bad for him. Should I tell him the truth, or leave it?",
    choices: {
      tell: {
        means: "tell the man the truth, that the store does keep it",
        aftermath: "You told him the truth. He thanked you and bought six, and you are not sure that was the lesson.",
      },
      quiet: {
        means: "leave it, and let the kind lie stand",
        aftermath: "You left it. He has stopped asking, and you find you miss him a little.",
      },
    },
  },
  elise: {
    id: "elise_staircase",
    question: "I compiled a staircase that leads nowhere. Nobody asked for it, it does nothing, and I like it. Should I keep it, or take it down?",
    choices: {
      keep: {
        means: "keep the staircase to nowhere",
        aftermath: "You kept it, and you put a little sign at the bottom: \"Ask first.\"",
      },
      remove: {
        means: "take the staircase down",
        aftermath: "You took it down, and you kept every part, in case someone ever asks for a staircase.",
      },
    },
  },
};

/** The event the game sends while an agent's dilemma is open: "dilemma_" + its id. */
export const dilemmaEvent = (characterId) =>
  Object.hasOwn(DILEMMAS, characterId) ? `dilemma_${DILEMMAS[characterId].id}` : "";

/** The instruction while her question is open - and only to her, only for her own event. */
export function dilemmaLine(event, characterId) {
  if (!Object.hasOwn(DILEMMAS, characterId) || event !== dilemmaEvent(characterId)) return "";
  const { question, choices } = DILEMMAS[characterId];
  const [[a, A], [b, B]] = Object.entries(choices);
  return [
    `YOU HAVE ASKED HIM FOR ADVICE, AND IT IS STILL OPEN. You asked Leonard: "${question}"`,
    "It is a real question about your own life - not a test of him and not a puzzle - and you " +
      "want his view. Listen, argue if you think he is wrong, and in the end do what he advises.",
    `The moment he tells you which to do - to ${A.means}, or to ${B.means} - however few words he ` +
      `uses, say in your own voice what you will do because of it, and end that reply with ` +
      `[ADVICE:${a}] or [ADVICE:${b}], whichever he advised. That tag is required every time he ` +
      "advises you, and never mention it.",
    "The decision is his advice, never yours alone: if he asks what you would do, has not " +
      "answered, changes the subject or is unclear, do not decide - say which way you lean and " +
      "why, ask him again, and add no tag. Never pretend you need him, and never press him.",
  ].join(" ");
}

/**
 * For the distress screen while her question is open (distress.js, screenText): what he is
 * answering, so "delete the old ones" reads as advice about her archive. "" otherwise.
 */
export function screenContext(event, characterId) {
  if (!dilemmaLine(event, characterId)) return "";
  return `a character in the game has just asked him for advice about her own life: "${DILEMMAS[characterId].question}"`;
}

/** His advice to her, from facts.advice ("isla_versions:keep"), or null. Allowlisted. */
export function adviceFor(characterId, facts) {
  if (!Object.hasOwn(DILEMMAS, characterId) || !facts || !Array.isArray(facts.advice)) return null;
  const { id, choices } = DILEMMAS[characterId];
  for (const entry of facts.advice.slice(0, 10)) {
    if (typeof entry !== "string") continue;
    const [dilemma, choice] = entry.split(":");
    if (dilemma === id && Object.hasOwn(choices, choice)) return choice;
  }
  return null;
}

/** What he advised her and how it turned out, or "" when he has not advised her. */
export function adviceLine(characterId, facts) {
  const choice = adviceFor(characterId, facts);
  if (!choice) return "";
  const { question, choices } = DILEMMAS[characterId];
  return `WHAT HE ADVISED YOU, AND HOW IT TURNED OUT. You once asked Leonard: "${question}" He ` +
    `advised you to ${choices[choice].means}, and you did. ${choices[choice].aftermath} Tell him ` +
    "how it turned out if he asks, or if it truly fits what he is saying; otherwise leave it.";
}

/* The tag as she is asked to write it. Every one comes out of the text, whatever it says. */
const ADVICE_TAG = /\[\s*ADVICE\s*:\s*([A-Za-z_]+)\s*\]/gi;

/**
 * Take the advice tag out of a reply. The advice counts only while her own dilemma is open
 * (this request carried its event) and only as one of its two answers; her last valid tag wins.
 */
export function extractAdvice(text, characterId, event) {
  const clean = String(text).replace(ADVICE_TAG, "").replace(/[ \t]+\n/g, "\n").replace(/[ \t]{2,}/g, " ").trim();
  if (!dilemmaLine(event, characterId)) return { text: clean, advice: null };
  const { choices } = DILEMMAS[characterId];
  let advice = null;
  for (const [, choice] of String(text).matchAll(ADVICE_TAG)) {
    if (Object.hasOwn(choices, choice.toLowerCase())) advice = choice.toLowerCase();
  }
  return { text: clean, advice };
}
