# The chat proxy. Zero dependencies, so this is a copy and a CMD - no install step,
# no lockfile, nothing to resolve at build time.
FROM node:22-alpine

# Runs as a non-root user: this process holds an API key, and there is no reason for it
# to be able to write anything except the ledger on its mounted volume.
RUN addgroup -S chat && adduser -S chat -G chat

WORKDIR /app
# news-candidates.js is deliberately NOT copied: it is a build-time harvesting tool that
# makes an outbound call, and nothing the server imports touches it. news.js IS needed —
# characters.js imports THE_WORLD_OUTSIDE from it.
COPY --chown=chat:chat package.json server.js budget.js gemini.js characters.js distress.js styles.js council.js events.js facts.js news.js dilemmas.js curiosity.js boasts.js ./

# The volume is mounted here and owned by root by default; the entrypoint needs to be
# able to write the ledger into it. Fly mounts after this, so ownership is fixed at boot
# by the run command rather than here.
USER chat

ENV NODE_ENV=production
EXPOSE 8787
CMD ["node", "server.js"]
