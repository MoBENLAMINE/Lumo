# How Lumo works

Lumo is one Qt 6 process (`lumo`) plus a tiny C relay (`lumo-hook`) that agents run on each hook event.

```
src/
  main.cpp          wiring: every object below, exposed to QML as a context property
  HookServer        Unix socket server for lumo-hook
  AgentHub          sessions, steps, diffs, pending requests; builds the hook's decision
  HookInstaller     merges lumo-hook into ~/.claude/settings.json (diff, backup, explicit Apply)
  Shell             the Wayland layer-shell surface, input region, tray, shortcuts, preferences
  ChatBackend       chat through `claude -p` (subscription), the Notion connector, or Ollama
  PlanUsage         5-hour / weekly usage from Anthropic with the Claude Code login
  CloudSessions     your claude.ai/code sessions, listed every minute with the same login
  Dictation         push-to-talk: key → microphone → Parakeet → paste
  Speech            sherpa-onnx offline recognizer (Parakeet TDT 0.6B v3, int8)
  Vocabulary        your words and spoken replacements, applied to transcripts
  EvdevKey          Left Ctrl alone, from /dev/input (hot-plug aware)
  Portal            xdg-desktop-portal GlobalShortcuts (fallback key) and RemoteDesktop (paste)
  ActiveWindow      a KWin script reporting the focused app; brings session windows forward
  Music             MPRIS players
  Sounds            synthesized sound effects
qml/
  Island.qml        the island: rest · compact · open · dictation, springs and reactions
  Mascot.qml        the character, drawn with scene-graph items and Animators
  Outfit.qml        its clothes, drawn with Qt Quick Shapes (behind the body and in front of it)
  …                 cards, chat, settings
hook/lumo-hook.c    the relay
```

## The hook

Claude Code runs `lumo-hook <Event>` with the event's JSON on stdin. The relay connects to
`$XDG_RUNTIME_DIR/lumo.sock` (the directory must be private; the server must run as the same user) and sends:

```
{"agent":"","event":"PreToolUse","ppid":123,"pids":[123,120,98],"term":"konsole","len":1834}\n
<len bytes of the original payload>
```

`pids` is the chain of parent processes, used to find the session's terminal or editor window.

Only `PermissionRequest` waits for an answer (110 s at most). The app writes back the exact line Claude Code expects —
`{"hookSpecificOutput":{"hookEventName":"PermissionRequest","decision":{…}}}` — and only after a click. No app, no
answer, or a timeout all mean silence, and Claude Code asks in its terminal as usual. Fire-and-forget events never hold
the agent more than a few milliseconds.

The timeouts on both sides live in `hook/protocol.h`, which the relay and the app both include. The app gives up 5 s
before the hook does, and the hook timeout written into `settings.json` is 10 s longer than the hook's wait. Agents
whose `PermissionRequest` Lumo can't answer (anything but Claude Code and Codex) get a *notice*: their hook leaves at
once, and the card stays up for 8 s or until it is dismissed.

## The island

A wlr-layer-shell surface (LayerShellQt) on the overlay layer, anchored to the top edge. The window is tall and
transparent; its **input region** follows the island's shape, so the rest of the screen stays click-through. The shape
morphs with spring animations between four modes:

| Mode | When |
|---|---|
| `rest` | nothing going on (a pill with Lumo) |
| `compact` | an agent works, finished, or needs your next prompt |
| `open` | hovered, pinned, or a request is waiting |
| `dictation` | while you dictate |

Every looping animation is gated on visibility, so the hidden or resting island renders no frames.

## Dictation

`EvdevKey` reads keyboards in `/dev/input` (needs the `input` group) and reports Left Ctrl held alone: 200 ms with
nothing else pressed starts recording; any other key, mouse button or wheel cancels (that was a shortcut). Without
access, a GlobalShortcuts portal binding is used instead.

Audio comes from the system's default source (`pactl get-default-source`) at 16 kHz mono through Qt Multimedia, while
the speakers are muted. Parakeet loads on a worker thread when you press the key and leaves memory after 5 idle
minutes. The text goes through `Vocabulary`, then onto the clipboard (`wl-copy`) and into the app in front with
Ctrl+V — Ctrl+Shift+V in terminals — sent through the RemoteDesktop portal one key event at a time.

## Cloud sessions

Cloud sessions run on Anthropic's VMs. They never read `~/.claude/settings.json`, so the hook never fires for them and
they can't reach `lumo.sock`. Instead `CloudSessions` asks `api.anthropic.com/v1/code/sessions` once a minute, with the
login Claude Code keeps and the organization from `~/.claude.json`. That is the endpoint the Claude Code CLI itself uses
to list them; it is not a documented public API, so a change on Anthropic's side turns the list off until Lumo follows.

`AgentHub::setCloudSessions` turns each session into an island card (`cloud:<id>`): running or pending is working,
requires action is waiting for you on claude.ai, idle is waiting for your next prompt, failed is an error. A settled
session stays for as long as an idle local one does. Sounds play only for changes Lumo saw happen. A card that you close
stays closed until its status changes. Approvals still happen on claude.ai.

## Chat

`claude -p --output-format stream-json --include-partial-messages` with no built-in tools and no hooks. Any API key in
the environment is removed so the subscription is always what's used. The Notion chat keeps the claude.ai connectors,
waits for the Notion one, and allows only its read tools unless you switch edits on.
