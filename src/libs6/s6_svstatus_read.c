/* ISC license. */

#include "bytestr.h"
#include "djbunix.h"
#include "s6-supervise.h"

int s6_svstatus_read (char const *dir, s6_svstatus_t_ref status)
{
  unsigned int n = str_len(dir) ;
  char pack[S6_SVSTATUS_SIZE] ;
  char tmp[n + 2 + sizeof(S6_SVSTATUS_FILENAME)] ;
  byte_copy(tmp, n, dir) ;
  byte_copy(tmp + n, 2 + sizeof(S6_SVSTATUS_FILENAME), "/" S6_SVSTATUS_FILENAME) ;
  if (openreadnclose(tmp, pack, S6_SVSTATUS_SIZE) < S6_SVSTATUS_SIZE) return 0 ;
  s6_svstatus_unpack(pack, status) ;
  return 1 ;
}
