#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

class Repository {
public:
    explicit Repository(fs::path root) : root_(fs::absolute(std::move(root))) {}

    bool initialize() const {
        if (fs::exists(metadataPath())) {
            std::cout << "MiniGit repository is already initialized.\n";
            return false;
        }

        std::error_code error;
        fs::create_directories(objectsPath(), error);
        fs::create_directories(refsPath(), error);
        fs::create_directories(commitsPath(), error);
        if (error) {
            std::cerr << "Unable to create repository: " << error.message() << "\n";
            return false;
        }

        writeText(metadataPath() / "HEAD", "ref: refs/main\n");
        writeText(refsPath() / "main", "\n");
        writeText(metadataPath() / "index", "");
        std::cout << "Initialized empty MiniGit repository in .mingit/\n";
        return true;
    }

    bool isInitialized() const {
        return fs::exists(metadataPath() / "HEAD") && fs::exists(commitsPath());
    }

    bool add(const fs::path& input) const {
        if (!isInitialized()) {
            std::cout << "Not a MiniGit repository. Run 'init' first.\n";
            return false;
        }

        fs::path relative;
        if (!normalizePath(input, relative)) {
            return false;
        }

        const Snapshot currentFiles = readSnapshot(currentCommit());
        if (!fs::is_regular_file(root_ / relative) && currentFiles.count(relative.generic_string()) == 0) {
            std::cout << "File does not exist or is not tracked: " << input.string() << "\n";
            return false;
        }

        std::vector<std::string> files = readIndex();
        const std::string name = relative.generic_string();
        for (const std::string& staged : files) {
            if (staged == name) {
                std::cout << "Already staged: " << name << "\n";
                return true;
            }
        }

        files.push_back(name);
        writeIndex(files);
        std::cout << "Staged '" << name << "'.\n";
        return true;
    }

    bool removeFile(const fs::path& input) const {
        if (!requireRepository()) {
            return false;
        }

        fs::path relative;
        if (!normalizePath(input, relative)) {
            return false;
        }
        const std::string name = relative.generic_string();
        const Snapshot currentFiles = readSnapshot(currentCommit());
        if (currentFiles.count(name) == 0 && !fs::exists(root_ / relative)) {
            std::cout << "File is not tracked: " << name << "\n";
            return false;
        }

        std::error_code error;
        fs::remove(root_ / relative, error);
        if (error) {
            std::cout << "Unable to remove '" << name << "': " << error.message() << "\n";
            return false;
        }
        stageRelative(name);
        std::cout << "Staged deletion: " << name << "\n";
        return true;
    }

    void status() const {
        if (!requireRepository()) {
            return;
        }

        std::cout << "On branch " << currentBranch() << "\n";
        const Snapshot currentFiles = readSnapshot(currentCommit());
        const std::vector<std::string> files = readIndex();
        bool hasChanges = false;
        if (!files.empty()) {
            std::cout << "Changes staged for commit:\n";
            for (const std::string& file : files) {
                const fs::path workingFile = root_ / file;
                const auto tracked = currentFiles.find(file);
                if (!fs::exists(workingFile)) {
                    std::cout << "  deleted:  " << file << "\n";
                } else if (tracked == currentFiles.end()) {
                    std::cout << "  new:      " << file << "\n";
                } else if (readText(workingFile) != tracked->second) {
                    std::cout << "  modified: " << file << "\n";
                } else {
                    std::cout << "  unchanged: " << file << "\n";
                }
                hasChanges = true;
            }
        }

        for (const auto& tracked : currentFiles) {
            const fs::path workingFile = root_ / tracked.first;
            if (!fs::exists(workingFile) &&
                std::find(files.begin(), files.end(), tracked.first) == files.end()) {
                std::cout << "  unstaged deletion: " << tracked.first << "\n";
                hasChanges = true;
            } else if (readText(workingFile) != tracked.second &&
                       std::find(files.begin(), files.end(), tracked.first) == files.end()) {
                std::cout << "  unstaged modification: " << tracked.first << "\n";
                hasChanges = true;
            }
        }
        if (!hasChanges) {
            std::cout << "Working tree clean.\n";
        }
    }

