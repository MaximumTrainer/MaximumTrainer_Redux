#!/usr/bin/env python3
"""
Pull skills from the shared catalogue, and see when a local copy has drifted.

This script is vendored into each repository alongside the skill-exchange skill,
so it must stay self-contained: stdlib only, no local clone of the catalogue
repository required.

Usage:
    python3 skills.py list [--search TERM]     what the catalogue offers
    python3 skills.py pull NAME [NAME ...]     vendor a skill into this repo
    python3 skills.py status                   compare local copies to upstream
    python3 skills.py diff NAME                local vs upstream, unified diff
    python3 skills.py contribute NAME          prepare a change to send back

Options:
    --ref REF        catalogue ref to read (default: main)
    --source OWNER/REPO
    --skills-dir DIR  override auto-detection of the skills directory
"""

import argparse
import difflib
import hashlib
import json
import subprocess
import sys
import urllib.error
import urllib.request
from datetime import date
from pathlib import Path

DEFAULT_SOURCE = "MaximumTrainer/agent-skills"
DEFAULT_REF = "main"
MANIFEST_NAME = ".skills-manifest.json"
RAW = "https://raw.githubusercontent.com/{source}/{ref}/{path}"


# --- fetching ----------------------------------------------------------------


def fetch(source, ref, path):
    url = RAW.format(source=source, ref=ref, path=path)
    request = urllib.request.Request(url, headers={"User-Agent": "skill-exchange"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return response.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        raise SystemExit(f"error: {url} -> HTTP {e.code}")
    except urllib.error.URLError as e:
        raise SystemExit(f"error: cannot reach {url}: {e.reason}")


def catalogue(source, ref):
    data = json.loads(fetch(source, ref, "catalogue.json"))
    if data.get("schema") != 1:
        raise SystemExit(f"error: unsupported catalogue schema {data.get('schema')}")
    return data


def entry_for(data, name):
    for skill in data["skills"]:
        if skill["name"] == name or skill["directory"] == name:
            return skill
    known = ", ".join(sorted(s["name"] for s in data["skills"]))
    raise SystemExit(f"error: no skill named '{name}'. Available: {known}")


# --- local layout ------------------------------------------------------------


def repo_root():
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True, encoding="utf-8", errors="replace",
        )
        if out.returncode == 0:
            return Path(out.stdout.strip())
    except OSError:
        pass
    return Path.cwd()


def skills_dir(override=None):
    """Honour whichever convention the repo already uses."""
    if override:
        return Path(override)
    root = repo_root()
    for candidate in (root / ".claude" / "skills", root / ".github" / "skills"):
        if candidate.is_dir():
            return candidate
    return root / ".claude" / "skills"


def load_manifest(directory):
    path = directory / MANIFEST_NAME
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8"))
    return {"source": DEFAULT_SOURCE, "skills": {}}


def save_manifest(directory, manifest):
    directory.mkdir(parents=True, exist_ok=True)
    manifest["skills"] = dict(sorted(manifest["skills"].items()))
    (directory / MANIFEST_NAME).write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def read_local(path):
    if not path.is_file():
        return None
    return path.read_bytes().decode("utf-8").replace("\r\n", "\n")


# --- commands ----------------------------------------------------------------


def cmd_list(args):
    data = catalogue(args.source, args.ref)
    term = (args.search or "").lower()
    rows = [
        s for s in data["skills"]
        if not term or term in s["name"].lower() or term in s["description"].lower()
    ]
    if not rows:
        print(f"nothing in the catalogue matches '{args.search}'")
        return 0
    print(f"{len(rows)} skill(s) in {data['source']}@{args.ref}\n")
    for s in rows:
        first = s["description"].split(". ")[0].rstrip(".")
        print(f"  {s['name']:<30} {s['version']:<8} {first[:96]}")
    print("\npull one with:  python3 skills.py pull <name>")
    return 0


def cmd_pull(args):
    data = catalogue(args.source, args.ref)
    directory = skills_dir(args.skills_dir)
    manifest = load_manifest(directory)

    for name in args.names:
        skill = entry_for(data, name)
        target = directory / skill["directory"]
        existing = manifest["skills"].get(skill["name"])

        if existing and not args.force:
            local_changed = [
                f for f, h in existing.get("files", {}).items()
                if (t := read_local(target / f)) is not None and digest(t) != h
            ]
            if local_changed:
                print(f"! {skill['name']}: locally modified ({', '.join(local_changed)}).")
                print("  Resolve first: 'diff' to see it, 'contribute' to send it back,")
                print("  or 'pull --force' to discard the local change.")
                continue

        hashes = {}
        for relative in skill["files"]:
            text = fetch(args.source, args.ref, f"{skill['directory']}/{relative}")
            out = target / relative
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(text, encoding="utf-8", newline="\n")
            hashes[relative] = digest(text)

        manifest["source"] = args.source
        manifest["skills"][skill["name"]] = {
            "directory": skill["directory"],
            "version": skill["version"],
            "ref": args.ref,
            "files": hashes,
            "vendored": date.today().isoformat(),
        }
        verb = "updated" if existing else "added"
        print(f"{verb} {skill['name']} {skill['version']} -> {target}")

    save_manifest(directory, manifest)
    print(f"\nmanifest: {directory / MANIFEST_NAME}")
    return 0


def classify(local_state, upstream_version, recorded_version):
    if local_state == "missing":
        return "MISSING", "vendored but not on disk; re-pull"
    upstream_newer = upstream_version != recorded_version
    if local_state == "modified" and upstream_newer:
        return "DIVERGED", f"local edits AND upstream is {upstream_version}"
    if local_state == "modified":
        return "MODIFIED", "local edits not in the catalogue"
    if upstream_newer:
        return "OUTDATED", f"upstream is {upstream_version}"
    return "CURRENT", ""


def local_state_of(target, recorded_files):
    seen_any = False
    for relative, recorded_hash in recorded_files.items():
        text = read_local(target / relative)
        if text is None:
            return "missing"
        seen_any = True
        if digest(text) != recorded_hash:
            return "modified"
    return "clean" if seen_any else "missing"


def cmd_status(args):
    directory = skills_dir(args.skills_dir)
    manifest = load_manifest(directory)
    if not manifest["skills"]:
        print(f"no vendored skills recorded in {directory / MANIFEST_NAME}")
        print("pull one with:  python3 skills.py list")
        return 0

    data = catalogue(args.source, args.ref)
    upstream = {s["name"]: s for s in data["skills"]}
    rows, worth_acting = [], False

    for name, record in manifest["skills"].items():
        target = directory / record.get("directory", name)
        if name not in upstream:
            rows.append(("REMOVED", name, record["version"], "no longer in the catalogue"))
            worth_acting = True
            continue
        state = local_state_of(target, record.get("files", {}))
        status, note = classify(state, upstream[name]["version"], record["version"])
        rows.append((status, name, record["version"], note))
        worth_acting = worth_acting or status != "CURRENT"

    width = max(len(r[1]) for r in rows)
    print(f"{len(rows)} vendored skill(s) against {data['source']}@{args.ref}\n")
    for status, name, version, note in sorted(rows):
        print(f"  {status:<9} {name:<{width}}  {version:<8} {note}")

    if worth_acting:
        print("\n  OUTDATED -> pull        MODIFIED -> contribute")
        print("  DIVERGED -> diff, then contribute the local change before pulling")
    return 0


def cmd_diff(args):
    data = catalogue(args.source, args.ref)
    skill = entry_for(data, args.name)
    directory = skills_dir(args.skills_dir)
    target = directory / skill["directory"]
    shown = False

    for relative in skill["files"]:
        upstream_text = fetch(args.source, args.ref, f"{skill['directory']}/{relative}")
        local_text = read_local(target / relative)
        if local_text is None:
            print(f"--- {relative}: not present locally")
            shown = True
            continue
        if local_text == upstream_text:
            continue
        shown = True
        sys.stdout.writelines(
            difflib.unified_diff(
                upstream_text.splitlines(keepends=True),
                local_text.splitlines(keepends=True),
                fromfile=f"upstream/{skill['directory']}/{relative}",
                tofile=f"local/{skill['directory']}/{relative}",
            )
        )
    if not shown:
        print(f"{skill['name']} is identical to {args.source}@{args.ref}")
    return 0


def cmd_contribute(args):
    data = catalogue(args.source, args.ref)
    skill = entry_for(data, args.name)
    directory = skills_dir(args.skills_dir)
    target = directory / skill["directory"]

    patch_lines = []
    for relative in skill["files"]:
        upstream_text = fetch(args.source, args.ref, f"{skill['directory']}/{relative}")
        local_text = read_local(target / relative)
        if local_text is None or local_text == upstream_text:
            continue
        patch_lines.extend(
            difflib.unified_diff(
                upstream_text.splitlines(keepends=True),
                local_text.splitlines(keepends=True),
                fromfile=f"a/{skill['directory']}/{relative}",
                tofile=f"b/{skill['directory']}/{relative}",
            )
        )

    if not patch_lines:
        print(f"{skill['name']} matches upstream - nothing to contribute")
        return 0

    patch = Path(f"{skill['directory']}.patch").resolve()
    patch.write_text("".join(patch_lines), encoding="utf-8", newline="\n")
    added = sum(1 for line in patch_lines if line.startswith("+") and not line.startswith("+++"))
    removed = sum(1 for line in patch_lines if line.startswith("-") and not line.startswith("---"))

    print(f"{skill['name']}: +{added} -{removed} lines vs {args.source}@{args.ref}")
    print(f"patch written to {patch}\n")
    print("Before sending it: is this change general, or specific to this repo?")
    print("Only general improvements belong upstream. See the skill for the test.\n")
    print(f"  git clone https://github.com/{args.source} /tmp/agent-skills")
    print("  cd /tmp/agent-skills")
    print(f"  git checkout -b improve/{skill['directory']}")
    print(f"  git apply {patch}")
    print(f"  # bump metadata.version in {skill['directory']}/SKILL.md")
    print("  python3 tools/build_catalogue.py")
    print(f"  git commit -am 'feat({skill['directory']}): <what it now covers>'")
    print("  gh pr create --fill")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    parser.add_argument("--ref", default=DEFAULT_REF)
    parser.add_argument("--skills-dir", default=None)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("list", help="what the catalogue offers")
    p.add_argument("--search", default=None)
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("pull", help="vendor a skill into this repo")
    p.add_argument("names", nargs="+")
    p.add_argument("--force", action="store_true", help="overwrite local modifications")
    p.set_defaults(func=cmd_pull)

    p = sub.add_parser("status", help="compare local copies to upstream")
    p.set_defaults(func=cmd_status)

    p = sub.add_parser("diff", help="local vs upstream")
    p.add_argument("name")
    p.set_defaults(func=cmd_diff)

    p = sub.add_parser("contribute", help="prepare a change to send back")
    p.add_argument("name")
    p.set_defaults(func=cmd_contribute)

    # Skill text is full of em-dashes and curly quotes; on a Windows console the
    # default codec is cp1252 and `diff` output comes out mangled.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
