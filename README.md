# MiniGit

MiniGit is a small, persistent command-line version-control system written in C++17. It is an academic DSA project inspired by the core workflow of Git:

```text
working files -> staging index -> commit snapshot -> branch history
```

The project demonstrates file systems, file input/output, snapshot storage, linked commit history, branching, checkout, line-level diff, fast-forward merge, three-way merge, conflict detection, and deletion tracking.

## What This Project Does

The maintained implementation is [main.cpp](main.cpp). The older `.cpp` files are preserved as incremental project-history demonstrations. They each contain their own `main()` function and should not be compiled together with `main.cpp`.

MiniGit stores repository data on disk, so commits remain available after the program is closed and reopened.

### Supported commands

| Command | What it does |
| --- | --- |
| `init` | Creates a new `.mingit` repository in the current folder. |
| `add <file>` | Stages a file for the next commit. |
| `rm <file>` | Removes a tracked file and stages its deletion. |
| `status` | Shows the current branch and staged, modified, new, or deleted files. |
| `commit -m <message>` | Saves a complete persistent snapshot. |
| `log` | Displays commit history from newest to oldest. |
| `branch <name>` | Creates a branch pointing to the current commit. |
| `checkout <name>` | Switches branches and restores that branch's snapshot. |
| `diff <id1> <id2>` | Compares two commit snapshots line by line. |
| `merge <name>` | Performs a fast-forward or three-way merge. |
| `help` | Displays the command list. |
| `exit` or `quit` | Closes MiniGit. |

## Requirements

On Windows, you need:

- Windows PowerShell
- MinGW g++ with C++17 support
- Git, if you want to clone or push the project

Check that the compiler is installed:

```powershell
g++ --version
```

The project was tested with MinGW g++ and C++17.

## Quick Start

### 1. Open the project folder

Open PowerShell and run:

```powershell
cd "C:\Users\amanu\Desktop\MiniGit-Project-main"
```

Use your own folder path if the project is stored somewhere else.

### 2. Compile the maintained implementation

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o minigit.exe
```

Explanation:

- `g++` invokes the C++ compiler.
- `-std=c++17` enables the required C++17 language standard.
- `-Wall -Wextra -pedantic` enables useful compiler warnings.
- `main.cpp` is the maintained source file.
- `-o minigit.exe` creates the Windows executable.

If the command prints no errors, the build succeeded.

### 3. Start MiniGit

```powershell
.\minigit.exe
```

You should see:

```text
MiniGit 1.0
Type 'help' for commands.
minigit>
```

## Beginner Walkthrough

This is the smallest useful workflow. Type each command after the `minigit>` prompt.

```text
help
init
add test.txt
status
commit -m first commit
log
exit
```

What happens:

1. `help` displays the available commands.
2. `init` creates `.mingit`.
3. `add test.txt` places the file in the staging index.
4. `status` shows the staged file.
5. `commit -m first commit` creates commit `0`.
6. `log` displays the new commit.
7. `exit` closes the program.

After the commit, the repository contains a snapshot of `test.txt`.

## Verify Persistence

Persistence means that MiniGit remembers commits after restarting.

Start the program again:

```powershell
.\minigit.exe
```

Then run:

```text
log
status
exit
```

The previous commit should still appear. `status` should show the current branch and a clean working tree.

## Complete Feature Test

The following walkthrough demonstrates branches, checkout, diff, merge conflicts, conflict resolution, deletion, and persistence.

### Create the base commit

Start MiniGit:

```powershell
.\minigit.exe
```

Run:

```text
init
add test.txt
commit -m base version
branch feature
exit
```

At this point:

- `main` points to commit `0`.
- `feature` also points to commit `0`.
- Both branches start from the same snapshot.

### Change the main branch

Close MiniGit if it is still running, then edit the file from PowerShell:

```powershell
Set-Content test.txt "main version"
```

Start MiniGit and run:

```powershell
.\minigit.exe
```

```text
add test.txt
commit -m main change
checkout feature
exit
```

The main branch now has commit `1`. Checking out `feature` restores the original base version because the feature branch still points to commit `0`.

### Change the feature branch differently

Edit the file again:

```powershell
Set-Content test.txt "feature version"
```

Run:

```powershell
.\minigit.exe
```

```text
add test.txt
commit -m feature change
diff 0 1
merge main
status
exit
```

The feature branch now has commit `2`. The `diff` command compares two snapshots. The merge detects that both branches changed the same file and prints:

```text
CONFLICT: test.txt
Merge stopped with conflicts. Resolve files, then commit.
```

The working file contains conflict markers:

```text
<<<<<<< HEAD
feature version
=======
main version
>>>>>>> main
```

### Resolve and commit the conflict

Replace the conflict contents with your chosen final version:

```powershell
Set-Content test.txt "resolved version"
```

Run:

```powershell
.\minigit.exe
```

```text
add test.txt
commit -m resolve conflict
log
exit
```

The merge resolution is now stored as commit `3` on the feature branch.

### Test deletion tracking

Start MiniGit again:

```powershell
.\minigit.exe
```

Run:

```text
rm test.txt
status
commit -m delete test file
log
exit
```

Expected status output includes:

```text
deleted:  test.txt
```

After the commit, `test.txt` is removed from the working tree and from the new snapshot.

## Automated Testing

The repository includes a PowerShell regression test at [tests/test_minigit.ps1](tests/test_minigit.ps1). It uses a temporary directory, so it does not modify your project files or your real `.mingit` repository.

From the project root, run:

```powershell
.\tests\test_minigit.ps1
```

Expected output:

```text
All MiniGit tests passed.
```

The automated test checks:

- C++17 compilation
- repository initialization
- persistent commit creation
- history after restarting the program
- branch creation and checkout
- modified-file status
- line-level diff
- divergent branch commits
- three-way merge conflict detection
- conflict resolution
- deletion tracking and deletion commits

If PowerShell blocks local scripts, run this once for the current PowerShell process:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

Then run the test again.

## Safe Manual Testing Folder

To avoid changing the actual project folder during a manual demonstration, create a temporary copy:

```powershell
$demo = Join-Path $env:TEMP "minigit-demo"
Remove-Item $demo -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $demo | Out-Null
Copy-Item .\minigit.exe, .\test.txt $demo
Set-Location $demo
```

Now run MiniGit from that temporary folder:

```powershell
.\minigit.exe
```

When finished, return to the project with:

```powershell
Set-Location "C:\Users\amanu\Desktop\MiniGit-Project-main"
```

You can remove the temporary demo folder with:

```powershell
Remove-Item $demo -Recurse -Force
```

## Repository Storage Design

After running `init`, MiniGit creates:

```text
.mingit/
├── HEAD
├── index
├── refs/
│   ├── main
│   └── feature
└── objects/
    └── commits/
        └── 0/
            ├── metadata
            └── files/
                └── test.txt