    bool commit(const std::string& message) const {
        if (!requireRepository()) {
            return false;
        }
        const std::vector<std::string> files = readIndex();
        if (files.empty()) {
            std::cout << "Nothing to commit. Use 'add <file>' first.\n";
            return false;
        }
        if (message.empty()) {
            std::cout << "Commit message cannot be empty.\n";
            return false;
        }

        const int id = nextCommitId();
        const fs::path snapshot = commitsPath() / std::to_string(id) / "files";
        std::error_code error;
        fs::create_directories(snapshot, error);
        if (error) {
            std::cerr << "Unable to create commit: " << error.message() << "\n";
            return false;
        }

        const int parent = currentCommit();
        if (parent >= 0) {
            copySnapshot(parent, snapshot);
        }
        for (const std::string& file : files) {
            const fs::path source = root_ / file;
            const fs::path destination = snapshot / file;
            error.clear();
            if (fs::is_regular_file(source, error)) {
                fs::create_directories(destination.parent_path(), error);
                error.clear();
                fs::remove(destination, error);
                error.clear();
                fs::copy_file(source, destination, fs::copy_options::none, error);
                if (error) {
                    std::cerr << "Unable to save '" << file << "': " << error.message() << "\n";
                    return false;
                }
            } else {
                error.clear();
                fs::remove(destination, error);
            }
        }

        std::ostringstream metadata;
        metadata << "parent=" << parent << "\n";
        metadata << "timestamp=" << timestamp() << "\n";
        metadata << "message=" << message << "\n";
        writeText(commitsPath() / std::to_string(id) / "metadata", metadata.str());
        writeText(refsPath() / currentBranch(), std::to_string(id) + "\n");
        writeText(metadataPath() / "index", "");

        std::cout << "Created commit " << id << ": " << message << "\n";
        return true;
    }

    void log() const {
        if (!requireRepository()) {
            return;
        }

        int id = currentCommit();
        if (id < 0) {
            std::cout << "No commits yet.\n";
            return;
        }

        while (id >= 0) {
            const CommitInfo info = readCommit(id);
            std::cout << "commit " << id << "\n";
            std::cout << "Branch: " << currentBranch() << "\n";
            std::cout << "Date: " << info.timestamp << "\n";
            std::cout << "    " << info.message << "\n\n";
            id = info.parent;
        }
    }

    bool createBranch(const std::string& name) const {
        if (!requireRepository()) {
            return false;
        }
        if (!validBranchName(name)) {
            std::cout << "Invalid branch name. Use letters, numbers, '-' or '_'.\n";
            return false;
        }
        const fs::path branch = refsPath() / name;
        if (fs::exists(branch)) {
            std::cout << "Branch already exists: " << name << "\n";
            return false;
        }
        writeText(branch, std::to_string(currentCommit()) + "\n");
        std::cout << "Created branch '" << name << "'.\n";
        return true;
    }

    bool checkout(const std::string& name) const {
        if (!requireRepository()) {
            return false;
        }
        const fs::path branch = refsPath() / name;
        if (!fs::exists(branch)) {
            std::cout << "Branch does not exist: " << name << "\n";
            return false;
        }

        const int id = readInteger(branch);
        if (id >= 0) {
            restoreSnapshot(id);
        }
        writeText(metadataPath() / "HEAD", "ref: refs/" + name + "\n");
        std::cout << "Switched to branch '" << name << "'.\n";
        return true;
    }

    void diff(int first, int second) const {
        if (!requireRepository()) {
            return;
        }
        if (!commitExists(first) || !commitExists(second)) {
            std::cout << "One or both commit IDs do not exist.\n";
            return;
        }

        const Snapshot left = readSnapshot(first);
        const Snapshot right = readSnapshot(second);
        std::set<std::string> files;
        for (const auto& file : left) {
            files.insert(file.first);
        }
        for (const auto& file : right) {
            files.insert(file.first);
        }

        bool changed = false;
        for (const std::string& file : files) {
            const auto leftFile = left.find(file);
            const auto rightFile = right.find(file);
            if (leftFile == left.end()) {
                std::cout << "Added: " << file << "\n";
                changed = true;
            } else if (rightFile == right.end()) {
                std::cout << "Removed: " << file << "\n";
                changed = true;
            } else if (leftFile->second != rightFile->second) {
                printLineDiff(file, leftFile->second, rightFile->second);
                changed = true;
            }
        }
        if (!changed) {
            std::cout << "No changes between commits " << first << " and " << second << ".\n";
        }
    }

