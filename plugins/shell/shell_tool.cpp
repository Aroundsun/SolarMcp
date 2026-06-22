#include "plugins/shell/shell_tool.h"

#include "mcp/common/types.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace mcp {

namespace {

constexpr int kPollIntervalMs = 100;

} // namespace

ShellTool::ShellTool(int timeout_sec,
                     std::vector<std::string> allowed_shells,
                     size_t max_output_bytes)
    : default_timeout_sec_(timeout_sec)
    , allowed_shells_(std::move(allowed_shells))
    , max_output_bytes_(max_output_bytes)
{}

std::string ShellTool::name() const {
    return "shell";
}

std::string ShellTool::description() const {
    return "Execute a shell command and return stdout, stderr and exit code";
}

nlohmann::json ShellTool::inputSchema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Shell command to execute"}
            }},
            {"timeout_seconds", {
                {"type", "integer"},
                {"description", "Timeout in seconds (default: " +
                                  std::to_string(default_timeout_sec_) + ")"}
            }}
        }},
        {"required", nlohmann::json::array({"command"})}
    };
}

Result ShellTool::execute(const nlohmann::json& params, Context& /*ctx*/) {
    if (!params.contains("command") || !params["command"].is_string()) {
        return Result::err(ErrorCodes::kInvalidParams,
                           "Missing required parameter: 'command'");
    }

    std::string command = params["command"].get<std::string>();
    if (command.empty()) {
        return Result::err(ErrorCodes::kInvalidParams, "Command cannot be empty");
    }

    int timeout = default_timeout_sec_;
    if (params.contains("timeout_seconds") && params["timeout_seconds"].is_number()) {
        timeout = params["timeout_seconds"].get<int>();
        if (timeout <= 0) {
            return Result::err(ErrorCodes::kInvalidParams,
                               "timeout_seconds must be positive");
        }
    }

    auto output = runCommand(command, timeout, max_output_bytes_);

    nlohmann::json content = {
        {"stdout", std::move(output.stdout_data)},
        {"stderr", std::move(output.stderr_data)},
        {"exit_code", output.exit_code}
    };

    content["status"] = (output.exit_code == 0) ? "success" : "error";
    return Result::ok(std::move(content));
}

