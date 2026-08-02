#!/usr/bin/env python3
"""Smoke-test the desktop Rules & Skills cloud contract against a local strata-be."""

from __future__ import annotations

import argparse
import json
import time
from urllib.error import HTTPError
from urllib.request import Request, urlopen


def request(base: str, path: str, method: str = "GET", token: str = "", body: dict | None = None) -> dict:
    payload = json.dumps(body).encode() if body is not None else None
    headers = {"content-type": "application/json"}
    if token:
        headers["authorization"] = f"Bearer {token}"
    try:
        with urlopen(Request(f"{base}{path}", data=payload, headers=headers, method=method), timeout=10) as response:
            return json.load(response)
    except HTTPError as error:
        raise RuntimeError(f"{method} {path} failed ({error.code}): {error.read().decode()}") from error


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--api-url", default="http://localhost:3001")
    args = parser.parse_args()
    stamp = time.time_ns()
    tokens = request(args.api_url, "/v1/auth/register", "POST", body={"email": f"desktop-smoke-{stamp}@example.test", "password": "smoke-test-password"})
    token = tokens["accessToken"]
    workspace = request(args.api_url, "/v1/workspaces", "POST", token, {"fingerprint": f"desktop-smoke-{stamp}", "name": "Desktop smoke"})
    workspace_id = workspace["id"]
    markdown = "---\nname: Smoke skill\ndescription: Desktop round-trip\nsource: {\"kind\":\"github\"}\n---\nBody\n"
    created = request(args.api_url, f"/v1/workspaces/{workspace_id}/skills", "POST", token, {
        "slug": "smoke-skill", "name": "Smoke skill", "description": "Desktop round-trip", "enabled": True, "content": markdown,
    })
    updated = request(args.api_url, f"/v1/workspaces/{workspace_id}/skills/{created['id']}", "PATCH", token, {
        "name": "Smoke skill", "description": "Desktop round-trip updated", "enabled": True, "content": markdown.replace("Body", "Updated body"),
    })
    assert updated["slug"] == "smoke-skill"
    items = request(args.api_url, f"/v1/workspaces/{workspace_id}/skills", token=token)["items"]
    assert len(items) == 1 and items[0]["id"] == created["id"] and "Updated body" in items[0]["content"]
    print("desktop Rules & Skills cloud smoke passed: POST -> fetch/match -> PATCH without slug")


if __name__ == "__main__":
    main()
