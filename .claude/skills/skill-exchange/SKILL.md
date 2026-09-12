---
name: skill-exchange
description: Check the shared skills catalogue before writing any skill, workflow or procedure from scratch, vendor in whatever already solves the problem, and send genuinely general improvements back. Use when about to write a new skill or a repeated procedure; when asked "is there a skill for X", "do we have something for this", "pull the shared skills", "update our skills", or "contribute this back"; when a vendored skill has been edited locally; and when setting up skills in a repository for the first time. The catalogue lives at MaximumTrainer/agent-skills.
license: MIT
metadata:
  version: "1.0.2"
---

# Skill exchange

There is a shared catalogue of skills at **`MaximumTrainer/agent-skills`**. It exists because the same practices kept being reinvented independently across repositories — outside-in TDD in nine of them, ports-and-adapters in eight, the same third-party API quirks handled in six — with each copy learning the same lessons separately and none of them sharing what it learned.

This skill is the loop that stops that happening again. It runs in both directions:

```
   catalogue ──── pull ────► this repo        reuse what exists
   catalogue ◄── contribute ─ this repo        send back what generalises
```

The rule:

> **Before writing a skill, a workflow document, or a procedure someone will follow twice — look in the catalogue first. After improving a vendored skill — decide whether the improvement is general, and if it is, send it back.**

## Before you write anything

When you are about to write a skill, a `CONTRIBUTING` section, a runbook, or an `AGENTS.md` block describing how something is done here, check first. It takes one command:

```bash
python3 .claude/skills/skill-exchange/scripts/skills.py list
python3 .claude/skills/skill-exchange/scripts/skills.py list --search testing
```

The catalogue covers, broadly:

| Area | Examples |
|---|---|
| Core practice | outside-in TDD, ports and adapters, shipping and verifying, CI triage, constant drift, deliberate decisions |
| Specification | issue specification by example, gap issues, documentation drift, white papers |
| Testing integrity | test-theatre audits, container integration tests, API quirk fixtures, live probes, synthetic data |
| Platform | Docker images, Connect IQ/Monkey C, Qt6, Three.js/R3F, screenshots |
| Integrations | Intervals.icu, MCP server tools |

Search by the *problem*, not by the name you had in mind. "flaky" finds the CI triage skill; "fixture" finds three different ones.

## The decision

Having found a candidate, choose deliberately. Getting this wrong in either direction is costly: vendoring something that does not fit produces a skill nobody follows, and writing a local one that duplicates the catalogue splits the knowledge again.

| Situation | Do |
|---|---|
| It solves the problem as written | **Pull it.** Do not copy-paste, do not rewrite it in your own words |
| It solves the problem but needs repo-specific detail | **Pull it, then add a thin local skill** that names this repo's paths, commands and conventions and defers to the shared one for the method |
| It is close but wrong in a way that would be wrong everywhere | **Pull it, fix it, contribute the fix back** |
| It is genuinely specific to this repo | **Write a local skill.** Not everything generalises, and forcing it into the catalogue makes the catalogue worse |
| Nothing matches, and the problem recurs across repos | **Write it here, then contribute it** so the next repo does not repeat the work |

Prefer the thin-local-skill option over forking a shared skill. A local `ship.md` that says "the gate here is `./gradlew check` and `npm run verify`; follow `verify-and-ship` for the method" stays in sync for free. A forked copy with the commands spliced in drifts the moment upstream improves.

## Pulling

```bash
python3 .claude/skills/skill-exchange/scripts/skills.py pull outside-in-tdd hexagonal-architecture
```

This copies `SKILL.md` and any `references/`, `scripts/` and `assets/` into the repo's skills directory — honouring whichever convention the repo already uses (`.claude/skills/` or `.github/skills/`) — and records what was taken in `.skills-manifest.json`: the version, the ref, and a content hash per file.

**Commit the manifest.** It is what makes `status` able to tell "someone improved this locally" apart from "upstream moved on", and without it a later sync silently discards local work.

A vendored skill is a **copy, not a link**. It keeps working with no network, and it will not change under you mid-task. The cost is that it goes stale, which is what `status` is for.

## Staying in sync

```bash
python3 .claude/skills/skill-exchange/scripts/skills.py status
```

```
  CURRENT    outside-in-tdd          1.0.0
  OUTDATED   verify-and-ship         1.0.0    upstream is 1.2.0
  MODIFIED   intervals-icu-api       1.0.0    local edits not in the catalogue
  DIVERGED   test-theatre-audit      1.0.0    local edits AND upstream is 1.1.0
```

- **OUTDATED** — `pull` it. Read the diff first if the skill encodes anything you rely on.
- **MODIFIED** — someone improved it here. Decide whether it generalises, below.
- **DIVERGED** — **contribute the local change first, then pull.** Pulling first silently destroys it. This is the one state where order matters.
- **REMOVED** — the skill is gone from the catalogue. Find out why before deleting; it may have been merged into another skill.

