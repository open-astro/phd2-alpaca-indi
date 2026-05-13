---
description: Bump version.md, close out CHANGELOG Unreleased into a numbered version, and commit the release
allowed-tools: Bash, Read, Edit, AskUserQuestion
---

Cut a release: ask the user for the semver bump type, update `version.md`,
rename `## [Unreleased]` in `CHANGELOG.md` to the new `## [<version>] - <date>`
heading, gather commit references since the last versioned release, drop in a
fresh empty `## [Unreleased]` for future `/commit` calls, and commit. Push and
tag are user actions, not this skill's.

## Steps

1. **Sanity-check the working tree.** Run `git status`. If there are unstaged or uncommitted changes, refuse to proceed and ask the user to commit them with `/commit` first. Release commits should contain only the version bump and CHANGELOG closeout — nothing else.

2. **Read the current version** from `version.md`. The file format is:
   ```
   # Version

   1.3.0
   ```
   The version is on line 3. Pre-release suffixes (`1.3.0rc1`, `1.3.0-beta.2`) are allowed and should be stripped before bumping.

3. **Read `CHANGELOG.md`.** Confirm `## [Unreleased]` exists and has at least one bullet under one of `### Added` / `### Changed` / `### Removed` / `### Fixed`. If Unreleased is missing or empty, refuse with a clear message — there's nothing to release.

4. **Ask the user for the bump type** using AskUserQuestion. Three choices:
   - **patch** — fixes, internal cleanup, no API/behavior change visible to users. e.g. `1.3.0 → 1.3.1`
   - **minor** — new features, expanded capabilities, additive changes. e.g. `1.3.0 → 1.4.0`
   - **major** — breaking changes: removed APIs, dropped platforms users may rely on, incompatible behavior changes. e.g. `1.3.0 → 2.0.0`

   Use the contents of the Unreleased section to inform a recommendation. If Unreleased has a `### Removed` block listing dropped platforms or features that users could currently be relying on, lean major. If it's only `### Fixed`, lean patch.

5. **Compute the new version.** Strip any pre-release suffix from the current version, then apply the standard semver rule for the chosen bump:
   - patch: bump the third component
   - minor: bump the second component, reset the third to 0
   - major: bump the first component, reset the second and third to 0

6. **Gather commit references** for the release. Find the previous release boundary — preferably via the latest semver tag (`git describe --tags --abbrev=0 --match='v[0-9]*' 2>/dev/null`), and if no tag is found fall back to scanning `CHANGELOG.md` for the previous versioned section's commit references and picking up from the most recent one. Run `git log --oneline <prev>..HEAD` to collect everything since. Format as a `### Commit References` block using the same style as existing versioned entries:
   ```
   - `<short-hash>` - <commit subject>
   ```

7. **Update `version.md`.** Replace the version line with the new version. Leave the `# Version` header and the blank line intact.

8. **Update `CHANGELOG.md`:**
   - Rename the existing `## [Unreleased]` heading to `## [<new-version>] - <YYYY-MM-DD>` using today's UTC date.
   - Append the `### Commit References` block (from step 6) at the end of that section, after any existing subsections.
   - Insert a fresh empty `## [Unreleased]` section directly above the just-renamed version block, so future `/commit` calls have a place to land. The empty Unreleased should look like:
     ```
     ## [Unreleased]

     ```
     (heading + blank line; no subsections until /commit adds them).

9. **Stage both files explicitly** — `git add version.md CHANGELOG.md`. Do not use `git add -A` or `git add .`.

10. **Create the release commit** with a HEREDOC message:
    ```
    Release <new-version>

    See CHANGELOG.md for full release notes.
    ```
    Do not use `--no-verify`. Do not amend prior commits.

11. **Verify** with `git log -1 --stat`. Then remind the user of the typical follow-ups they may want to run themselves:
    - Tag: `git tag v<new-version>`
    - Push: `git push && git push --tags`

## Don't

- Don't push or tag automatically. Both are user actions with side effects on shared remotes.
- Don't include source code changes in the release commit. If `git status` shows working-tree changes at the start, abort and ask the user to `/commit` first.
- Don't backfill commit references for releases older than the current `Unreleased` block. Historical versions in `CHANGELOG.md` are authoritative; don't rewrite them.
- Don't auto-pick the bump type. Always ask. The choice has policy implications the skill can't infer reliably.
- Don't proceed if `Unreleased` is empty. A release with no notes is a process bug, not a no-op.
