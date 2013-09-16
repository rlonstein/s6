/* ISC license. */

#include "sgetopt.h"
#include "bytestr.h"
#include "uint16.h"
#include "fmtscan.h"
#include "bitarray.h"
#include "tai.h"
#include "strerr2.h"
#include "iopause.h"
#include "ftrigr.h"
#include "s6-supervise.h"

#define USAGE "s6-svwait [ -u | -d ] [ -A | -a | -o ] [ -t timeout ] servicedir..."
#define dieusage() strerr_dieusage(100, USAGE)

static inline int check (unsigned char const *ba, unsigned int n, int wantup, int or)
{
  return (bitarray_first(ba, n, or == wantup) < n) == or ;
}

int main (int argc, char const *const *argv)
{
  struct taia deadline, tto ;
  ftrigr_t a = FTRIGR_ZERO ;
  uint32 options = FTRIGR_REPEAT ;
  int or = 0 ;
  int wantup = 1 ;
  PROG = "s6-svwait" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0 ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "udAaot:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'u' : wantup = 1 ; break ;
        case 'd' : wantup = 0 ; break ;
        case 'A' : or = 0 ; options |= FTRIGR_REPEAT ; break ;
        case 'a' : or = 0 ; options &= ~FTRIGR_REPEAT ; break ;
        case 'o' : or = 1 ; options &= ~FTRIGR_REPEAT ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) taia_from_millisecs(&tto, t) ; else tto = infinitetto ;
  }
  if (!argc) dieusage() ;

  taia_now_g() ;
  taia_add_g(&deadline, &tto) ;

  if (!ftrigr_startf_g(&a, &deadline)) strerr_diefu1sys(111, "ftrigr_startf") ;

  {
    iopause_fd x = { -1, IOPAUSE_READ, 0 } ;
    unsigned int i = 0 ;
    uint16 list[argc] ;
    unsigned char states[bitarray_div8(argc)] ;
    x.fd = ftrigr_fd(&a) ;
    for (; i < (unsigned int)argc ; i++)
    {
      unsigned int len = str_len(argv[i]) ;
      char s[len + 1 + sizeof(S6_SUPERVISE_EVENTDIR)] ;
      byte_copy(s, len, argv[i]) ;
      s[len] = '/' ;
      byte_copy(s + len + 1, sizeof(S6_SUPERVISE_EVENTDIR), S6_SUPERVISE_EVENTDIR) ;
      list[i] = ftrigr_subscribe_g(&a, s, "u|d", options, &deadline) ;
      if (!list[i]) strerr_diefu2sys(111, "ftrigr_subscribe to ", argv[i]) ;
    }

    for (i = 0 ; i < (unsigned int)argc ; i++)
    {
      s6_svstatus_t st = S6_SVSTATUS_ZERO ;
      if (!s6_svstatus_read(argv[i], &st)) strerr_diefu1sys(111, "s6_svstatus_read") ;
      bitarray_poke(states, i, !!st.pid) ;
    }

    for (;;)
    {
      if (check(states, argc, wantup, or)) break ;
      {
        register int r = iopause_g(&x, 1, &deadline) ;
        if (r < 0) strerr_diefu1sys(111, "iopause") ;
        else if (!r) strerr_dief1x(1, "timed out") ;
      }

      if (ftrigr_update(&a) < 0) strerr_diefu1sys(111, "ftrigr_update") ;
      for (i = 0 ; i < (unsigned int)argc ; i++)
      {
        char what ;
        register int r = ftrigr_check(&a, list[i], &what) ;
        if (r < 0) strerr_diefu1sys(111, "ftrigr_check") ;
        if (r) bitarray_poke(states, i, what == 'u') ;
      } 
    }
  }
  return 0 ;
}
