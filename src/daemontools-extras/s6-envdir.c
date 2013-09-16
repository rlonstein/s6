/* ISC license. */

#include <errno.h>
#include "sgetopt.h"
#include "strerr2.h"
#include "stralloc.h"
#include "env.h"
#include "djbunix.h"

#define USAGE "s6-envdir [ -I | -i ] [ -n ] [ -f ] [ -c nullchar ] dir prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  stralloc modifs = STRALLOC_ZERO ;
  subgetopt_t l = SUBGETOPT_ZERO ;
  int insist = 1 ;
  unsigned int options = 0 ;
  char nullis = '\n' ;
  PROG = "s6-envdir" ;
  for (;;)
  {
    register int opt = subgetopt_r(argc, argv, "Iinfc:", &l) ;
    if (opt == -1) break ;
    switch (opt)
    {
      case 'I' : insist = 0 ; break ;
      case 'i' : insist = 1 ; break ;
      case 'n' : options |= SKALIBS_ENVDIR_NOCHOMP ; break ;
      case 'f' : options |= SKALIBS_ENVDIR_VERBATIM ; break ;
      case 'c' : nullis = *l.arg ; break ;
      default : strerr_dieusage(100, USAGE) ;
    }
  }
  argc -= l.ind ; argv += l.ind ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if ((envdir_internal(*argv++, &modifs, options, nullis) < 0) && (insist || (errno != ENOENT)))
    strerr_diefu1sys(111, "envdir") ;
  pathexec_r(argv, envp, env_len(envp), modifs.s, modifs.len) ;
  strerr_dieexec(111, argv[0]) ;
}
