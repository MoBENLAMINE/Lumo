#!/usr/bin/env python3
"""A fake claude.ai session list, to measure how fast Lumo shows a change.

    python3 scripts/fake-cloud-sessions.py [port]

Start Lumo against it (the URL must be local; the token is never sent elsewhere):

    LUMO_CLOUD_SESSIONS_URL=http://127.0.0.1:8765/v1/code/sessions lumo

The fake session moves running → requires_action → running → idle, and each
change is printed with its time. Compare that time with the moment the card
changes in the island.
"""
import json
import sys
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
# (status, seconds it stays)
SCRIPT = [("running", 8), ("requires_action", 12), ("running", 8), ("idle", 30)]
SESSION_ID = "session_fake00000000000000001"

lock = threading.Lock()
state = {"status": SCRIPT[0][0], "since": datetime.now(timezone.utc)}


def stamp(t=None):
    return (t or datetime.now(timezone.utc)).strftime("%H:%M:%S.%f")[:-3]


def iso(t):
    # Same shape as the server: microseconds and a Z.
    return t.strftime("%Y-%m-%dT%H:%M:%S.%fZ")


def run_script():
    index = 0
    while True:
        status, seconds = SCRIPT[index]
        now = datetime.now(timezone.utc)
        with lock:
            state["status"], state["since"] = status, now
        print(f"{stamp(now)}  -> {status}", flush=True)
        time.sleep(seconds)
        index = (index + 1) % len(SCRIPT)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        with lock:
            status, since = state["status"], state["since"]
        print(f"{stamp()}  Lumo asked; answering {status}", flush=True)
        worker = {"running": "running", "requires_action": "requires_action"}.get(status, "idle")
        body = {
            "data": [{
                "id": SESSION_ID,
                "title": "Session de test (faux serveur)",
                "status": "running",
                "worker_status": worker,
                "connection_status": "connected",
                "environment_kind": "anthropic_cloud",
                "updated_at": iso(since),
                "last_event_at": iso(since),
                "created_at": iso(since),
                "sources": [],
            }],
            "next_cursor": None,
            "resume_token": None,
        }
        payload = json.dumps(body).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *args):
        pass  # keep the output to the timeline


if __name__ == "__main__":
    threading.Thread(target=run_script, daemon=True).start()
    print(f"Fake session list on http://127.0.0.1:{PORT}/v1/code/sessions", flush=True)
    HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
