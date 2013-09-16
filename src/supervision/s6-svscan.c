/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "allreadwrite.h"
#include "bytestr.h"
#include "sgetopt.h"
#include "fmtscan.h"
#include "strerr2.h"
#include "tai.h"
#include "iopause.h"
#include "djbunix.h"
#include "direntry.h"
#include "sig.h"
#include "selfpipe.h"
#include "environ.h"
#include "s6-config.h"
#include "s6-supervise.h"

#define USAGE "s6-svscan [ -t timeout ] [ dir ]"

#define FINISH_PROG S6_SVSCAN_CTLDIR "/finish"
#define CRASH_PROG S6_SVSCAN_CTLDIR "/crash"

#define DIR_RETRY_TIMEOUT 3
#define CHECK_RETRY_TIMEOUT 4
#define MAXSERVICES 500

#define KILLMODE_NOKILL 0x01
#define KILLMODE_ACTIVE 0x02
#define KILLMODE_SERVICES 0x04
#define KILLMODE_LOGGERS 0x08

struct svinfo
{
  dev_t dev ;
  ino_t ino ;
  struct taia restartafter[2] ;
  int pid[2] ;
  int p[2] ;
  unsigned int flagactive : 1 ;
  unsigned int flaglog : 1 ;
} ;
#define SVINFO_ZERO { -1, -1, { TAIA_ZERO, TAIA_ZERO }, { 0, 0 }, { -1, -1 }, 0, 0, 0 } ;

static struct svinfo services[MAXSERVICES] ;

static unsigned int n = 0 ;
static struct taia deadline, defaulttimeout ;
static char const *finish_arg = "reboot" ;
static int wantreap = 1 ;
static int wantscan = 1 ;
static int cont = 1 ;
static unsigned int finalkillmode = 0 ;

static void panicnosp (char const *) gccattr_noreturn ;
static void panicnosp (char const *errmsg)
{
  char const *eargv[2] = { CRASH_PROG, 0 } ;
  strerr_warnwu1sys(errmsg) ;
  strerr_warnw2x("executing into ", eargv[0]) ;
  execve(eargv[0], (char *const *)eargv, (char *const *)environ) ;
 /* and if that execve fails, screw it and just die */
  strerr_dieexec(111, eargv[0]) ;
}

static void panic (char const *) gccattr_noreturn ;
static void panic (char const *errmsg)
{
  int e = errno ;
  selfpipe_finish() ;
  errno = e ;
  panicnosp(errmsg) ;
}

static void killthem (unsigned int mode)
{
  register unsigned int i = 0 ;
  if (mode & KILLMODE_NOKILL) return ;
  for (; i < n ; i++)
  {
    if (!(mode & KILLMODE_ACTIVE) && services[i].flagactive)
      continue ;
    if (services[i].pid[0])
      kill(services[i].pid[0], (mode & KILLMODE_SERVICES) ? SIGTERM : SIGHUP) ;
    if (services[i].flaglog && services[i].pid[1])
      kill(services[i].pid[1], (mode & KILLMODE_LOGGERS) ? SIGTERM : SIGHUP) ;
  }
}

static void handle_signals (void)
{
  for (;;)
  {
    switch (selfpipe_read())
    {
      case -1 : panic("selfpipe_read") ;
      case 0 : return ;
      case SIGCHLD : wantreap = 1 ; break ;
      case SIGALRM : wantscan = 1 ; break ;
      case SIGTERM : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; break ;
      case SIGHUP : cont = 0 ; finalkillmode = KILLMODE_ACTIVE ; break ;
      case SIGQUIT : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES | KILLMODE_LOGGERS ; break ;
      case SIGABRT : cont = 0 ; finalkillmode = KILLMODE_NOKILL ; break ;
      case SIGINT : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "reboot" ; break ;
    }
  }
}

static void handle_control (int fd)
{
  for (;;)
  {
    char c ;
    int r = sanitize_read(fd_read(fd, &c, 1)) ;
    if (r == -1) panic("read control pipe") ;
    else if (!r) break ;
    else switch (c)
    {
      case 'p' : finish_arg = "poweroff" ; break ;
      case 'h' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE ; return ;
      case 'r' : finish_arg = "reboot" ; break ;
      case 'a' : wantscan = 1 ; break ;
      case 't' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; return ;
      case 's' : finish_arg = "halt" ; break ;
      case 'z' : wantreap = 1 ; break ;
      case 'b' : cont = 0 ; finalkillmode = KILLMODE_NOKILL ; return ;
      case 'n' : killthem(KILLMODE_SERVICES) ; break ;
      case 'N' : killthem(KILLMODE_SERVICES | KILLMODE_LOGGERS) ; break ;
      case 'i' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "reboot" ; return ;
      case 'q' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES | KILLMODE_LOGGERS ; return ;
      case '0' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "halt" ; return ;
      case '6' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "reboot" ; return ;
      case '7' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "poweroff" ; return ;
      case '8' : cont = 0 ; finalkillmode = KILLMODE_ACTIVE | KILLMODE_SERVICES ; finish_arg = "other" ; return ;
      default :
      {
        char s[2] = { c, 0 } ;
        strerr_warnw2x("received unknown control command: ", s) ;
      }
    }
  }
}


