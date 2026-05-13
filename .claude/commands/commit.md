---
description: Format staged C++ with clang-format-18, then create a clean commit
allowed-tools: Bash, Read, Edit
---

Stage what's currently changed, run clang-format on the project's C++ sources so
CI's formatting check will pass, then create a commit with a message that
captures *why* the change exists.

## Steps

1. **Survey the working tree** — run `git status` and `git diff` (and `git diff --staged` if anything is already staged) to understand what changed and why. Read the actual hunks; don't trust the file list alone.

2. **Run clang-format on changed C++ sources.** The project ships `build/run-clang-format` which formats every `.cpp`/`.h` in `src/` and `contributions/MPI_IS_gaussian_process/`. CI uses `clang-format-18` and will fail the PR if local formatting drifts.

   Try in order:
   - `./build/run-clang-format` — works if `clang-format` or `clang-format-18` is on `$PATH`
   - `clang-format-18 -i <changed .cpp/.h files>` — direct invocation
   - `pip install --quiet "clang-format==18.1.8" && ./build/run-clang-format` — install on the fly if absent
   - Manual fixes from `.clang-format` style: if no clang-format is installable, read the rules at the rules section below and hand-apply

   After formatting, re-run `git diff` to see what the formatter changed.

3. **Stage the right files.** Use specific `git add <file>` paths, not `git add -A` or `git add .` — sensitive files (.env, credentials) or generated/build artifacts can sneak in. Skip anything outside the scope of what the user actually changed.

4. **Look at recent commits** (`git log -5 --oneline`) to match the project's commit message style — it's terse, lowercase first word after the type, no trailing period in the title.

5. **Write the commit message.** Use a HEREDOC. Structure:
   - **Subject (≤72 chars):** what changed in plain language. Imperative mood. No type prefix unless recent commits use one.
   - **Blank line.**
   - **Body:** explain the *why* — what failure mode this fixes, what user-visible behavior changes, why this approach was chosen over alternatives. Bullet points for multi-part changes. Mention non-obvious tradeoffs.
   - **Co-author trailer:** `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`

6. **Create the commit** with the HEREDOC message. Do not amend an existing commit unless the user explicitly asks. Do not use `--no-verify`.

7. **Verify** with `git log -1 --stat` so the user can see what landed.

## Don't

- Don't push. The user pushes when they're ready.
- Don't include unrelated changes in the same commit. If you find drift, mention it and let the user decide.
- Don't write a summary that just restates the file list. Explain motivation.
- Don't commit if the working tree is clean (no changes to make).

## clang-format rules cheat-sheet (the things that bite most)

If you have to hand-format, these are the patterns the project enforces (from `.clang-format`):

- **No single-line ifs.** `if (cond) foo;` becomes:
  ```cpp
  if (cond)
      foo;
  ```
- **No same-line braces in compound statements.** `if (cond) { foo; break; }` becomes the four-line block form.
- **Long function calls wrap at the opening paren, not arguments.** If a `Func(arg1, arg2, ...)` call breaks 120 cols, prefer:
  ```cpp
  Func(
      arg1, arg2, arg3);
  ```
  over wrapping between arguments at the call-site column.
- **Trailing comments are single-space-separated.** `int x; // note` not `int x;    // note` — clang-format collapses multiple spaces before `//`.
- **Braces on own line** (Allman style) for functions, classes, control flow.

When in doubt, mirror the surrounding file's existing style.
