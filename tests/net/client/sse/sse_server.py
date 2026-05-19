#!/usr/bin/env python3
"""Small standard-library HTTP server for testing Server-Sent Events."""

from __future__ import annotations

import argparse
import json
import queue
import socketserver
import sys
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8080
HEARTBEAT_SECONDS = 15


class Client:
    def __init__(self) -> None:
        self.messages: queue.Queue[dict[str, object] | None] = queue.Queue()

    def send(self, event: dict[str, object]) -> None:
        self.messages.put(event)

    def close(self) -> None:
        self.messages.put(None)


class EventHub:
    def __init__(self) -> None:
        self._clients: set[Client] = set()
        self._lock = threading.Lock()
        self._next_id = 1

    def subscribe(self) -> Client:
        client = Client()
        with self._lock:
            self._clients.add(client)
        return client

    def unsubscribe(self, client: Client) -> None:
        with self._lock:
            self._clients.discard(client)
        client.close()

    def publish(self, event_type: str, data: object) -> dict[str, object]:
        with self._lock:
            event = {
                "id": self._next_id,
                "event": event_type,
                "data": data,
                "time": time.time(),
            }
            self._next_id += 1
            clients = tuple(self._clients)

        for client in clients:
            client.send(event)
        return event

    def client_count(self) -> int:
        with self._lock:
            return len(self._clients)


HUB = EventHub()


def format_sse(event: dict[str, object]) -> bytes:
    event_id = event.get("id")
    event_type = event.get("event", "message")
    data = event.get("data", "")

    if not isinstance(data, str):
        data = json.dumps(data, ensure_ascii=False, separators=(",", ":"))

    lines = []
    if event_id is not None:
        lines.append(f"id: {event_id}")
    if event_type:
        lines.append(f"event: {event_type}")
    for line in str(data).splitlines() or [""]:
        lines.append(f"data: {line}")
    lines.append("")
    lines.append("")
    return "\n".join(lines).encode("utf-8")


class SSEHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "SSETestHTTP/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        sys.stderr.write(
            "%s - - [%s] %s\n"
            % (self.client_address[0], self.log_date_time_string(), fmt % args)
        )

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/":
            self._write_html()
        elif parsed.path == "/events":
            self._stream_events()
        elif parsed.path == "/send":
            self._handle_send(parsed.query)
        elif parsed.path == "/health":
            self._write_json({"ok": True, "clients": HUB.client_count()})
        else:
            self.send_error(HTTPStatus.NOT_FOUND, "not found")

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path != "/send":
            self.send_error(HTTPStatus.NOT_FOUND, "not found")
            return

        content_length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(content_length) if content_length else b""
        content_type = self.headers.get("Content-Type", "")

        if "application/json" in content_type:
            try:
                payload = json.loads(raw_body.decode("utf-8") or "{}")
            except json.JSONDecodeError as exc:
                self.send_error(HTTPStatus.BAD_REQUEST, f"bad json: {exc}")
                return
            event_type = str(payload.get("event", "message"))
            data = payload.get("data", payload)
        else:
            payload = parse_qs(raw_body.decode("utf-8"))
            event_type = payload.get("event", ["message"])[0]
            data = payload.get("data", [raw_body.decode("utf-8")])[0]

        event = HUB.publish(event_type, data)
        self._write_json({"published": event, "clients": HUB.client_count()})

    def _stream_events(self) -> None:
        client = HUB.subscribe()
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("X-Accel-Buffering", "no")
        self.end_headers()

        try:
            self.wfile.write(b": connected\n\n")
            self.wfile.flush()
            while True:
                try:
                    event = client.messages.get(timeout=HEARTBEAT_SECONDS)
                except queue.Empty:
                    self.wfile.write(b": heartbeat\n\n")
                    self.wfile.flush()
                    continue

                if event is None:
                    break
                self.wfile.write(format_sse(event))
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            HUB.unsubscribe(client)

    def _handle_send(self, query: str) -> None:
        params = parse_qs(query)
        event_type = params.get("event", ["message"])[0]
        data = params.get("data", [f"server time {time.strftime('%H:%M:%S')}"])[0]
        event = HUB.publish(event_type, data)
        self._write_json({"published": event, "clients": HUB.client_count()})

    def _write_json(self, payload: object, status: HTTPStatus = HTTPStatus.OK) -> None:
        body = json.dumps(payload, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _write_html(self) -> None:
        body = INDEX_HTML.encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


INDEX_HTML = """<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SSE Test Server</title>
  <style>
    body {
      max-width: 760px;
      margin: 40px auto;
      padding: 0 20px;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      line-height: 1.5;
    }
    form {
      display: flex;
      gap: 8px;
      margin: 18px 0;
    }
    input, button {
      font: inherit;
      padding: 8px 10px;
    }
    input {
      flex: 1;
    }
    pre {
      min-height: 260px;
      padding: 12px;
      overflow: auto;
      background: #111827;
      color: #e5e7eb;
    }
  </style>
</head>
<body>
  <h1>SSE Test Server</h1>
  <form id="send-form">
    <input id="message" value="hello from browser" autocomplete="off">
    <button type="submit">Send</button>
  </form>
  <pre id="log"></pre>
  <script>
    const log = document.querySelector("#log");
    const source = new EventSource("/events");

    function append(line) {
      log.textContent += line + "\\n";
      log.scrollTop = log.scrollHeight;
    }

    source.addEventListener("open", () => append("[open] connected"));
    source.addEventListener("error", () => append("[error] reconnecting"));
    source.addEventListener("message", event => {
      append(`[message #${event.lastEventId}] ${event.data}`);
    });

    document.querySelector("#send-form").addEventListener("submit", async event => {
      event.preventDefault();
      const data = document.querySelector("#message").value;
      await fetch("/send", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({event: "message", data})
      });
    });
  </script>
</body>
</html>
"""


class ReuseAddressThreadingHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run a small SSE test server.")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"default: {DEFAULT_HOST}")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"default: {DEFAULT_PORT}")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    socketserver.TCPServer.allow_reuse_address = True
    server = ReuseAddressThreadingHTTPServer((args.host, args.port), SSEHandler)
    print(f"SSE test server listening on http://{args.host}:{args.port}", flush=True)
    print("Endpoints: /events, /send?data=hello, /health", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nshutting down", flush=True)
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
