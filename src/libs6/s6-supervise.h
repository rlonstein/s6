/* ISC license. */

#ifndef S6_SUPERVISE_H
#define S6_SUPERVISE_H

#include "tai.h"

#define S6_SUPERVISE_CTLDIR "supervise"
#define S6_SUPERVISE_EVENTDIR "event"
#define S6_SVSCAN_CTLDIR ".s6-svscan"
#define S6_SVSTATUS_FILENAME S6_SUPERVISE_CTLDIR "/status"
#define S6_SVSTATUS_SIZE 18

extern int s6_svc_write (char const *, char const *, unsigned int) ;
extern int s6_svc_main (int, char const *const *, char const *, char const *, char const *) ;

typedef struct s6_svstatus_s s6_svstatus_t, *s6_svstatus_t_ref ;
struct s6_svstatus_s
{
  struct taia stamp ;
  unsigned int pid ;
  unsigned int flagwant : 1 ;
  unsigned int flagwantup : 1 ;
  unsigned int flagpaused : 1 ;
  unsigned int flagfinishing : 1 ;
} ;

#define S6_SVSTATUS_ZERO { TAIA_ZERO, 0, 0, 0, 0, 0 }


extern void s6_svstatus_pack (char *, s6_svstatus_t const *) ;
extern void s6_svstatus_unpack (char const *, s6_svstatus_t_ref) ;
extern int s6_svstatus_read (char const *, s6_svstatus_t_ref) ;
extern int s6_svstatus_write (char const *, s6_svstatus_t const *) ;

/* These functions leak a fd, that's intended */
extern int s6_supervise_lock (char const *) ;
extern int s6_supervise_lock_mode (char const *, unsigned int, unsigned int) ;

#endif
