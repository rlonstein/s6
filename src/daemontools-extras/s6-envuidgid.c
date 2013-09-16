/* ISC license. */

#include <sys/types.h>
#include <pwd.h>
#include "env.h"
#include "strerr2.h"
#include "fmtscan.h"
#include "djbunix.h"

#define USAGE "s6-envuidgid acctname prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  char fmt[UINT_FMT] ;
  struct passwd *pw ;
  PROG = "s6-envuidgid" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;

  pw = getpwnam(argv[1]) ;
  if (!pw) strerr_dief2x(1, "unknown account: ", argv[1]) ;

  fmt[uint_fmt(fmt, pw->pw_gid)] = 0 ;
  if (!pathexec_env("GID", fmt))
    strerr_diefu1sys(111, "update env") ;
  fmt[uint_fmt(fmt, pw->pw_uid)] = 0 ;
  if (!pathexec_env("UID", fmt))
    strerr_diefu1sys(111, "update env") ;

  pathexec_fromenv(argv+2, envp, env_len(envp)) ;
  strerr_dieexec(111, argv[2]) ;
}
