/* ISC license. */

#include <errno.h>
#include "allreadwrite.h"
#include "sgetopt.h"
#include "fmtscan.h"
#include "uint16.h"
#include "strerr2.h"
#include "tai.h"
#include "ftrigr.h"

#define USAGE "s6-ftrig-wait [ -t timeout ] fifodir regexp"

int main (int argc, char const *const *argv)
{
  struct taia deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  uint16 id ;
  char pack[2] = " \n" ;
  PROG = "s6-ftrig-wait" ;
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
  if (argc < 2) strerr_dieusage(100, USAGE) ;

  taia_now_g() ;
  taia_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;
  id = ftrigr_subscribe_g(&a, argv[0], argv[1], 0, &deadline) ;
  if (!id) strerr_diefu4sys(111, "subscribe to ", argv[0], " with regexp ", argv[1]) ;
  if (ftrigr_wait_or_g(&a, &id, 1, &deadline, &pack[0]) == -1)
    strerr_diefu2sys((errno == ETIMEDOUT) ? 1 : 111, "match regexp on ", argv[1]) ;
  if (allwrite(1, pack, 2) < 2) strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}
