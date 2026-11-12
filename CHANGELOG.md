# Changelog

## Unreleased

- Ideas from Astraeus, written anew for KDE: pages in the island (Music, Today, Controls, System, Tools), a
  fullscreen player with synced lyrics, a calendar, Wi-Fi, Bluetooth and mixer control centers, the shelf, timers,
  battery and notification pills, a configurable resting pill and long pill, themes (light, glass, custom,
  Starfall), an accent from KDE or the wallpaper, reduce motion, a full-screen dashboard, settings export and
  import, and a problem report. Each page works only while it shows.
- Lumo says goodbye when you quit it: the island opens, Lumo waves with "See you soon!", winks, then spins
  away into the light as the island closes, with its own "bye" sound. A click quits at once.
- Music from a player that starts after Lumo (a browser tab, Zen, Firefox…) now shows up and makes Lumo
  dance; before, Lumo only saw players that were already there when it started.
- A new Lumo, a cousin of Claude's Clawd: a blocky body in Claude's terracotta with little arms and four
  stubby legs, a soft aura, and a glowing screen for a face. Each mood changes the light: cyan while working,
  amber when it needs you, arcs when happy, slits when sad, spinning crosses when dizzy. New app icon.
- Eight new outfits replace the old ones: crown, Moroccan tarbouche, lion, engineering student, engineer,
  military dress uniform, headphones and sunglasses. "Auto" puts the headphones on while music plays; a
  saved choice from before (a witch hat…) acts as Auto.
- Lumo says hello when it starts: the island opens in a burst of light, Lumo pops up, smiles and waves
  with its little arm, then the island goes back to what it was. Its own sound follows every step: a rising
  swell, chimes as the light scatters, a "ta-da" when it smiles, a "hi" on each wave, and a falling swell. A request or a dictation comes first; a
  click skips it. Settings → General → "Say hello when Lumo starts" (on by default) also replays it.
- Settings → General → Screen puts the island on the screen of your choice. If that screen is unplugged,
  the primary one takes the island until it's back.
- New expressions, each with its own sound: "> <" when poked (ouch, giggle or poke, never the same twice),
  spinning spirals and "Too many pokes!" in the island when it's dizzy, dozing off with drifting z's after
  ten quiet minutes and a yawn when it wakes, heart eyes and floating hearts when you rest the pointer on it,
  a wink after it celebrates, and a thinking face with three pulsing dots while the chat thinks.
- Dictation, your way: edit your dictionary in Settings; snippets paste a whole text when you say only their
  short phrase; Styles can have Claude clean your words up (Natural or Polished, per app, through the `claude`
  CLI on your plan; As said by default); and Statistics show time saved, days in a row, words a day and the
  apps you dictate in.
- The music card has shuffle, repeat and volume, when the player offers them.
- Lumo dances on the beat: it listens to what your computer plays (in memory, nothing recorded), finds the
  tempo and moves on every beat — a nod, a sway, a side step, a hop, a pumping arm, now and then a twirl —
  changing move every two bars, bigger on the hard beats. Without `parec` or `pw-record` it keeps a steady
  tempo instead.
- Lumo speaks English, French and Arabic: Settings → General → Language (System follows your desktop), switched live.
- Settings can be searched: a field in the sidebar (or Ctrl+F) finds any setting and takes you to it.
- A new README: the launch greeting as an animated hero, Lumo's moods and outfits, and the features grouped
  by what they do for you.
- Sounds work from the start: they follow the system's default output, so a Lumo started at login, before
  the sound server had picked an output, no longer stays silent until you switch outputs.
- The island and Settings match: a depth gradient and a rim lit by the agent's colour on the island, a lit
  stage for Lumo in Settings → About, and one shared palette.

- Cloud sessions: your Claude Code sessions on claude.ai/code appear in the island (working, needs you, done, failed),
  with ↗ to open them in the browser. A switch in Settings → Claude Code turns the list off.
- Allow or deny the top request from anywhere: Meta+Alt+Y allows, Meta+Alt+D denies (a plan is approved, or
  Claude keeps planning). The keys show on the request card and can be changed in System Settings → Shortcuts.

### Fixed
- Connecting to Claude Code no longer rewrites a `settings.json` that isn't valid JSON (which used to drop everything
  but Lumo's hooks); Lumo now asks you to fix the file first. Two applies in the same second each get their own backup.
- Lumo no longer renews your Claude login itself. Renewing rotates the refresh token and could sign Claude Code out;
  plan usage now uses the login only while it is valid, and the status line otherwise.
- Permission notices from agents Lumo can't answer for (Gemini CLI, Cursor…) no longer vanish the moment they appear.
- Updating `lumo-hook` swaps it in atomically, so a hook starting at that moment never finds it missing.
- Muting the speakers for dictation works whatever the desktop's language, and no longer blocks the island.
- Session process chains and old daily word counts no longer pile up.

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
