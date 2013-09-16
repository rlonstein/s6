/* ISC license. */

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "sgetopt.h"
#include "fmtscan.h"
#include "uint16.h"
#include "strerr2.h"
#include "tai.h"
#include "iopause.h"
#include "djbunix.h"
#include "sig.h"
#include "selfpipe.h"
#include "ftrigr.h"

#define USAGE "s6-ftrig-listen1 [ -t timeout ] fifodir regexp prog..."

static void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
    case 0 : return ;
    case SIGCHLD : wait_reap() ; break ;
    default : strerr_dief1x(101, "unexpected data in selfpipe") ;
  }
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  struct taia deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  int pid ;
  uint16 id ;
  PROG = "s6-ftrig-listen1" ;
  {
    unsigned long t = 0 ;
    for (;;)
    {
      register int opt = subgetopt(argc, argv, "t:") ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (ulong0_scan(subgetopt_here.arg, &t)) break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    if (t) taia_from_millisecs(&tto, t) ;
    else tto = infinitetto ;
    argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  }
  if (argc < 3) strerr_dieusage(100, USAGE) ;

  taia_now_g() ;
  taia_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  id = ftrigr_subscribe_g(&a, argv[0], argv[1], 0, &deadline) ;
  if (!id) strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;

  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore") ;
  x[1].fd = ftrigr_fd(&a) ;

  pid = fork() ;
  switch (pid)
  {
    case -1 : strerr_diefu1sys(111, "fork") ;
    case 0  :
    {
      PROG = "s6-ftrig-listen1 (child)" ;
      pathexec_run(argv[2], argv+2, envp) ;
      strerr_dieexec(111, argv[2]) ;
    }
  }

  for (;;)
  {
    char dummy ;
    register int r = ftrigr_check(&a, id, &dummy) ;
    if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
    if (r) break ;
    r = iopause_g(x, 2, &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r)
    {
      errno = ETIMEDOUT ;
      strerr_diefu1sys(1, "get expected event") ;
    }
    if (x[0].revents & IOPAUSE_READ) handle_signals() ;
    if (x[1].revents & IOPAUSE_READ)
    {
      if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
    }
  }

  return 0 ;
}
