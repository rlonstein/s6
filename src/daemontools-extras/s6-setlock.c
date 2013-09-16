/* ISC license. */

#include <unistd.h>
#include <errno.h>
#include "sgetopt.h"
#include "strerr2.h"
#include "djbunix.h"

#define USAGE "s6-setlock [ -r | -w ] [ -n | -N ] lockfile prog..."
#define dieusage() strerr_dieusage(100, USAGE)

typedef int lockfunc_t (int) ;
typedef lockfunc_t *lockfunc_t_ref ;

static lockfunc_t_ref f[2][2] = { { &lock_sh, &lock_shnb }, { &lock_ex, &lock_exnb } } ;

int main (int argc, char const *const *argv, char const *const *envp)
{
  int fd ;
  int nb = 0, ex = 1 ;
  PROG = "s6-setlock" ;
  for (;;)
  {
    register int opt = subgetopt(argc, argv, "nNrw") ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'n' : nb = 1 ; break ;
      case 'N' : nb = 0 ; break ;
      case 'r' : ex = 0 ; break ;
      case 'w' : ex = 1 ; break ;
      default : dieusage() ;
    }
  }
  argc -= subgetopt_here.ind ; argv += subgetopt_here.ind ;
  if (argc < 2) dieusage() ;

  fd = open_create(argv[0]) ;
  if (fd == -1) strerr_diefu2sys(111, "open_append ", argv[0]) ;
  if ((*f[ex][nb])(fd) == -1) strerr_diefu2sys(1, "lock ", argv[0]) ;
  pathexec_run(argv[1], argv+1, envp) ;
  strerr_dieexec(111, argv[1]) ;
}