Run `status` when starting work in a repo you have not touched recently, and when a shared practice feels out of date.

## Contributing back

This is the half that usually does not happen, and the reason catalogues rot. When a vendored skill has been improved locally, the improvement is already written — the only work left is deciding where it belongs and spending five minutes on a pull request.

### Is the change general or repo-specific?

Apply this test honestly. **Would this change help a repository that does not share this one's stack, paths or tooling?**

**General — contribute it:**

- A failure mode and its cause, in any repo that uses this technology
- A sharper explanation, a better worked example, a clearer decision table
- A missing case in a checklist
- A correction — the skill said something that is not true
- A trigger phrase that should have matched and did not

**Repo-specific — keep it local:**

- This repo's file paths, module names, build commands, branch names
- A threshold or convention chosen for this project
- Anything naming a service, account or dataset only this repo talks to
- A workaround for a version only this repo is pinned to

The honest split is usually "both": a general insight wrapped in local specifics. Extract the general part, phrase it without the local nouns, and contribute that — keeping the local detail in a thin local skill. A finding of the shape *"the vendor's `/wellness` endpoint returns oldest-first while `/activities` returns newest-first"* is general. *"so `DashboardSync.kt:88` reads a stale value"* is not.

### Sending it

```bash
python3 .claude/skills/skill-exchange/scripts/skills.py diff intervals-icu-api
python3 .claude/skills/skill-exchange/scripts/skills.py contribute intervals-icu-api
```

`contribute` writes a patch and prints the exact sequence — clone, branch, apply, bump the version, rebuild the catalogue, PR. Before running it:

- **Strip the local specifics** from the patch rather than sending them upstream.
- **Say what broke.** A rule with a named failure mode survives; a rule without one gets "simplified" away by the next reader. The catalogue's own convention is that where a rule exists because something broke, the skill says what broke.
- **Bump the version deliberately** — patch for a clarification, minor for new material, major for a restructure that changes the method.
- **Run `python3 tools/build_catalogue.py`** in the catalogue repo; `catalogue.json` is generated and CI fails on drift.
- Open the PR against `main` and say which repo the improvement came from.

After it merges, `pull` it back here so the manifest hash matches again and `status` returns to CURRENT.

### The catalogue is served from a cache

`raw.githubusercontent.com` responds with `Cache-Control: max-age=300`, so for up
to five minutes after a merge the catalogue still serves the previous content.
`list` will not show a newly added skill, and `status` will still say CURRENT
against the old version.

That is the cache, not a failed contribution. Wait and re-run rather than
re-pushing. To confirm what actually landed, look at the repository rather than
the raw URL:

```bash
gh api repos/MaximumTrainer/agent-skills/contents/catalogue.json --jq '.sha'
```

## Seeding a repository

To put this skill into a repo that does not have it:

```bash
mkdir -p .claude/skills/skill-exchange/scripts
curl -fsSL https://raw.githubusercontent.com/MaximumTrainer/agent-skills/main/skill-exchange/SKILL.md \
  -o .claude/skills/skill-exchange/SKILL.md
curl -fsSL https://raw.githubusercontent.com/MaximumTrainer/agent-skills/main/skill-exchange/scripts/skills.py \
  -o .claude/skills/skill-exchange/scripts/skills.py
python3 .claude/skills/skill-exchange/scripts/skills.py list
```

Then mention it in the repo's `AGENTS.md` or `CLAUDE.md` in one line, so it is found on the first session rather than the fifth:

> Shared skills live in `MaximumTrainer/agent-skills`. Check the catalogue before writing a new skill or procedure — see `.claude/skills/skill-exchange/`.

Where the repo already keeps skills in `.github/skills/`, the script follows that; do not move existing skills to satisfy the tool.

## What not to do

- **Do not copy a skill's text by hand.** It arrives without provenance, so nothing can tell you when it goes stale and nothing can send an improvement back. Use `pull`.
- **Do not edit a vendored skill to add local paths.** That guarantees a diverge on the next update. Put the local detail in a local skill that defers to the shared one.
- **Do not pull over a MODIFIED skill** without reading the diff. `pull` refuses by default for exactly this reason; `--force` discards local work.
- **Do not contribute repo-specific detail.** A catalogue full of one repo's file paths is a catalogue the other repos stop reading.
- **Do not vendor everything.** Pull what the repo actually needs. Twenty-four skills in a repo that needs three makes selection worse for the agent, not better.
- **Do not fork silently.** If a shared skill is genuinely wrong for this repo and the fix will not generalise, say so in the local skill and record it as a deliberate decision — so the next person knows the divergence was chosen.

## Related skills

- `deliberate-decisions` — recording a chosen divergence from a shared skill
- `docs-drift-guard` — the derived-with-a-drift-check pattern `catalogue.json` follows
- `single-source-constants` — the same argument, applied to values rather than practices
- `verify-and-ship` — landing a contribution back in the catalogue
- `spec-by-example-issue` — when a gap in the catalogue deserves an issue rather than a PR