/* First essential function: the reaper.
   s6-svscan must wait() for all children,
   including ones it doesn't know it has.
   Dead active services are flagged to be restarted in 1 second. */

static void reap (void)
{
  struct taia nextscan ;
  taia_addsec_g(&nextscan, 1) ;
  wantreap = 0 ;
  for (;;)
  {
    int wstat ;
    int r = wait_nohang(&wstat) ;
    if (r < 0)
      if (errno != ECHILD) panic("wait_nohang") ;
      else break ;
    else if (!r) break ;
    else
    {
      register unsigned int i = 0 ;
      for (; i < n ; i++)
      {
        if (services[i].pid[0] == r)
        {
          services[i].pid[0] = 0 ;
          services[i].restartafter[0] = nextscan ;
          break ;
        }
        else if (services[i].pid[1] == r)
        {
          services[i].pid[1] = 0 ;
          services[i].restartafter[1] = nextscan ;
          break ;
        }
      }
      if (i == n) continue ;
      if (services[i].flagactive)
      {
        if (taia_less(&nextscan, &deadline)) deadline = nextscan ;
      }
      else
      {
        if (services[i].flaglog)
        {
 /*
    BLACK MAGIC:
     - we need to close the pipe early:
       * as soon as the writer exits so the logger can exit on EOF
       * or as soon as the logger exits so the writer can crash on EPIPE
     - but if the same service gets reactivated before the second
       supervise process exits, ouch: we've lost the pipe
     - so we can't reuse the same service even if it gets reactivated
     - so we're marking a dying service with a closed pipe
     - if the scanner sees a service with p[0] = -1 it won't flag
       it as active (and won't restart the dead supervise)
     - but if the service gets reactivated we want it to restart
       as soon as the 2nd supervise process dies
     - so the scanner marks such a process with p[0] = -2
     - and the reaper triggers a scan when it finds a -2.
 */
          if (services[i].p[0] >= 0)
          {
            fd_close(services[i].p[1]) ; services[i].p[1] = -1 ;
            fd_close(services[i].p[0]) ; services[i].p[0] = -1 ;
          }
          else if (services[i].p[0] == -2) wantscan = 1 ;
        }
        if (!services[i].pid[0] && !services[i].pid[1])
          services[i] = services[--n] ;
      }
    }
  }
}


/* Second essential function: the scanner.
   It monitors the service directories and spawns a supervisor
   if needed. */

static void trystart (unsigned int i, char const *name, int islog)
{
  int pid = fork() ;
  switch (pid)
  {
    case -1 :
      taia_addsec_g(&services[i].restartafter[islog], CHECK_RETRY_TIMEOUT) ;
      strerr_warnwu2sys("fork for ", name) ;
      return ;
    case 0 :
    {
      char const *cargv[3] = { "s6-supervise", name, 0 } ;
      PROG = "s6-svscan (child)" ;
      selfpipe_finish() ;
      if (services[i].flaglog)
        if (fd_move(!islog, services[i].p[!islog]) == -1)
          strerr_diefu2sys(111, "set fds for ", name) ;
      pathexec_run(S6_BINPREFIX "s6-supervise", cargv, (char const **)environ) ;
      strerr_dieexec(111, S6_BINPREFIX "s6-supervise") ;
    }
  }
  services[i].pid[islog] = pid ;
}

static void retrydirlater (void)
{
  struct taia a ;
  taia_addsec_g(&a, DIR_RETRY_TIMEOUT) ;
  if (taia_less(&a, &deadline)) deadline = a ;
}

