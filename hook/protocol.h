/*
 * What lumo-hook (C) and the app (C++) must agree on. Change it here only.
 */
#pragma once

/* How long the hook waits for a click on a permission request. */
#define LUMO_DECISION_MS 110000
/* The app gives up a little earlier, so the hook always hears back first. */
#define LUMO_APP_DECISION_MS (LUMO_DECISION_MS - 5000)
/* The timeout written into ~/.claude/settings.json for that hook, in seconds:
 * Claude Code must never kill the hook while it is still waiting. */
#define LUMO_PERMISSION_HOOK_TIMEOUT_S (LUMO_DECISION_MS / 1000 + 10)
/* Largest payload either side accepts. */
#define LUMO_MAX_PAYLOAD (8 * 1024 * 1024)
