#!/usr/bin/env python3
"""Validate canonical gallery Markdown and build its deterministic manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

import yaml


REQUIRED = {
    "name",
    "description",
    "category",
    "tags",
    "source",
    "license",
    "attribution",
    "trustLevel",
    "enabled",
}
SOURCE_REQUIRED = {"kind", "url", "title"}
VALID_SOURCE_KINDS = {"paper", "blog", "github", "dataset"}
VALID_TRUST = {"CURATED", "VERIFIED"}


def parse_document(path: Path) -> tuple[dict, str]:
    content = path.read_text(encoding="utf-8")
    lines = content.replace("\r\n", "\n").split("\n")
    if not lines or lines[0].strip() != "---":
        raise ValueError(f"{path}: YAML frontmatter is required")
    try:
        end = next(index for index, line in enumerate(lines[1:], 1) if line.strip() == "---")
    except StopIteration as error:
        raise ValueError(f"{path}: frontmatter is not closed") from error
    metadata = yaml.safe_load("\n".join(lines[1:end]))
    if not isinstance(metadata, dict):
        raise ValueError(f"{path}: frontmatter must be a mapping")
    if not "\n".join(lines[end + 1 :]).strip():
        raise ValueError(f"{path}: Markdown body is required")
    return metadata, content


def validate(path: Path, kind: str, metadata: dict) -> None:
    missing = REQUIRED - metadata.keys()
    if missing:
        raise ValueError(f"{path}: missing {', '.join(sorted(missing))}")
    if kind == "rule" and not {"globs", "alwaysApply"} <= metadata.keys():
        raise ValueError(f"{path}: public rules require globs and alwaysApply")
    if not isinstance(metadata["tags"], list) or not all(isinstance(tag, str) and tag for tag in metadata["tags"]):
        raise ValueError(f"{path}: tags must be a non-empty string list")
    source = metadata["source"]
    if not isinstance(source, dict) or not SOURCE_REQUIRED <= source.keys():
        raise ValueError(f"{path}: source must contain kind, url and title")
    if source["kind"] not in VALID_SOURCE_KINDS or not str(source["url"]).startswith("https://"):
        raise ValueError(f"{path}: source kind or HTTPS URL is invalid")
    if metadata["license"] != "CC-BY-4.0":
        raise ValueError(f"{path}: license is not in the v0 allowlist")
    if metadata["trustLevel"] not in VALID_TRUST:
        raise ValueError(f"{path}: invalid trustLevel")


def item_for(root: Path, path: Path, kind: str) -> dict:
    metadata, content = parse_document(path)
    validate(path, kind, metadata)
    slug = path.parent.name if kind == "skill" else path.stem
    item = {
        "type": kind,
        "slug": slug,
        "name": metadata["name"],
        "description": metadata["description"],
        "category": metadata["category"],
        "tags": metadata["tags"],
        "source": metadata["source"],
        "license": metadata["license"],
        "attribution": metadata["attribution"],
        "trustLevel": metadata["trustLevel"],
        "enabled": metadata["enabled"],
        "path": path.relative_to(root).as_posix(),
        "sha256": hashlib.sha256(content.encode("utf-8")).hexdigest(),
    }
    if kind == "rule":
        item.update({"globs": metadata["globs"], "alwaysApply": metadata["alwaysApply"]})
    return item


def build(root: Path) -> bytes:
    paths = [(path, "skill") for path in root.glob("skills/*/SKILL.md")]
    paths += [(path, "rule") for path in root.glob("rules/*.md")]
    items = [item_for(root, path, kind) for path, kind in paths]
    items.sort(key=lambda item: (item["type"], item["slug"]))
    return (json.dumps({"version": 1, "items": items}, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1] / "docs" / "gallery")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = build(args.root)
    manifest = args.root / "manifest.json"
    if args.check:
        if not manifest.exists() or manifest.read_bytes() != expected:
            print(f"{manifest} is stale; run scripts/build_skill_gallery.py", file=sys.stderr)
            return 1
        print(f"gallery manifest valid: {manifest}")
        return 0
    manifest.write_bytes(expected)
    print(f"wrote {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
