/* ISC license. */

#include <sys/types.h>
#include <pwd.h>
#include "strerr2.h"
#include "djbunix.h"

#define USAGE "s6-setuidgid acctname prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  struct passwd *pw ;
  PROG = "s6-setuidgid" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  pw = getpwnam(argv[1]) ;
  if (!pw) strerr_dief2x(1, "unknown account: ", argv[1]) ;
  if (prot_gid(pw->pw_gid) == -1) strerr_diefu1sys(111, "setgid") ;
  if (prot_uid(pw->pw_uid) == -1) strerr_diefu1sys(111, "setuid") ;
  pathexec_run(argv[2], argv+2, envp) ;
  strerr_dieexec(111, argv[2]) ;
}
