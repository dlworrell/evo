#define _POSIX_C_SOURCE 200809L

#include "internal/project_provider_probe.h"

#if defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

bool evo_project_provider_probe_bwrap(void)
{
#if defined(__linux__)
    static char *const arguments[] = {
        (char *)"bwrap",
        (char *)"--die-with-parent",
        (char *)"--new-session",
        (char *)"--unshare-user",
        (char *)"--unshare-pid",
        (char *)"--unshare-net",
        (char *)"--ro-bind",
        (char *)"/",
        (char *)"/",
        (char *)"--proc",
        (char *)"/proc",
        (char *)"--dev",
        (char *)"/dev",
        (char *)"--",
        (char *)"/bin/true",
        NULL};
    struct timespec pause = {0, 10000000L};
    pid_t process;
    size_t attempt;
    int status = 0;

    process = fork();
    if (process < 0) {
        return false;
    }
    if (process == 0) {
        const int null_descriptor = open("/dev/null", O_RDWR | O_CLOEXEC);

        if (null_descriptor >= 0) {
            (void)dup2(null_descriptor, STDOUT_FILENO);
            (void)dup2(null_descriptor, STDERR_FILENO);
            if (null_descriptor > STDERR_FILENO) {
                (void)close(null_descriptor);
            }
        }
        execvp(arguments[0], arguments);
        _exit(127);
    }
    for (attempt = 0U; attempt < 200U; attempt += 1U) {
        const pid_t waited = waitpid(process, &status, WNOHANG);

        if (waited == process) {
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }
        if (waited < 0 && errno != EINTR) {
            return false;
        }
        (void)nanosleep(&pause, NULL);
    }
    if (kill(process, SIGKILL) != 0 && errno != ESRCH) {
        return false;
    }
    while (waitpid(process, &status, 0) < 0) {
        if (errno != EINTR) {
            break;
        }
    }
    return false;
#else
    return false;
#endif
}
