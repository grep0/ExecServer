#ifndef _UTIL_H
#define _UTIL_H

/* Interface to OS-dependant things */

#if defined(_WINDOWS)
  #include "win_utsname.h"
#else
  #include <sys/utsname.h>
#endif

#define DEFAULT_PORT 5840

struct configuration {
    const char* start_dir;
    int listen_port;
};

const struct configuration *cfg(void);

void clear_error(void);
int get_os_error(const char** pdesc);

int rmdir_recursive(const char* dirname);

int pkill(int pid);
int pwait(int pid, unsigned seconds);

int pspawn(const char* const* argv, const char* const* envp, const char* cwd,
           const char* fstdin, const char* fstdout, const char* fstderr);

#endif