```

### Meaning of each item

- `.mingit/HEAD` stores the active branch, such as `ref: refs/main`.
- `.mingit/index` stores staged relative file paths.
- `.mingit/refs/main` stores the commit ID at the tip of `main`.
- `.mingit/refs/feature` stores the commit ID at the tip of `feature`.
- `metadata` stores the parent ID, timestamp, and commit message.
- `files/` stores the complete file snapshot for that commit.

Each commit starts with its parent's snapshot and overlays the staged files. This is why checkout, diff, merge, and restart behavior remain consistent.

## Data Structures and Algorithms

- `std::vector<std::string>` stores staged paths loaded from the index.
- `std::map<std::string, std::string>` represents a snapshot as `path -> file content`.
- Parent commit IDs form a persistent singly linked history.
- `std::set` combines filenames during diff and merge operations.
- `std::unordered_set` helps find a common ancestor for three-way merge.
- `std::filesystem` handles repository paths, directories, copying, and deletion.
- Standard streams handle metadata and file contents.

### Merge behavior

MiniGit supports three cases:

1. **Fast-forward:** the current branch has no unique changes, so its reference moves to the target commit.
2. **No-op:** the target branch is already behind the current branch.
3. **Three-way merge:** both branches changed after a common ancestor. Non-conflicting files are prepared automatically; files changed differently on both branches receive conflict markers.

## CMake Build

If CMake is installed, use:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\minigit.exe
```

The direct g++ command is the simplest option on Windows. `CMakeLists.txt` is included for reproducible IDE and multi-platform builds.

## Project Files

| File | Purpose |
| --- | --- |
| [main.cpp](main.cpp) | Maintained persistent MiniGit implementation. |
| [tests/test_minigit.ps1](tests/test_minigit.ps1) | Automated end-to-end regression test. |
| [PRESENTATION.md](PRESENTATION.md) | Architecture explanation and five-minute demo plan. |
| [CMakeLists.txt](CMakeLists.txt) | CMake build configuration. |
| `init.cpp`, `add.cpp`, `commit.cpp`, `log.cpp`, `branching.cpp`, `checkout.cpp`, `diff.cpp`, `merge.cpp`, `Final.cpp` | Earlier incremental implementations preserved for project history. |
| `test.txt` | Sample file used in manual demonstrations. |

## Troubleshooting

### `g++ is not recognized`

Install MinGW-w64 or MSYS2, then add the compiler's `bin` folder to the Windows `PATH`. Restart PowerShell and check:

```powershell
g++ --version
```

### `minigit.exe is not recognized`

Use the PowerShell path prefix:

```powershell
.\minigit.exe
```

### `Not a MiniGit repository`

Run `init` in the folder where you want MiniGit to store `.mingit`.

### The test script cannot run

Run:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\tests\test_minigit.ps1
```

### I want to restart a manual test from zero

Only in the folder used for the test, run:

```powershell
Remove-Item .mingit -Recurse -Force
```

This deletes MiniGit history for that folder. It does not delete the source code.

## Academic Scope and Honest Limitations

MiniGit is a focused educational implementation, not a replacement for Git. It currently:

- tracks regular files rather than full operating-system metadata;
- uses integer commit IDs instead of cryptographic hashes;
- stores snapshots as readable files rather than compressed objects;
- does not provide remotes, networking, authentication, or collaboration;
- does not implement every Git status edge case;
- does not include a graphical interface.

These limitations make the code easier to understand and present. Suitable future work includes content-addressed hashing, binary-safe metadata escaping, rename detection, remote repositories, more granular line merging, and a graphical client.

## Presentation Checklist

For a strong demonstration:

1. Build with warnings enabled.
2. Run the automated test and show `All MiniGit tests passed.`
3. Create a base commit and branch.
4. Make different edits on two branches.
5. Run `merge` and explain the conflict markers.
6. Resolve the conflict and create a new commit.
7. Run `log` after restarting the program to prove persistence.
8. Show `.mingit` in File Explorer or PowerShell and explain its layout.

For a longer architecture explanation, see [PRESENTATION.md](PRESENTATION.md).
