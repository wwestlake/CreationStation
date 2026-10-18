# Agent Instructions

## Scope

These instructions apply to the umbrella `Creation Suite` repository.

App-specific repositories under `apps/` may also have their own `AGENTS.md` files. When working inside an app repo, follow both:

- this top-level suite policy
- the app-local policy

Follow current user instructions and the umbrella's October 17 shared-workspace and task-completion policy. It supersedes the former per-agent workspace, branch, and build-drop arrangement.

## Development Environment Rule

This workspace is a development/evaluation environment by default.

- Do not preserve legacy filesystem-first project behavior unless the user explicitly asks for backward compatibility.
- Do not keep dual storage authority alive "just in case" if the active task is a suite VFS/container cutover.
- If a suite-managed container/VFS path and a legacy folder/project-file path both exist, prefer making the suite path authoritative and removing the legacy runtime path rather than synchronizing both.
- Treat old folder-era project structures as disposable test data in this environment unless the user explicitly says real assets or historical projects must be preserved.
- Do not reintroduce filesystem-first fallbacks, migration glue, or dual-write logic as a default safety measure when the user's goal is to stop using the filesystem model.

## Path Rule

- No hard-coded filesystem paths in code. Ever.
- Do not hard-code absolute Windows paths, developer-machine-specific roots, fixed user directories, or baked-in local workspace locations into runtime code.
- Resolve locations through the suite settings/bootstrap system, shared storage helpers, configuration, or explicit user choice where appropriate.
- If code still depends on a specific filesystem layout, treat that as technical debt to remove rather than a pattern to extend.

## Shared Ownership Rule

`shared/` is the highest-risk collision area in this workspace.

- Only one writable owner should modify `shared/` at a time.
- If another agent is doing suite-platform work, do not also edit `shared/` in the same checkout.
- Do not treat another agent's untracked files as authoritative without explicit confirmation.

## Multi-Agent Rule

Owner decision, October 17, 2026: this app's submodule checkout is shared by all LLMs under `D:\CreationSuite-Workspaces\DjehutiSuite`. Follow the umbrella `AGENTS.md` and `docs/MULTI_AGENT_WORKFLOW.md` for shared-workspace, feature-branch, build-output, and assignment-closeout policy. The former per-LLM clones/worktrees and binary drop areas are retired.

Coordinate one writable owner per repository and one active build at a time. Hand off the existing checkout and working state. Do not create another clone/build area to switch LLMs, change branches underneath an active owner, or discard another agent's edits.

## Secrets Directory Rule

The `Secrets/` directory contains critical credentials and private keys.
- `Secrets/` MUST remain in `.gitignore` at all times.
- **NEVER** stage, commit, or push any file inside `Secrets/` to any remote repository.

### Branch Ownership Check

Before editing or committing, inspect the current branch and working tree. A shared checkout retains the previous agent's state; an agent prefix is descriptive, not permission to switch away while that agent is active. Continue another agent's feature branch only through an explicit handoff. Start a new task branch only after the repository is available and its existing work is accounted for.

## Integration Rule

Serialize integration within each repository. The agent completing the authorized assignment performs its commit/push/merge/default-branch push under the umbrella's Task Branch and Completion Rule; do not park finished work awaiting a separate integration assignment unless the user directs that.

For submodule work, finish and push the app's default-branch merge first. Then explicitly stage, commit, merge, and push the matching umbrella gitlink update. Coordinate umbrella ownership before changing that checkout. Keep unrelated edits and gitlinks out of the commit.

### Push Authorization Rule

The standing workflow authorizes pushing and merging the work in an assigned implementation task. Do not ask again merely because an old policy required per-action approval. This does not authorize unrelated repositories, deployments, credential changes, destructive cleanup, force-pushes, or new features. Stop and report a real authorization or repository-protection blocker when one remains.

## Commit Scope Rule

Before running `git commit` in ANY repo in this workspace — the umbrella repo, an app repo, or a wiki repo — run `git status --short` immediately before staging and stage only the specific files intended for that commit.

- Never assume the index is clean. Content staged earlier by another agent (or by you, earlier in a session) can silently ride along into an unrelated commit.
- Never use a broad `git add -A` / `git add .`.
- **Wiki commits contain wiki content only.** A commit made inside `wiki/` or any `apps/*/wiki/` checkout must never include changes from the paired code repo, and vice versa.
- If `git status` shows anything staged that you didn't just add yourself and can't explain, stop and ask before committing — don't push it along "because it was already there."
- This rule exists because it has already been violated once: a single new spec file committed via `git add <file>` (skipping a pre-commit `git status` check) swept in four unrelated submodule-pointer bumps that were already staged, and the resulting commit was pushed before anyone noticed. Submodule-pointer updates are the integrator's job per the Integration Rule above — they should never land as a side effect of an unrelated commit.

## Build Rule

