# Working rules for coding agents

These rules apply to every automated change in this repository. They are the
minimum; when in doubt, follow the pattern of the surrounding code and docs.

## 1. Never work on `master`

`master` is never committed to directly. Work on a branch derived from `master`,
or on a branch derived from such a branch (both count as "master-derived").

Check before the first commit:

```bash
git branch --show-current
```

If that prints `master` — or prints **nothing**, which means a detached HEAD and
so no branch to commit onto at all — create a branch and continue there:

```bash
git checkout -b <type>/<short-topic> master
```

Existing branch names follow `feat/…`, `fix/…`, `hotfix/…`, `docs/…`.

## 2. Format with pre-commit before every commit

```bash
pre-commit run --all-files
```

`--all-files` (short form `-a`) is the flag; `-all` is not valid. Fix whatever
the hooks report and re-run until the run is clean, with the single exception
noted below. The evidence required before committing is a
`pre-commit run --all-files` whose only remaining failures are that exception,
and a `git status` showing no file the hooks touched that you did not intend to
change.

`pre-commit` is not part of the default environment on JURECA. Install it once
if the command is missing:

```bash
pipx install pre-commit  # or: pip install --user pre-commit
```

If it cannot be installed or run at all, say so plainly in the report of the
change — do not claim the tree was formatted.

The exception: `isort` and `ruff-format` disagree over the imports of
`src/ec_mlp/tf/modifier/dipole_charge_electrode.py` and
`tests/tf/test_data_modifier.py`, so both hooks report `Failed` on a full run
while the two rewrites cancel and the working tree ends up unchanged. These two
hooks, on those two files only, may stay red: confirm with `git status` that the
tree is unchanged, say so in the report, and do not chase it in an unrelated
commit. Every other failure is fixed before the commit.

The `exclude:` pattern in `.pre-commit-config.yaml` is deliberate: it keeps the
hooks away from `doc/`, from the byte-exact regression reference output, and
from the vendored deepmd-kit `.patch` files, which a reformatting hook would
silently invalidate (`git am` would then fail). Do not widen the hook coverage
to those paths, and do not bypass the hooks with `git commit --no-verify`.

One consequence worth stating: because the whole of `doc/` is excluded, nothing
in `doc/src/`, `doc/CHANGELOG.md` or the built book is formatted or checked by
the hooks. A clean `pre-commit` run says nothing about them — match the style of
the surrounding prose by hand.

## 3. Update the docs and the CHANGELOG before every commit

The changelog is `doc/CHANGELOG.md`, not a file at the repository root. Add
entries under the existing `## Unreleased` heading, grouped by the subsystem
headings already in use there (for example `### LAMMPS plugins`,
`### deepmd-kit patches`).

Prose documentation lives in `doc/src/`. A new page must also be registered in
`doc/src/SUMMARY.md`: `doc/book.toml` sets `create-missing = false`, so an
unregistered page is a build error rather than a silent omission. User-facing
behaviour that changes should be reflected in `README.md` as well.

The single exception is a work-in-progress commit, whose subject starts with
`WIP:`. Everything else updates the docs and the changelog in the same commit
as the change.

## 4. Rebuild the book when the docs changed

```bash
cd doc && mdbook build
```

The build output is `doc/ec-mlp-doc/`, and it is tracked in git — commit the
regenerated files together with the source change, so the published site and
`doc/src/` never drift apart.

If `mdbook` is not available in the environment, state explicitly that the book
was not rebuilt. Committing a `doc/src/` change with stale HTML and saying
nothing is the failure mode this rule exists to prevent.

## 5. Squash when merging a pull request

A merged pull request lands on the target branch as **one** commit. Combine the
branch's commits into a single one — on GitHub use _Squash and merge_, locally
`git merge --squash <branch>` followed by one `git commit` — so the target
branch keeps one commit per change, not the working history of the branch.

Write the squashed subject in the repository's convention:
`type(scope): summary`, with `type` one of `feat`, `fix`, `docs`, `test`,
`style`, `refactor`, `chore`. Put the detail in the body.

## Additional expectations

- Do not bypass the checks: no `git commit --no-verify`, and no `git push
--force` to a branch someone else may have pulled. Do not rewrite history that
  has already been pushed.
- Push, open, merge and close pull requests only when asked. Committing locally
  is the default end state of a change.
- Run the tests when Python code changed: `python -m pytest tests`. Say which
  tests were run, and report failures rather than describing the change as done.
- Never hand-edit generated or vendored files: `doc/ec-mlp-doc/` comes from
  `mdbook build`, the `.patch` files are cut with `git format-patch`, and the
  regression reference output under `tests/` (or `dp-patch/regression/`) is
  byte-exact recorded output. Regenerate them at their source instead.
- Keep the repository free of build artifacts, environments, large binaries
  (the `check-added-large-files` hook caps at 10 MB), credentials, and
  machine-specific absolute paths. Cluster paths belong in the report, not in a
  committed file.
- Report honestly: name any rule above that was skipped and why. Silently
  skipping a step is worse than the step being impossible.

## Commit checklist

```bash
git branch --show-current      # 1. not master, not empty
pre-commit run --all-files     # 2. formatting clean
                               # 3. doc/CHANGELOG.md updated; doc/src/ and
                               #    README.md too if the change touches them
                               #    (none of it for a `WIP:` commit)
cd doc && mdbook build && cd ..  # 4. only if doc/src/ changed
git commit
# 5. squash to a single commit when the branch is merged
```
