#!/usr/bin/env python3
"""Minimal Unreal MCP client over Streamable HTTP (stdlib only).

The UE 5.8 ModelContextProtocol plugin embeds an MCP server in the running editor at
http://127.0.0.1:8000/mcp. Spawned engineer shells start in the v8 agent home, not the
project dir, so this is the project-local client that proves reachability and drives the
editor's toolset tools (list_toolsets / describe_toolset / call_tool).

Transport notes (UE 5.8 ModelContextProtocol, learned live in S1-B):
- JSON-RPC 2.0 POSTed to /mcp. The server binds session state to a SINGLE persistent
  keep-alive TCP connection: a fresh connection per request loses the `initialized` state and
  `tools/call` then returns 200 with an empty body. So this client holds ONE http.client
  connection for the whole session (init -> initialized -> calls).
- Accept must allow application/json AND text/event-stream. `initialize` answers as plain JSON;
  `tools/call` answers as a single SSE frame (`event: message\r\ndata: {json}`).
- The initialize response carries an `Mcp-Session-Id` header, echoed on every later request.
  The GET SSE stream is NOT supported (405) — POST-only.

Usage:
    python tools/mcp_client.py ping                             # initialize + list tool names
    python tools/mcp_client.py list                             # tools/list (full)
    python tools/mcp_client.py call <tool> '<json-args>'        # top-level tool
    python tools/mcp_client.py tool <toolset> <tool> '<json>'   # call_tool into a toolset
"""
from __future__ import annotations

import http.client
import json
import sys
from urllib.parse import urlparse

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL_VERSION = "2025-06-18"


class MCP:
    def __init__(self, url: str = URL):
        p = urlparse(url)
        self.path = p.path or "/mcp"
        self.session_id: str | None = None
        self._id = 0
        try:
            self._conn = http.client.HTTPConnection(p.hostname, p.port or 80, timeout=120)
        except Exception as e:  # pragma: no cover
            raise SystemExit(f"cannot open connection to {url}: {e}")

    def _next_id(self) -> int:
        self._id += 1
        return self._id

    def _post(self, payload: dict, notify: bool = False):
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "MCP-Protocol-Version": PROTOCOL_VERSION,
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        try:
            self._conn.request("POST", self.path, body=json.dumps(payload), headers=headers)
            resp = self._conn.getresponse()
            sid = resp.getheader("Mcp-Session-Id")
            if sid:
                self.session_id = sid
            body = resp.read().decode("utf-8", "replace")
            ctype = resp.getheader("Content-Type", "") or ""
        except (http.client.HTTPException, OSError) as e:
            raise SystemExit(f"cannot reach {URL}: {e} "
                             f"(is the UE editor open with MCP autostarted?)")
        if notify:
            return None
        return _parse(body, ctype)

    def initialize(self) -> dict:
        res = self._post({
            "jsonrpc": "2.0", "id": self._next_id(), "method": "initialize",
            "params": {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "spacetravel-mcp-client", "version": "0.1"},
            },
        })
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"}, notify=True)
        return res

    def tools_list(self) -> dict:
        return self._post({"jsonrpc": "2.0", "id": self._next_id(), "method": "tools/list"})

    def tools_call(self, name: str, arguments: dict) -> dict:
        return self._post({
            "jsonrpc": "2.0", "id": self._next_id(), "method": "tools/call",
            "params": {"name": name, "arguments": arguments},
        })

    def call_toolset(self, toolset: str, tool: str, arguments: dict) -> dict:
        """Invoke an editor toolset tool through the top-level `call_tool` dispatcher."""
        return self.tools_call("call_tool", {
            "toolset_name": toolset, "tool_name": tool, "arguments": arguments,
        })


def _parse(body: str, ctype: str) -> dict:
    """Return the JSON-RPC message from a plain-JSON or SSE body."""
    body = body.strip()
    if "text/event-stream" in ctype or body.startswith("event:") or body.startswith("data:"):
        for line in body.splitlines():
            line = line.strip()
            if line.startswith("data:"):
                return json.loads(line[len("data:"):].strip())
        raise SystemExit(f"no data frame in SSE body: {body[:300]}")
    return json.loads(body)


def _result(msg: dict):
    if isinstance(msg, dict) and "error" in msg:
        raise SystemExit(f"MCP error: {json.dumps(msg['error'])}")
    return msg.get("result", msg) if isinstance(msg, dict) else msg


def main(argv: list[str]) -> int:
    cmd = argv[1] if len(argv) > 1 else "ping"
    m = MCP()
    init = _result(m.initialize())
    if cmd == "ping":
        tl = _result(m.tools_list())
        names = [t.get("name") for t in tl.get("tools", [])]
        print(json.dumps({
            "reachable": True,
            "url": URL,
            "session_id": m.session_id,
            "serverInfo": init.get("serverInfo"),
            "tool_count": len(names),
            "tools": names,
        }, indent=2))
    elif cmd == "list":
        print(json.dumps(_result(m.tools_list()), indent=2))
    elif cmd == "call":
        name = argv[2]
        args = json.loads(argv[3]) if len(argv) > 3 else {}
        print(json.dumps(_result(m.tools_call(name, args)), indent=2))
    elif cmd == "tool":
        toolset, tool = argv[2], argv[3]
        args = json.loads(argv[4]) if len(argv) > 4 else {}
        print(json.dumps(_result(m.call_toolset(toolset, tool, args)), indent=2))
    elif cmd == "describe":
        print(json.dumps(_result(m.tools_call("describe_toolset",
              {"toolset_name": argv[2]})), indent=2))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