static void check (char const *name)
{
  struct stat st ;
  unsigned int namelen ;
  unsigned int i = 0 ;
  if (name[0] == '.') return ;
  if (stat(name, &st) == -1)
  {
    strerr_warnwu2sys("stat ", name) ;
    retrydirlater() ;
    return ;
  }
  if (!S_ISDIR(st.st_mode)) return ;
  namelen = str_len(name) ;
  for (; i < n ; i++) if ((services[i].ino == st.st_ino) && (services[i].dev == st.st_dev)) break ;
  if (i < n)
  {
    if (services[i].flaglog && (services[i].p[0] < 0))
    {
     /* See BLACK MAGIC above. */
      services[i].p[0] = -2 ;
      return ;
    }
  }
  else
  {
    if (n >= MAXSERVICES)
    {
      strerr_warnwu3x("start supervisor for ", name, ": too many services") ;
      return ;
    }
    else
    {
      struct stat su ;
      char tmp[namelen + 5] ;
      byte_copy(tmp, namelen, name) ;
      byte_copy(tmp + namelen, 5, "/log") ;
      if (stat(tmp, &su) < 0)
        if (errno == ENOENT) services[i].flaglog = 0 ;
        else
        {
          strerr_warnwu2sys("stat ", tmp) ;
          retrydirlater() ;
          return ;
        }
      else if (!S_ISDIR(su.st_mode))
        services[i].flaglog = 0 ;
      else
      {
        if (pipecoe(services[i].p) < 0)
        {
          strerr_warnwu1sys("pipecoe") ;
          retrydirlater() ;
          return ;
        }
        services[i].flaglog = 1 ;
      }
      services[i].ino = st.st_ino ;
      services[i].dev = st.st_dev ;
      taia_copynow(&services[i].restartafter[0]) ;
      taia_copynow(&services[i].restartafter[1]) ;
      services[i].pid[0] = 0 ;
      services[i].pid[1] = 0 ;
      n++ ;
    }
  }
  
  services[i].flagactive = 1 ;

  if (services[i].flaglog && !services[i].pid[1])
  {
    if (!taia_future(&services[i].restartafter[1]))
    {
      char tmp[namelen + 5] ;
      byte_copy(tmp, namelen, name) ;
      byte_copy(tmp + namelen, 5, "/log") ;
      trystart(i, tmp, 1) ;
    }
    else if (taia_less(&services[i].restartafter[1], &deadline))
      deadline = services[i].restartafter[1] ;
  }

  if (!services[i].pid[0])
  {
    if (!taia_future(&services[i].restartafter[0]))
      trystart(i, name, 0) ;
    else if (taia_less(&services[i].restartafter[0], &deadline))
      deadline = services[i].restartafter[0] ;
  }
}

static void scan (void)
{
  DIR *dir = opendir(".") ;
  wantscan = 0 ;
  if (!dir)
  {
    strerr_warnwu1sys("opendir .") ;
    retrydirlater() ;
    return ;
  }
  {
    register unsigned int i = 0 ;
    for (; i < n ; i++) services[i].flagactive = 0 ;
  }
  for (;;)
  {
    direntry *d ;
    errno = 0 ;
    d = readdir(dir) ;
    if (!d) break ;
    check(d->d_name) ;
  }
  if (errno)
  {
    strerr_warnwu1sys("readdir .") ;
    retrydirlater() ;
  }
  dir_close(dir) ;
}


int main (int argc, char const *const *argv)
{
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  PROG = "s6-svscan" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 5000 ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (uint0_scan(l.arg, &t)) break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    if (t) taia_from_millisecs(&defaulttimeout, t) ;
    else defaulttimeout = infinitetto ;
    argc -= l.ind ; argv += l.ind ;
  }

  /* Init phase.
     If something fails here, we can die, because it means that
     something is seriously wrong with the system, and we can't
     run correctly anyway.
  */

  if (argc && (chdir(argv[0]) < 0)) strerr_diefu1sys(111, "chdir") ;
  x[1].fd = s6_supervise_lock(S6_SVSCAN_CTLDIR) ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "selfpipe_init") ;

  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGCHLD) ;
    sigaddset(&set, SIGALRM) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGHUP) ;
    sigaddset(&set, SIGQUIT) ;
    sigaddset(&set, SIGABRT) ;
    sigaddset(&set, SIGINT) ;
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }

  taia_now_g() ;


  /* Loop phase.
     From now on, we must not die.
     Temporize on recoverable errors, and panic on serious ones. */

  while (cont)
  {
    int r ;
    taia_add_g(&deadline, &defaulttimeout) ;
    if (wantreap) reap() ;
    if (wantscan) scan() ;
    r = iopause_g(x, 2, &deadline) ;
    if (r < 0) panic("iopause") ;
    else if (!r) wantscan = 1 ;
    else
    {
      if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
      {
        errno = EIO ;
        panic("check internal pipes") ;
      }
      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      if (x[1].revents & IOPAUSE_READ) handle_control(x[1].fd) ;
    }
  }


  /* Finish phase. */

  killthem(finalkillmode) ;
  selfpipe_finish() ;
  reap() ;
  {
    char const *eargv[3] = { FINISH_PROG, finish_arg, 0 } ;
    execve(eargv[0], (char **)eargv, (char *const *)environ) ;
  }
  panicnosp("exec finish script " FINISH_PROG) ;
}