ShellTool::CommandOutput ShellTool::runCommand(const std::string& command,
                                                int timeout_seconds,
                                                size_t max_bytes) const {
    CommandOutput result;

    std::string shell_path;
    for (const auto& allowed : allowed_shells_) {
        if (::access(allowed.c_str(), X_OK) == 0) {
            shell_path = allowed;
            break;
        }
    }
    if (shell_path.empty()) {
        result.stderr_data = "No allowed shell found in whitelist";
        return result;
    }

    int stdout_pipe[2];
    int stderr_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stderr_pipe) != 0) {
        result.stderr_data = "Failed to create pipes: " +
                             std::string(std::strerror(errno));
        return result;
    }

    for (int fd : {stdout_pipe[0], stderr_pipe[0]}) {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        result.stderr_data = "Failed to fork: " +
                             std::string(std::strerror(errno));
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[0]);
        ::close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        ::setsid();
        ::close(stdout_pipe[0]);
        ::close(stderr_pipe[0]);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stderr_pipe[1], STDERR_FILENO);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);
        ::execl(shell_path.c_str(), shell_path.c_str(), "-c", command.c_str(), nullptr);
        _exit(127);
    }

    ::close(stdout_pipe[1]);
    ::close(stderr_pipe[1]);

    auto readFromPipe = [&](int fd, std::string& dest, bool& open,
                            size_t& budget, bool& truncated) {
        if (!open) {
            return;
        }

        std::array<char, 4096> buf{};
        while (true) {
            ssize_t n = ::read(fd, buf.data(), buf.size());
            if (n > 0) {
                if (budget > 0) {
                    size_t take = std::min(static_cast<size_t>(n), budget);
                    dest.append(buf.data(), take);
                    budget -= take;
                    if (take < static_cast<size_t>(n)) {
                        truncated = true;
                    }
                } else {
                    truncated = true;
                }
            } else if (n == 0) {
                open = false;
                break;
            } else {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                open = false;
                break;
            }
        }
    };

    auto killProcessGroup = [&]() {
        ::kill(-pid, SIGKILL);
        int status = 0;
        ::waitpid(pid, &status, 0);
        return status;
    };

    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();

    bool stdout_open = true;
    bool stderr_open = true;
    bool child_exited = false;
    bool timed_out = false;
    bool truncated = false;
    bool wait_failed = false;
    int status = 0;
    size_t budget = max_bytes;

    while (true) {
        if (!child_exited) {
            if (timeout_seconds > 0) {
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - start).count();
                if (elapsed_ms >= static_cast<long long>(timeout_seconds) * 1000LL) {
                    timed_out = true;
                    status = killProcessGroup();
                    child_exited = true;
                }
            }

            if (!child_exited) {
                pid_t waited = ::waitpid(pid, &status, WNOHANG);
                if (waited == pid) {
                    child_exited = true;
                } else if (waited < 0 && errno != EINTR) {
                    wait_failed = true;
                    result.stderr_data = "waitpid failed: " +
                                         std::string(std::strerror(errno));
                    status = killProcessGroup();
                    child_exited = true;
                }
            }
        }

        struct pollfd fds[2];
        int nfds = 0;
        if (stdout_open) {
            fds[nfds++] = {stdout_pipe[0], POLLIN, 0};
        }
        if (stderr_open) {
            fds[nfds++] = {stderr_pipe[0], POLLIN, 0};
        }

        if (child_exited && nfds == 0) {
            break;
        }

        int poll_timeout_ms = kPollIntervalMs;
        if (timeout_seconds > 0 && !child_exited) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - start).count();
            long long remain_ms =
                static_cast<long long>(timeout_seconds) * 1000LL - elapsed_ms;
            if (remain_ms <= 0) {
                poll_timeout_ms = 0;
            } else {
                poll_timeout_ms = static_cast<int>(
                    std::min<long long>(kPollIntervalMs, remain_ms));
            }
        } else if (child_exited) {
            poll_timeout_ms = 0;
        }

        if (nfds > 0) {
            int pr = ::poll(fds, nfds, poll_timeout_ms);
            if (pr < 0 && errno != EINTR) {
                result.stderr_data = "poll failed: " +
                                     std::string(std::strerror(errno));
                if (!child_exited) {
                    status = killProcessGroup();
                    child_exited = true;
                }
            } else if (pr > 0) {
                for (int i = 0; i < nfds; ++i) {
                    if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                        if (fds[i].fd == stdout_pipe[0]) {
                            readFromPipe(stdout_pipe[0], result.stdout_data,
                                         stdout_open, budget, truncated);
                        } else if (fds[i].fd == stderr_pipe[0]) {
                            readFromPipe(stderr_pipe[0], result.stderr_data,
                                         stderr_open, budget, truncated);
                        }
                    }
                }
            }
        } else if (!child_exited) {
            ::usleep(static_cast<useconds_t>(poll_timeout_ms) * 1000U);
        }

        if (child_exited && !stdout_open && !stderr_open) {
            break;
        }
    }

    ::close(stdout_pipe[0]);
    ::close(stderr_pipe[0]);

    if (truncated) {
        result.stderr_data +=
            "\n[TRUNCATED] Combined output exceeded " +
            std::to_string(max_bytes) + " bytes";
    }
    if (timed_out) {
        result.exit_code = -1;
        result.stderr_data += "\n[TIMEOUT] Command exceeded " +
                              std::to_string(timeout_seconds) + "s limit";
    } else if (wait_failed) {
        result.exit_code = -1;
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = -1;
        result.stderr_data += "\n[SIGNAL] Killed by signal " +
                              std::to_string(WTERMSIG(status));
    }

    return result;
}

} // namespace mcp
