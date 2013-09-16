/* ISC license. */

#include <errno.h>
#include "uint16.h"
#include "uint32.h"
#include "bytestr.h"
#include "tai.h"
#include "gensetdyn.h"
#include "skaclient.h"
#include "ftrigr.h"

uint16 ftrigr_subscribe (ftrigr_ref a, char const *path, char const *re, uint32 options, struct taia const *deadline, struct taia *stamp)
{
  unsigned int pathlen = str_len(path) ;
  unsigned int relen = str_len(re) ;
  unsigned int i ;
  if (!gensetdyn_new(&a->data, &i)) return 0 ;
  {
    char tmp[pathlen + relen + 16] ;
    uint16_pack_big(tmp, (uint16)i) ;
    tmp[2] = 'L' ;
    uint32_pack_big(tmp+3, options) ;
    uint32_pack_big(tmp+7, (uint32)pathlen) ;
    uint32_pack_big(tmp+11, (uint32)relen) ;
    byte_copy(tmp+15, pathlen, path) ;
    tmp[15+pathlen] = 0 ;
    byte_copy(tmp+16+pathlen, relen, re) ;
    if (!skaclient2_send(&a->connection, tmp, pathlen + relen + 16, deadline, stamp)) goto fail ;
  }
  if (!skaclient2_getack(&a->connection, deadline, stamp)) goto fail ;
  {
    ftrigr1_t *p = GENSETDYN_P(ftrigr1_t, &a->data, i) ;
    p->options = options ;
    p->state = FR1STATE_LISTENING ;
    p->count = 0 ;
    p->what = 0 ;
  }
  return (uint16)(i+1) ;

 fail:
  gensetdyn_delete(&a->data, i) ;
  return 0 ;
}