    bool merge(const std::string& targetBranch) const {
        if (!requireRepository()) {
            return false;
        }
        const fs::path targetRef = refsPath() / targetBranch;
        if (!fs::exists(targetRef)) {
            std::cout << "Branch does not exist: " << targetBranch << "\n";
            return false;
        }

        const int current = currentCommit();
        const int target = readInteger(targetRef);
        if (target < 0) {
            std::cout << "The target branch has no commits.\n";
            return false;
        }
        if (current == target) {
            std::cout << "Already up to date.\n";
            return true;
        }

        const int base = findCommonAncestor(current, target);
        if (base < 0) {
            std::cout << "Branches have no common ancestor.\n";
            return false;
        }
        if (base == current) {
            restoreSnapshot(target);
            writeText(refsPath() / currentBranch(), std::to_string(target) + "\n");
            std::cout << "Fast-forwarded to commit " << target << ".\n";
            return true;
        }
        if (base == target) {
            std::cout << "Already up to date; target branch is behind current branch.\n";
            return true;
        }

        const Snapshot baseFiles = readSnapshot(base);
        const Snapshot currentFiles = readSnapshot(current);
        const Snapshot targetFiles = readSnapshot(target);
        std::set<std::string> files;
        for (const auto& file : baseFiles) {
            files.insert(file.first);
        }
        for (const auto& file : currentFiles) {
            files.insert(file.first);
        }
        for (const auto& file : targetFiles) {
            files.insert(file.first);
        }

        bool conflicts = false;
        for (const std::string& file : files) {
            const std::string baseContent = contentOf(baseFiles, file);
            const std::string currentContent = contentOf(currentFiles, file);
            const std::string targetContent = contentOf(targetFiles, file);
            if (currentContent == targetContent) {
                continue;
            }
            if (currentContent == baseContent) {
                writeWorkingFile(file, targetContent);
                stageRelative(file);
            } else if (targetContent == baseContent) {
                continue;
            } else {
                conflicts = true;
                writeWorkingFile(file, "<<<<<<< HEAD\n" + currentContent +
                    "\n=======\n" + targetContent + "\n>>>>>>> " + targetBranch + "\n");
                stageRelative(file);
                std::cout << "CONFLICT: " << file << "\n";
            }
        }

        if (conflicts) {
            std::cout << "Merge stopped with conflicts. Resolve files, then commit.\n";
        } else {
            std::cout << "Merge prepared successfully. Commit the staged changes.\n";
        }
        return true;
    }

private:
    using Snapshot = std::map<std::string, std::string>;

    struct CommitInfo {
        int parent = -1;
        std::string timestamp;
        std::string message;
    };

    fs::path root_;

    fs::path metadataPath() const { return root_ / ".mingit"; }
    fs::path objectsPath() const { return metadataPath() / "objects"; }
    fs::path refsPath() const { return metadataPath() / "refs"; }
    fs::path commitsPath() const { return objectsPath() / "commits"; }

    bool requireRepository() const {
        if (!isInitialized()) {
            std::cout << "Not a MiniGit repository. Run 'init' first.\n";
            return false;
        }
        return true;
    }

    static void writeText(const fs::path& path, const std::string& value) {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << value;
    }

