
# MiniGit

MiniGit is a small command-line version-control system written in C++17. It is designed as a DSA final project and demonstrates repository layout, file snapshots, branches, persistence, and filesystem I/O.

## Current implementation

The maintained implementation is [main.cpp](main.cpp). The older source files are preserved as incremental project-history demos; they are not compiled together with the maintained application.

The current core supports:

| Command | Description |
| --- | --- |
| `init` | Creates a `.mingit` repository. |
| `add <file>` | Adds a file to the persistent staging index. |
| `rm <file>` | Removes a tracked file and stages its deletion. |
| `status` | Shows the current branch and staged files. |
| `commit -m <message>` | Saves a persistent snapshot and commit metadata. |
| `log` | Reads commit history from disk. |
| `diff <id> <id>` | Compares two persistent snapshots line by line. |
| `branch <name>` | Creates a branch reference. |
| `checkout <name>` | Switches branches and restores the branch snapshot. |
| `merge <name>` | Performs fast-forward or three-way merge preparation. |
| `help` | Shows available commands. |

## Build and run on Windows

With MinGW g++:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o minigit.exe
.\minigit.exe
```

With CMake:

```powershell
cmake -S . -B build
cmake --build build
.\build\Debug\minigit.exe
```

The executable must be run from the folder that should become the MiniGit repository. A quick demonstration is:

```text
init
add test.txt
commit -m first persistent commit
log
branch feature
checkout feature
status
exit
```

Close and reopen the program to verify that the commit history remains available.

Run the automated regression test from PowerShell:

```powershell
.\tests\test_minigit.ps1
```

To test differences and merging, create a branch after a commit, make different edits on the two branches, and use `diff 0 1` or `merge feature`. A conflicting merge writes standard conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) into the working file. Resolve the file manually, run `add <file>`, and commit the result.

## Repository layout

```text
.mingit/
    HEAD                 current branch reference
    index                staged file paths
    refs/<branch>        branch tip commit ID
    objects/commits/<id>/
        metadata           parent, timestamp, and message
        files/             snapshot contents
```

## Data structures and concepts

- `std::vector` stores staged paths loaded from the index.
- Commit history is represented by persistent parent IDs.
- Branches are filesystem references to commit IDs.
- `std::filesystem` and standard streams provide repository and snapshot I/O.

See [PRESENTATION.md](PRESENTATION.md) for the architecture explanation and a five-minute demonstration script.
