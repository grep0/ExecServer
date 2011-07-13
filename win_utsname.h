#ifndef _WIN_UTSNAME_H
#define _WIN_UTSNAME_H

/* a simple replacement of <sys/utsname.h> for WinXX */

#define SYS_NMLN 256

struct utsname {
   char    sysname[SYS_NMLN];
   char    nodename[SYS_NMLN];
   char    release[SYS_NMLN];
   char    version[SYS_NMLN];
   char    machine[SYS_NMLN];
};
int uname(struct utsname *name);

#endif
