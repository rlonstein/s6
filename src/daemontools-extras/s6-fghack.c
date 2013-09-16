/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include "strerr2.h"
#include "allreadwrite.h"
#include "djbunix.h"

#define USAGE "s6-fghack prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  int p[2] ;
  int pcoe[2] ;
  int pid ;
  char dummy ;
  PROG = "s6-fghack" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (pipe(p) < 0) strerr_diefu1sys(111, "create hackpipe") ;
  if (pipe(pcoe) < 0) strerr_diefu1sys(111, "create coepipe") ;

  switch (pid = fork())
  {
    case -1 : strerr_diefu1sys(111, "fork") ;
    case 0 :
    {
      int i = 0 ;
      fd_close(p[0]) ;
      fd_close(pcoe[0]) ;
      if (coe(pcoe[1]) < 0) _exit(111) ;
      for (; i < 30 ; i++) dup(p[1]) ; /* hack. gcc's warning is justified. */
      pathexec_run(argv[1], argv+1, envp) ;
      i = errno ;
      if (fd_write(pcoe[1], "", 1) < 1) _exit(111) ;
      _exit(i) ;
    }
  }

  fd_close(p[1]) ;
  fd_close(pcoe[1]) ;

  switch (fd_read(pcoe[0], &dummy, 1))
  {
    case -1 : strerr_diefu1sys(111, "read on coepipe") ;
    case 1 :
    {
      int wstat ;
      if (wait_pid(&wstat, pid) < 0) strerr_diefu1sys(111, "wait_pid") ;
      errno = wait_exitcode(wstat) ;
      strerr_dieexec(111, argv[1]) ;
    }
  }

  fd_close(pcoe[0]) ;

  p[1] = fd_read(p[0], &dummy, 1) ;
  if (p[1] < 0) strerr_diefu1sys(111, "read on hackpipe") ;
  if (p[1]) strerr_dief2x(102, argv[1], " wrote on hackpipe") ;

  {
    int wstat ;
    if (wait_pid(&wstat, pid) < 0) strerr_diefu1sys(111, "wait_pid") ;
    if (wait_crashed(wstat)) strerr_dief2x(111, argv[2], " crashed") ;
    return wait_exitcode(wstat) ;
  }
}
