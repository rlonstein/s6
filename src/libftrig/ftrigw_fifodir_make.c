/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "ftrigw.h"

int ftrigw_fifodir_make (char const *path, int gid, int force)
{
  mode_t m = umask(0) ;
  if (mkdir(path, 0700) == -1)
  {
    struct stat st ;
    umask(m) ;
    if (errno != EEXIST) return 0 ;
    if (stat(path, &st) == -1) return 0 ;
    if (st.st_uid != getuid()) return (errno = EACCES, 0) ;
    if (!S_ISDIR(st.st_mode)) return (errno = ENOTDIR, 0) ;
    if (!force) return 1 ;
  }
  else umask(m) ;
  if ((gid >= 0) && (chown(path, -1, gid) == -1)) return 0 ;
  if (chmod(path, (gid >= 0) ? 03730 : 01733) == -1) return 0 ;
  return 1 ;
}
