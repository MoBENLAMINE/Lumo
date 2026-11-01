# Changelog

## Unreleased

### Fixed
- Connecting to Claude Code no longer rewrites a `settings.json` that isn't valid JSON (which used to drop everything
  but Lumo's hooks); Lumo now asks you to fix the file first. Two applies in the same second each get their own backup.
- Lumo no longer renews your Claude login itself. Renewing rotates the refresh token and could sign Claude Code out;
  plan usage now uses the login only while it is valid, and the status line otherwise.
- Permission notices from agents Lumo can't answer for (Gemini CLI, Cursor…) no longer vanish the moment they appear.
- Updating `lumo-hook` swaps it in atomically, so a hook starting at that moment never finds it missing.
- Muting the speakers for dictation works whatever the desktop's language, and no longer blocks the island.
- Session process chains and old daily word counts no longer pile up.

### Added
- <kbd>Meta+Alt+Y</kbd> allows and <kbd>Meta+Alt+D</kbd> denies the request at the top of the island, from anywhere
  (approve / keep planning for a plan). Rebind them in System Settings → Shortcuts → Lumo.
- A redesigned Settings window: a sidebar with icons, grouped cards, restyled controls, Notion in the chat options.
- Island polish: one icon set instead of emoji, tooltips on the toolbar, a tab control with counts, request cards
  that show what kind of request they are, choice markers on questions, and keycap hints for the new shortcuts.

### Changed
- The hook and the app share their timeouts and limits through `hook/protocol.h`.
- The sherpa-onnx download is checked against a pinned SHA-256.
- Tests can be turned off with `-DLUMO_BUILD_TESTS=OFF`; new installer tests; the music test now runs (and skips
  without a session bus). CI runs every test, adds an ASan + UBSan job and an advisory `qmllint`.

## 0.1.0 — 2026-11-01

First release.

- The island: rest, compact, open and dictation modes on a Wayland layer-shell surface; 0 % CPU when idle.
- Claude Code sessions: live activity, your prompt, the answer, files changed with diffs, a step timeline, and a
  button that brings the session's window forward.
- Allow / Deny / Always, `AskUserQuestion` answers, and plan approval with feedback.
- Hold Left Ctrl to dictate, offline with Parakeet v3; vocabulary and spoken replacements; speakers muted while
  listening.
- Chat on your Claude subscription (Claude Code CLI) or Ollama; Notion chat through your claude.ai connector.
- Claude plan usage from Anthropic; music from any MPRIS player; ten outfits with seasons; Instinct relay.