    static std::string readText(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    std::vector<std::string> readIndex() const {
        std::vector<std::string> files;
        std::istringstream input(readText(metadataPath() / "index"));
        std::string file;
        while (std::getline(input, file)) {
            if (!file.empty()) {
                files.push_back(file);
            }
        }
        return files;
    }

    void writeIndex(const std::vector<std::string>& files) const {
        std::ostringstream output;
        for (const std::string& file : files) {
            output << file << "\n";
        }
        writeText(metadataPath() / "index", output.str());
    }

    bool normalizePath(const fs::path& input, fs::path& relative) const {
        const fs::path absolute = fs::absolute(input);
        std::error_code error;
        relative = fs::relative(absolute, root_, error).lexically_normal();
        const std::string relativeName = relative.generic_string();
        if (error || relative.empty() || relative == "." ||
            (relativeName.size() >= 2 && relativeName.compare(0, 2, "..") == 0)) {
            std::cout << "File must be inside the repository.\n";
            return false;
        }
        if (relative.begin() != relative.end() && *relative.begin() == ".mingit") {
            std::cout << "Files inside .mingit cannot be staged.\n";
            return false;
        }
        return true;
    }

    std::string currentBranch() const {
        const std::string head = readText(metadataPath() / "HEAD");
        const std::string prefix = "ref: refs/";
        if (head.rfind(prefix, 0) == 0) {
            std::string branch = head.substr(prefix.size());
            while (!branch.empty() && (branch.back() == '\n' || branch.back() == '\r')) {
                branch.pop_back();
            }
            return branch;
        }
        return "main";
    }

    int currentCommit() const {
        return readInteger(refsPath() / currentBranch());
    }

    static int readInteger(const fs::path& path) {
        std::ifstream input(path);
        int value = -1;
        input >> value;
        return input ? value : -1;
    }

    int nextCommitId() const {
        int highest = -1;
        if (!fs::exists(commitsPath())) {
            return 0;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(commitsPath())) {
            if (!entry.is_directory()) {
                continue;
            }
            try {
                highest = std::max(highest, std::stoi(entry.path().filename().string()));
            } catch (const std::exception&) {
                continue;
            }
        }
        return highest + 1;
    }

    CommitInfo readCommit(int id) const {
        CommitInfo info;
        std::istringstream input(readText(commitsPath() / std::to_string(id) / "metadata"));
        std::string line;
        while (std::getline(input, line)) {
            if (line.rfind("parent=", 0) == 0) {
                info.parent = std::stoi(line.substr(7));
            } else if (line.rfind("timestamp=", 0) == 0) {
                info.timestamp = line.substr(10);
            } else if (line.rfind("message=", 0) == 0) {
                info.message = line.substr(8);
            }
        }
        return info;
    }

    bool commitExists(int id) const {
        return id >= 0 && fs::exists(commitsPath() / std::to_string(id) / "metadata");
    }

    Snapshot readSnapshot(int id) const {
        Snapshot snapshot;
        const fs::path files = commitsPath() / std::to_string(id) / "files";
        if (!fs::exists(files)) {
            return snapshot;
        }

        std::error_code error;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(files, error)) {
            if (error || !entry.is_regular_file()) {
                continue;
            }
            const fs::path relative = fs::relative(entry.path(), files, error);
            if (!error) {
                snapshot[relative.generic_string()] = readText(entry.path());
            }
        }
        return snapshot;
    }

    void copySnapshot(int id, const fs::path& destination) const {
        const fs::path source = commitsPath() / std::to_string(id) / "files";
        if (!fs::exists(source)) {
            return;
        }

        std::error_code error;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source, error)) {
            if (error || !entry.is_regular_file()) {
                continue;
            }
            const fs::path relative = fs::relative(entry.path(), source, error);
            const fs::path target = destination / relative;
            fs::create_directories(target.parent_path(), error);
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, error);
        }
    }

    static std::string contentOf(const Snapshot& snapshot, const std::string& file) {
        const auto found = snapshot.find(file);
        return found == snapshot.end() ? std::string() : found->second;
    }

    void writeWorkingFile(const std::string& file, const std::string& content) const {
        writeText(root_ / file, content);
    }

    void stageRelative(const std::string& file) const {
        std::vector<std::string> files = readIndex();
        if (std::find(files.begin(), files.end(), file) == files.end()) {
            files.push_back(file);
            writeIndex(files);
        }
    }

    int findCommonAncestor(int first, int second) const {
        std::unordered_set<int> ancestors;
        while (first >= 0) {
            ancestors.insert(first);
            first = readCommit(first).parent;
        }
        while (second >= 0) {
            if (ancestors.count(second) != 0) {
                return second;
            }
            second = readCommit(second).parent;
        }
        return -1;
    }

    static void printLineDiff(const std::string& file, const std::string& before,
                              const std::string& after) {
        std::cout << "Changes in " << file << ":\n";
        std::istringstream oldLines(before);
        std::istringstream newLines(after);
        std::vector<std::string> oldValues;
        std::vector<std::string> newValues;
        std::string line;
        while (std::getline(oldLines, line)) {
            oldValues.push_back(line);
        }
        while (std::getline(newLines, line)) {
            newValues.push_back(line);
        }

        const size_t count = std::max(oldValues.size(), newValues.size());
        for (size_t index = 0; index < count; ++index) {
            const std::string oldValue = index < oldValues.size() ? oldValues[index] : "";
            const std::string newValue = index < newValues.size() ? newValues[index] : "";
            if (oldValue != newValue) {
                std::cout << "  line " << index + 1 << "\n";
                if (!oldValue.empty()) {
                    std::cout << "- " << oldValue << "\n";
                }
                if (!newValue.empty()) {
                    std::cout << "+ " << newValue << "\n";
                }
            }
        }
    }

    void restoreSnapshot(int id) const {
        const fs::path snapshot = commitsPath() / std::to_string(id) / "files";
        if (!fs::exists(snapshot)) {
            return;
        }
        std::error_code error;
        const Snapshot currentFiles = readSnapshot(currentCommit());
        const Snapshot targetFiles = readSnapshot(id);
        for (const auto& file : currentFiles) {
            if (targetFiles.count(file.first) == 0) {
                fs::remove(root_ / file.first, error);
            }
        }
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(snapshot)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const fs::path relative = fs::relative(entry.path(), snapshot, error);
            const fs::path destination = root_ / relative;
            fs::create_directories(destination.parent_path(), error);
            fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, error);
        }
    }

    static std::string timestamp() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        std::ostringstream output;
        output << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
        return output.str();
    }

    static bool validBranchName(const std::string& name) {
        if (name.empty()) {
            return false;
        }
        for (const char character : name) {
            if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '_')) {
                return false;
            }
        }
        return true;
    }
};