Reuse the project's canonical build directory and outputs across all LLMs. Coordinate access and run one build at a time; do not create per-agent build folders, separate checkouts, or binary drops as the default answer to contention. Hand off or wait for the existing build owner.

### Build Configuration Rule

Use `Debug` builds by default for normal development, testing, and troubleshooting.

- `Debug` is the standard day-to-day development build.
- `Release` is only for intentional release work, such as a tagged version release or explicit release-build verification requested by the user.
- Do not assume `Release` just because a build is meant to be runnable.
- When reporting a build result, name the configuration you actually built.

## Merge Rule

Do not merge just because code exists locally.

Before reporting something as complete or integrated, verify:

- it is committed in the correct repo
- it is on the correct branch
- any required submodule pointer updates are done
- the relevant build or smoke checks were actually run when possible

## GitHub Project Board Rule

Always use GitHub Issues and the official GitHub Project Board (**Creation Suite Road Map** - Project #19) to track feature work, tasks, and completion status.

- **Shorthand Definition**: Whenever the user says "create a task", "add tasks", "track tasks", or similar, it ALWAYS means: *"Create real GitHub Issues on the repository for the active app/component (`gh issue create`) and add them directly to the **Creation Suite Road Map** project board (`gh project item-add 19 --owner wwestlake --url <issue-url>`)"*.
- **Status Sync Rule**: Every time an agent completes a task, fixes an issue, or changes work status, the agent MUST immediately update the corresponding GitHub Issue and Project Board item on **Creation Suite Road Map** (Project #19), adding completion comments (`gh issue comment`) and explicitly setting the project board item Status field to **Ready for Testing** (`gh project item-edit --id <item-id> --project-id PVT_kwHOADBc_84Bet07 --field-id PVTSSF_lAHOADBc_84Bet07zhZF9co --single-select-option-id a822205a`).
- Do not use temporary local `.md` task files as the primary task tracker.

## C: Drive Rule

**Under no circumstances write, download, build, or install anything on the C: drive. Period.** No exceptions for size or a tool's default location — redirect to a D:-drive path instead. See the root `AGENTS.md`'s C: Drive Rule for the full incident.

## LLVM / vcpkg Build Rule

**Never build, rebuild, or touch LLVM (`apps/CreationEngine/vcpkg_installed/x64-windows/`, which CreationStation's FRust scripting layer also depends on via `shared/CMake/CreationSharedLLVM.cmake`) — directly or as a side effect of any `vcpkg` command — without an explicit, in-the-moment yes from the user.** Running `vcpkg install` in manifest mode anywhere reconciles the whole dependency list and can silently trigger a full rebuild even when adding something unrelated. See the root `AGENTS.md`'s LLVM / vcpkg Build Rule for the full incident and what to do instead. If blocked, stop and ask — never act.

## No OS Dialogs Except Asset Import/Export Rule

**The app never opens a native OS file/folder dialog except for explicit asset import/export** (importing a WAV, exporting a render, saving/loading a project-level file the user explicitly asked to save/load). Anything that is "pick a thing the framework already knows about" — plugin selection being the concrete case — must be selected from the framework's own pre-scanned/registered catalog only, never via `juce::FileChooser` browse-for-file/folder.

This was violated on 2026-08-11: the Signal Graph's "VST Host" node's `showPluginLoadMenu()` (`MainComponent.cpp`) fell back to (and always additionally offered, via a "Browse manually..." item) a raw OS file dialog to browse for a `.vst3` DLL directly, instead of exclusively listing `vstPluginCatalog`'s scanned entries. Fixed by removing both OS-dialog escape hatches — the menu now only ever lists catalog entries, plus "Rescan" and "Manage VST folders..." (which configure *where* the framework scans, not which plugin to load). See the root `AGENTS.md`'s equivalent entry and [memory: feedback-no-os-dialogs-except-asset-io] for the full incident.

Before wiring any "choose an X" UI, check whether the framework already has a scanned/registered list for X. If it does, the picker must be a menu/list over that catalog only.

## Required Reference

For the full concurrency and handoff process, read:

- [Suite multi-agent workflow](../../docs/MULTI_AGENT_WORKFLOW.md)

## ASIO Is Required (Do Not Disable)

ASIO support is a required part of Djehuti Station, in every build and in the released binary. Station is deliberately GPLv3 so that it can use the GPLv3 version of Steinberg's ASIO SDK (2.3.4, pinned in `CMakeLists.txt` and in the release workflow).

- **Never** disable, remove, gate behind an option, default to OFF, or "work around" ASIO, and never leave it out of a build because it looks proprietary or licence-sensitive. The licensing decision has been made by the owner. It is not yours to reopen.
- The build and the release pipeline both fail if ASIO is missing. If a build cannot get the SDK, fix the download or the path. Do not remove the requirement.
- If you think something about ASIO is wrong, stop and ask the user. Do not change it silently. (Before this rule, ASIO was a CMake switch defaulting to OFF and it kept vanishing from the audio settings when a build was made without the switch, or when it was turned off without telling anyone.)
