# CI policy for the `custom` branch

CI minutes are limited, so this fork runs as little as possible on GitHub
Actions and expects tests to be run locally. Start small, add jobs later
when they prove necessary.

## What runs when

| Trigger                    | Workflow | Jobs                                   |
| -------------------------- | -------- | -------------------------------------- |
| Pull request to `custom`   | Lint     | buildifier                             |
|                            | Linux    | `build` (package, release_build)       |
| Push (merge) to `custom`   | Lint     | buildifier                             |
|                            | Linux    | `build`                                |
|                            | macOS    | `build_arm64`                          |
|                            | Windows  | `build_on_x64`                         |
| Manual (`workflow_dispatch`) | any    | as above; with `full: true`, every job (tests, other architectures) |
| Manual only                | Android  | all jobs                               |

* Pushes to feature branches do not trigger CI. Open a PR instead.
* Changes that touch only `*.md` / `docs/**` do not trigger CI.
* Tests (`bazel test ...`) are not run automatically. In CI the debug test
  build takes longer than the release build itself (Linux: ~27 min vs ~10
  min), so run them locally.

To run every job manually: Actions tab → pick the workflow → "Run workflow"
→ check `full`.

## Running checks locally (Linux)

Install the build dependencies once (see
[build_mozc_for_linux.md](build_mozc_for_linux.md)), then from `src/`:

```
# Tests for the area you changed (fast)
bazelisk test //rewriter/... -c dbg --test_output=errors

# Everything the Linux test job would run (slow)
bazelisk test ... -c dbg --build_tests_only --test_output=errors

# Same build as the Linux CI job
bazelisk build package --config release_build

# Lint, same as the Lint CI job
go install github.com/bazelbuild/buildtools/buildifier@latest
$(go env GOPATH)/bin/buildifier --mode=diff -r .
```

Set `ANDROID_NDK_HOME=` (empty) if an Android NDK is installed but not needed.
If `releases.bazel.build` is unreachable, point bazelisk at GitHub instead:
`export BAZELISK_BASE_URL=https://github.com/bazelbuild/bazel/releases/download`.

To validate workflow edits before pushing:

```
go install github.com/rhysd/actionlint/cmd/actionlint@latest
$(go env GOPATH)/bin/actionlint
```