void printHelp() {
    std::cout << "Commands:\n"
              << "  init                 Create a MiniGit repository\n"
              << "  add <file>           Stage a file\n"
              << "  rm <file>            Remove and stage a file deletion\n"
              << "  status               Show branch and staged files\n"
              << "  commit -m <message>  Save a persistent snapshot\n"
              << "  log                  Show commit history\n"
              << "  diff <id> <id>       Compare two commits\n"
              << "  branch <name>        Create a branch\n"
              << "  checkout <name>      Switch branches and restore files\n"
              << "  merge <name>         Merge a branch into the current branch\n"
              << "  help                 Show this help\n"
              << "  exit                 Quit MiniGit\n";
}

int main() {
    Repository repository(fs::current_path());
    std::cout << "MiniGit 1.0\nType 'help' for commands.\n";

    std::string line;
    while (std::cout << "minigit> " && std::getline(std::cin, line)) {
        std::istringstream command(line);
        std::string name;
        command >> name;

        if (name.empty()) {
            continue;
        }
        if (name == "exit" || name == "quit") {
            break;
        }
        if (name == "help") {
            printHelp();
        } else if (name == "init") {
            repository.initialize();
        } else if (name == "add") {
            std::string file;
            command >> file;
            if (file.empty()) {
                std::cout << "Usage: add <file>\n";
            } else {
                repository.add(file);
            }
        } else if (name == "rm") {
            std::string file;
            command >> file;
            if (file.empty()) {
                std::cout << "Usage: rm <file>\n";
            } else {
                repository.removeFile(file);
            }
        } else if (name == "status") {
            repository.status();
        } else if (name == "commit") {
            std::string option;
            command >> option;
            std::string message;
            std::getline(command, message);
            if (!message.empty() && message.front() == ' ') {
                message.erase(0, 1);
            }
            if (option != "-m") {
                std::cout << "Usage: commit -m <message>\n";
            } else {
                repository.commit(message);
            }
        } else if (name == "log") {
            repository.log();
        } else if (name == "branch") {
            std::string branch;
            command >> branch;
            if (branch.empty()) {
                std::cout << "Usage: branch <name>\n";
            } else {
                repository.createBranch(branch);
            }
        } else if (name == "checkout") {
            std::string branch;
            command >> branch;
            if (branch.empty()) {
                std::cout << "Usage: checkout <name>\n";
            } else {
                repository.checkout(branch);
            }
        } else if (name == "diff") {
            int first = -1;
            int second = -1;
            if (!(command >> first >> second)) {
                std::cout << "Usage: diff <commit-id> <commit-id>\n";
            } else {
                repository.diff(first, second);
            }
        } else if (name == "merge") {
            std::string branch;
            command >> branch;
            if (branch.empty()) {
                std::cout << "Usage: merge <branch>\n";
            } else {
                repository.merge(branch);
            }
        } else {
            std::cout << "Unknown command: " << name << "\n";
        }
    }
    return 0;
}
