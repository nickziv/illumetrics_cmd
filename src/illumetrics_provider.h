/*
 * Generated by dtrace(1M).
 */

#ifndef	_ILLUMETRICS_PROVIDER_H
#define	_ILLUMETRICS_PROVIDER_H

#include <unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if _DTRACE_VERSION

#define	ILLUMETRICS_GOT_HERE(arg0) \
	__dtrace_illumetrics___got_here(arg0)
#ifndef	__sparc
#define	ILLUMETRICS_GOT_HERE_ENABLED() \
	__dtraceenabled_illumetrics___got_here()
#else
#define	ILLUMETRICS_GOT_HERE_ENABLED() \
	__dtraceenabled_illumetrics___got_here(0)
#endif


extern void __dtrace_illumetrics___got_here(int);
#ifndef	__sparc
extern int __dtraceenabled_illumetrics___got_here(void);
#else
extern int __dtraceenabled_illumetrics___got_here(long);
#endif

#else

#define	ILLUMETRICS_GOT_HERE(arg0)
#define	ILLUMETRICS_GOT_HERE_ENABLED() (0)

#endif


#ifdef	__cplusplus
}
#endif

#endif	/* _ILLUMETRICS_PROVIDER_H */
