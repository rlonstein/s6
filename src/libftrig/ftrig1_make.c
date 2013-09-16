/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>      /* for rename() */
#include <errno.h>
#include "bytestr.h"
#include "tai.h"
#include "stralloc.h"
#include "djbunix.h"
#include "random.h"
#include "ftrig1.h"

int ftrig1_make (ftrig1 *f, char const *path)
{
  ftrig1 ff = FTRIG1_ZERO ;
  unsigned int pathlen = str_len(path) ;
  int e = 0 ;
  char tmp[pathlen + 46 + FTRIG1_PREFIXLEN] ;
  
  byte_copy(tmp, pathlen, path) ;
  tmp[pathlen] = '/' ; tmp[pathlen+1] = '.' ;
  byte_copy(tmp + pathlen + 2, FTRIG1_PREFIXLEN, FTRIG1_PREFIX) ;
  tmp[pathlen + 2 + FTRIG1_PREFIXLEN] = ':' ;
  if (!timestamp(tmp + pathlen + 3 + FTRIG1_PREFIXLEN)) return 0 ;
  tmp[pathlen + 28 + FTRIG1_PREFIXLEN] = ':' ;
  if (random_name(tmp + pathlen + 29 + FTRIG1_PREFIXLEN, 16) < 16) return 0 ;
  tmp[pathlen + 45 + FTRIG1_PREFIXLEN] = 0 ;
  
  {
    mode_t m = umask(0) ;
    if (fifo_make(tmp, S_IRUSR|S_IWUSR|S_IWGRP|S_IWOTH) == -1)
    {
      umask(m) ;
      return 0 ;
    }
    umask(m) ;
  }

  if (!stralloc_catb(&ff.name, tmp, pathlen+1)) { e = errno ; goto err0 ; }
  if (!stralloc_catb(&ff.name, tmp + pathlen + 2, FTRIG1_PREFIXLEN + 44))
  {
    e = errno ; goto err1 ;
  }
  ff.fd = open_read(tmp) ;
  if (ff.fd == -1) { e = errno ; goto err1 ; }
  ff.fdw = open_write(tmp) ;
  if (ff.fdw == -1) { e = errno ; goto err2 ; }
  if (rename(tmp, ff.name.s) == -1) goto err3 ;
  *f = ff ;
  return 1 ;

 err3:
  e = errno ;
  fd_close(ff.fdw) ;
 err2:
  fd_close(ff.fd) ;
 err1:
  stralloc_free(&ff.name) ;
 err0:
  unlink(tmp) ;
  errno = e ;
  return 0 ;
}
