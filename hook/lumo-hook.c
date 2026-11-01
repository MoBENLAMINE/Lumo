/*
 * lumo-hook — the relay Claude Code (or any agent) runs on each hook event.
 *
 *   lumo-hook [--agent NAME] EVENT      a hook event, JSON on stdin
 *   lumo-hook --statusline              Claude Code's status line command
 *
 * Wire format to the app ($XDG_RUNTIME_DIR/lumo.sock):
 *   line 1: {"agent":"…","event":"…","ppid":N,"pids":[…],"term":"…","len":N}
 *   then exactly `len` bytes of raw payload.
 * For a PermissionRequest the app may answer with the exact line to print on
 * stdout; anything else gets no answer.
 *
 * Hard rule: never block the agent. No socket, a socket owned by someone
 * else, or no answer in time all mean "no opinion": print nothing, exit 0.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define CONNECT_MS 300
#define SEND_MS 2000
#define DECISION_MS 110000
#define MAX_PAYLOAD (8 * 1024 * 1024)
#define MAX_REPLY (1024 * 1024)

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static char *read_all(int fd, size_t *len) {
    size_t cap = 64 * 1024, n = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (n == cap) {
            if (cap >= MAX_PAYLOAD) break;
            cap *= 2;
            char *grown = realloc(buf, cap);
            if (!grown) break;
            buf = grown;
        }
        ssize_t r = read(fd, buf + n, cap - n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        n += (size_t)r;
    }
    *len = n;
    return buf;
}

/* $XDG_RUNTIME_DIR must be ours and closed to everyone else. */
static int socket_path(char *out, size_t cap) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    char fallback[64];
    if (!dir || dir[0] != '/') {
        snprintf(fallback, sizeof fallback, "/run/user/%u", getuid());
        dir = fallback;
    }
    struct stat st;
    if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077))
        return -1;
    int n = snprintf(out, cap, "%s/lumo.sock", dir);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}

static int wait_fd(int fd, short events, long deadline) {
    for (;;) {
        long left = deadline - now_ms();
        if (left <= 0) return 0;
        struct pollfd p = {fd, events, 0};
        int r = poll(&p, 1, (int)left);
        if (r < 0 && errno == EINTR) continue;
        return r > 0;
    }
}

static int connect_app(void) {
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    if (socket_path(addr.sun_path, sizeof addr.sun_path) != 0) return -1;
    long deadline = now_ms() + CONNECT_MS;
    for (;;) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (fd < 0) return -1;
        if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0) {
            struct ucred cred;
            socklen_t len = sizeof cred;
            if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.uid != getuid()) {
                close(fd);
                return -1;
            }
            return fd;
        }
        int err = errno;
        close(fd);
        /* Only a full backlog is worth a retry; anything else means nobody's home. */
        if (err != EAGAIN || now_ms() >= deadline) return -1;
        usleep(15000);
    }
}

static int send_all(int fd, const char *buf, size_t len, long deadline) {
    while (len) {
        if (!wait_fd(fd, POLLOUT, deadline)) return -1;
        ssize_t w = send(fd, buf, len, MSG_NOSIGNAL);
        if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (w <= 0) return -1;
        buf += w;
        len -= (size_t)w;
    }
    return 0;
}

/* Keep only [A-Za-z0-9._-] so the header never needs escaping. */
static void sanitize(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; src && *src && n + 1 < cap; src++) {
        char c = *src;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
            c == '_' || c == '-')
            dst[n++] = c;
    }
    dst[n] = 0;
}

/* The chain of parent processes (shell, terminal, editor…), so the app can
 * bring the session's window forward. Written as a JSON array into `out`. */
