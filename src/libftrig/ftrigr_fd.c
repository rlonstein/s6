/* ISC license. */

#include "skaclient.h"
#include "ftrigr.h"

int ftrigr_fd (ftrigr const *a)
{
  return skaclient2_fd(&a->connection) ;
}
