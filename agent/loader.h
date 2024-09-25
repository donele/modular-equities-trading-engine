#ifndef _MRTL_AGENT_LOADER_H_
#define _MRTL_AGENT_LOADER_H_

#include <mbpsig.h>
#include <iniparser.h>


typedef int (*agent_init_func)( mbp_sigproc_t *, dictionary * );
typedef int (*agent_fini_func)( mbp_sigproc_t * );

#endif  // _MRTL_AGENT_LOADER_H_

