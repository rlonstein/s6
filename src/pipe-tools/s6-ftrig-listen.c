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
#include "execline.h"
#include "ftrigr.h"

#define USAGE "s6-ftrig-listen [ -a | -o ] [ -t timeout ] ~fifodir1 ~regexp1 ... ; prog..."
#define dieusage() strerr_dieusage(100, USAGE)

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

int main (int argc, char const **argv, char const *const *envp)
{
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  struct taia deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  int argc1 ;
  unsigned int i = 0 ;
  char or = 0 ;
  PROG = "s6-ftrig-listen" ;
  {
    unsigned long t = 0 ;
    for (;;)
    {
      register int opt = subgetopt(argc, argv, "aot:") ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'a' : or = 0 ; break ;
        case 'o' : or = 1 ; break ;
        case 't' : if (ulong0_scan(subgetopt_here.arg, &t)) break ;
        default : dieusage() ;
      }
    }
    if (t) taia_from_millisecs(&tto, t) ; else tto = infinitetto ;
    argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  }
  if (argc < 2) dieusage() ;
  argc1 = el_semicolon(argv) ;
  if (!argc1 || (argc1 & 1) || (argc == argc1 + 1)) dieusage() ;
  if (argc1 >= argc) strerr_dief1x(100, "unterminated fifodir+regex block") ;
  taia_now_g() ;
  taia_add_g(&deadline, &tto) ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "selfpipe_trap") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "sig_ignore") ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;

  x[1].fd = ftrigr_fd(&a) ;

  {
    int pid = 0 ;
    unsigned int idlen = argc1 >> 1 ;
    uint16 ids[idlen] ;
    for (; i < idlen ; i++)
    {
      ids[i] = ftrigr_subscribe_g(&a, argv[i<<1], argv[(i<<1)+1], 0, &deadline) ;
      if (!ids[i]) strerr_diefu4sys(111, "subscribe to ", argv[i<<1], " with regexp ", argv[(i<<1)+1]) ;
    }

    pid = fork() ;
    switch (pid)
    {
      case -1 : strerr_diefu1sys(111, "fork") ;
      case 0  :
      {
        PROG = "s6-ftrig-listen (child)" ;
        pathexec_run(argv[argc1 + 1], argv + argc1 + 1, envp) ;
        strerr_dieexec(111, argv[argc1 + 1]) ;
      }
    }

    for (;;)
    {
      register int r ;
      i = 0 ;
      while (i < idlen)
      {
        char dummy ;
        r = ftrigr_check(&a, ids[i], &dummy) ;
        if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
        else if (!r) i++ ;
        else if (or) idlen = 0 ;
        else ids[i] = ids[--idlen] ;
      }
      if (!idlen) break ;
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
  }
  return 0 ;
}