static void ancestors(char *out, size_t cap) {
    size_t n = (size_t)snprintf(out, cap, "[");
    int pid = (int)getppid();
    for (int depth = 0; depth < 12 && pid > 1 && n + 16 < cap; depth++) {
        n += (size_t)snprintf(out + n, cap - n, "%s%d", depth ? "," : "", pid);
        char path[64], buf[512];
        snprintf(path, sizeof path, "/proc/%d/stat", pid);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) break;
        ssize_t r = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (r <= 0) break;
        buf[r] = 0;
        /* "pid (comm) state ppid …": comm may hold spaces, so start after the last ')'. */
        char *end = strrchr(buf, ')');
        int ppid = 0;
        if (!end || sscanf(end + 1, " %*c %d", &ppid) != 1) break;
        pid = ppid;
    }
    snprintf(out + n, cap - n, "]");
}

/* Sends header + payload. With wait_ms > 0, returns the app's reply (malloc'd). */
static char *talk(const char *agent, const char *event, const char *payload, size_t len, long wait_ms) {
    int fd = connect_app();
    if (fd < 0) return NULL;
    char a[64], e[64], term[64], pids[256], header[1024];
    sanitize(a, sizeof a, agent);
    sanitize(e, sizeof e, event);
    sanitize(term, sizeof term, getenv("TERM_PROGRAM") ? getenv("TERM_PROGRAM") : getenv("TERM"));
    ancestors(pids, sizeof pids);
    int hn = snprintf(header, sizeof header,
                      "{\"agent\":\"%s\",\"event\":\"%s\",\"ppid\":%d,\"pids\":%s,\"term\":\"%s\",\"len\":%zu}\n", a, e,
                      (int)getppid(), pids, term, len);
    long deadline = now_ms() + SEND_MS;
    char *reply = NULL;
    if (send_all(fd, header, (size_t)hn, deadline) == 0 && send_all(fd, payload, len, deadline) == 0) {
        if (wait_ms > 0) {
            long until = now_ms() + wait_ms;
            size_t cap = 4096, n = 0;
            reply = malloc(cap);
            while (reply && wait_fd(fd, POLLIN, until)) {
                if (n + 1 == cap) {
                    if (cap >= MAX_REPLY) break;
                    char *grown = realloc(reply, cap * 2);
                    if (!grown) break;
                    reply = grown;
                    cap *= 2;
                }
                ssize_t r = recv(fd, reply + n, cap - n - 1, 0);
                if (r < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                if (r <= 0) break;
                n += (size_t)r;
            }
            if (reply) {
                reply[n] = 0;
                if (n == 0) {
                    free(reply);
                    reply = NULL;
                }
            }
        }
    }
    close(fd);
    return reply;
}

/* Runs the user's previous status line (saved by the app) with the same input. */
static void previous_statusline(const char *raw, size_t len) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[1024];
    snprintf(path, sizeof path, "%s/.config/lumo/statusline-previous", home);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;
    size_t n = 0;
    char *cmd = read_all(fd, &n);
    close(fd);
    if (!cmd) return;
    while (n && (cmd[n - 1] == '\n' || cmd[n - 1] == ' ')) n--;
    cmd[n < 1 ? 0 : n] = 0;
    if (n) {
        fflush(stdout);
        FILE *p = popen(cmd, "w");
        if (p) {
            fwrite(raw, 1, len, p);
            pclose(p);
        }
    }
    free(cmd);
}

int main(int argc, char **argv) {
    const char *agent = "";
    const char *event = "";
    int statusline = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--statusline")) statusline = 1;
        else if (!strcmp(argv[i], "--agent") && i + 1 < argc) agent = argv[++i];
        else if (!*event) event = argv[i];
    }

    size_t len = 0;
    char *raw = read_all(STDIN_FILENO, &len);
    if (!raw) return 0;

    if (statusline) {
        /* Plan limits for the usage gauge; only sent when present. */
        if (memmem(raw, len, "\"rate_limits\"", 13)) free(talk(agent, "StatusLine", raw, len, 0));
        previous_statusline(raw, len);
        return 0;
    }

    int waits = !strcmp(event, "PermissionRequest") && (!*agent || !strcmp(agent, "codex"));
    char *reply = talk(agent, event, raw, len, waits ? DECISION_MS : 0);
    if (reply && reply[0] == '{') {
        fputs(reply, stdout);
        if (reply[strlen(reply) - 1] != '\n') fputc('\n', stdout);
    }
    return 0;
}
