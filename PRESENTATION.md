# MiniGit Presentation Guide

## Project idea

MiniGit is a persistent command-line version-control system implemented in C++17. It models the core ideas behind Git with a deliberately small repository format that is easy to inspect and explain.

## Architecture

- `main.cpp` owns command parsing and the `Repository` abstraction.
- `.mingit/HEAD` identifies the active branch.
- `.mingit/refs/<branch>` stores the branch tip commit ID.
- `.mingit/index` stores staged paths.
- `.mingit/objects/commits/<id>/metadata` stores the parent, timestamp, and message.
- `.mingit/objects/commits/<id>/files/` stores the complete snapshot for that commit.

The important design choice is that each commit copies its parent snapshot and overlays staged files. This makes checkout, diff, merge, and restart behavior deterministic.

## Data structures

- `std::vector` stores the staging index.
- `std::map` represents a commit snapshot as `path -> content`.
- Parent commit IDs form a persistent singly linked history.
- `std::set` combines filenames during diff and merge operations.
- `std::unordered_set` finds common ancestors during three-way merge.
- `std::filesystem` handles repository paths and snapshot restoration.

## Five-minute demonstration

Build the program:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o minigit.exe
.\minigit.exe
```

Inside MiniGit:

```text
init
add test.txt
commit -m base version
branch feature
checkout feature
```

Edit `test.txt`, then run:

```text
add test.txt
commit -m feature change
checkout main
```

Edit `test.txt` differently on `main`, then run:

```text
add test.txt
commit -m main change
checkout feature
merge main
```

Explain that MiniGit detects both branches changing the same file and writes conflict markers. Resolve the file, then run:

```text
add test.txt
commit -m resolve conflict
log
diff 0 1
```

Finally, close and reopen the program and run `log` to demonstrate persistence.

## Automated verification

From PowerShell:

```powershell
.\tests\test_minigit.ps1
```

The test script checks compilation, persistence, status, diff, branch checkout, conflict merging, conflict resolution, and deletion commits in an isolated temporary repository.

## Honest limitations

MiniGit is intentionally smaller than Git. It currently tracks regular files, uses integer commit IDs, does not hash or compress objects, and does not support remote repositories. These are clear future extensions rather than hidden behavior.
