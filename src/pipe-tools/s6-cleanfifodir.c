/* ISC license. */

#include "strerr2.h"
#include "ftrigw.h"

#define USAGE "s6-cleanfifodir fifodir"

int main (int argc, char const *const *argv)
{
  PROG = "s6-cleanfifodir" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if (!ftrigw_clean(argv[1]))
    strerr_diefu2sys(111, "clean up fifodir at ", argv[1]) ;
  return 0 ;
}
