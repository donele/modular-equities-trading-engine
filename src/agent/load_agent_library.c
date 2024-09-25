#include <mrtl/common/task.h>
#include <mbpsig.h>
#include <mlog.h>
#include <iniparser.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>


typedef int (*agent_init_func)(
        mbp_sigproc_t *,
        const struct mbp_sigproc_args *,
        dictionary * );

typedef int (*agent_fini_func)( mbp_sigproc_t );

typedef int (*agent_on_pre_func)(
        mbp_sigproc_t proct,
        uint64_t nsecs );

typedef int (*agent_on_post_func)(
        mbp_sigproc_t proct,
        uint64_t nsecs );

typedef int (*agent_on_trade_func)(
        mbp_sigproc_t proct,
        uint64_t nsecs,
        uint16_t sec_id,
        uint16_t mkt_id,
        int64_t px,
        uint32_t sz,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct mbpupd * u );

typedef int (*agent_on_book_update_func)(
        mbp_sigproc_t,
        uint64_t nsecs,
        uint16_t sec_id,
        uint16_t mkt_id,
        const struct mbp_book * book,
        const struct mbp_book * agg,
        const struct mbpupd * u );


void * agentlib;
int8_t use_dark_trades;
agent_init_func           agent_init;
agent_fini_func           agent_fini;
agent_on_pre_func         agent_on_pre;
agent_on_post_func        agent_on_post;
agent_on_trade_func       agent_on_trade;
agent_on_book_update_func agent_on_book_update;


bool has_market_order( const struct mbp_book * agg )
{
    return ( (agg->bid[0].px == mktpx && agg->bid[0].sz > 0) || (agg->ask[0].px == mktpx && agg->ask[0].sz > 0) ) ?
        true : false;
}


int mbp_sigalloc_i ( mbp_sigproc_t * proct, const struct mbp_sigproc_args * args )
{
    const char * agent_config_file = getenv( "MRTL_AGENT_CONFIG" );

    if ( NULL == agent_config_file )
    {
        log_error( "Environment variable MRTL_AGENT_CONFIG must be set." );
        return -1;
    }

    dictionary * cfg = iniparser_load( agent_config_file );

    if ( NULL == cfg )
    {
        log_error( "mbp_sigalloc_i() cannot load agent config file: '%s'", agent_config_file );
        errno = ENOENT;
        return -1;
    }

    log_severity = iniparser_getint( cfg, "agent:log_severity", ls_info );

    use_dark_trades = iniparser_getint( cfg, "agent:use_dark_trades", 0 );
    log_notice( "Agent loader has use_dark_trades = %d", use_dark_trades );

    const char * agent_library_file = iniparser_getstring( cfg, "agent:library", NULL );
    agentlib = dlopen( agent_library_file, RTLD_LAZY );

    if ( NULL == agentlib )
    {
        log_error( "mbp_sigalloc_i() cannot load agent library: '%s'", agent_library_file );
        errno = ENOENT;
        return -1;
    }
    else
    {
        log_notice( "mbp_sigalloc_i() loaded agent library: '%s'", agent_library_file );
    }

    agent_init = dlsym( agentlib, "agent_init" );

    if ( NULL == agent_init )
    {
        log_error( "mbp_sigalloc_i() cannot find 'agent_init' function in agent library: %s", agent_library_file );
        errno = ELIBBAD;
        return -1;
    }

    agent_fini = dlsym( agentlib, "agent_fini" );

    if ( NULL == agent_fini )
    {
        log_error( "mbp_sigalloc_i() cannot find 'agent_fini' function in agent library: %s", agent_library_file );
        errno = ELIBBAD;
        return -1;
    }

    agent_on_pre = dlsym( agentlib, "agent_on_pre" );

    if ( NULL == agent_on_pre )
    { log_notice( "Warning: mbp_sigalloc_i() cannot find 'agent_on_pre' function in agent library: %s", agent_library_file ); }

    agent_on_post = dlsym( agentlib, "agent_on_post" );

    if ( NULL == agent_on_post )
    { log_notice( "Warning: mbp_sigalloc_i() cannot find 'agent_on_post' function in agent library: %s", agent_library_file ); }

    agent_on_trade = dlsym( agentlib, "agent_on_trade" );

    if ( NULL == agent_on_trade )
    {
        log_error( "mbp_sigalloc_i() cannot find 'agent_on_trade' function in agent library: %s", agent_library_file );
        errno = ELIBBAD;
        return -1;
    }

    agent_on_book_update = dlsym( agentlib, "agent_on_book_update" );

    if ( NULL == agent_on_book_update )
    {
        log_error( "mbp_sigalloc_i() cannot find 'agent_on_book_update' function in agent library: %s", agent_library_file );
        errno = ELIBBAD;
        return -1;
    }


    int err = agent_init( proct, args, cfg );

    iniparser_freedict(cfg);
    log_notice( "Agent initialized with result: %d", err );

    return err;
}


int mbp_sigcall_pre_i (
        mbp_sigproc_t proct,
        uint64_t nsecs )
{
    if ( NULL == agent_on_pre )
    {
        return 0;
    }

    int err = agent_on_pre( proct, nsecs );

    if ( err != 0 )
    { log_notice( "agent_on_pre() returned %d", err ); }

    return err;
}


int mbp_sigcall_i (
        mbp_sigproc_t proct,
        const struct mbpupd * u,
        struct mbp_book * book,
        struct mbp_book * agg )
{
    int err = 0;
    uint64_t nsecs = u->ts;

    // Do not process books with a market order (auction).
    if ( has_market_order( agg ) )
    { return 0; }

    if ( !u->drk || use_dark_trades > 0 )
    {
        if ( u->trd )
        {
            err = agent_on_trade( proct, nsecs, u->sec_id,
                    u->mkt_id, u->px, u->sz, book, agg, u );

            if ( err != 0 )  { log_notice( "agent_on_trade() returned %d", err ); }

            if ( !err && u->del )
            {
                err = agent_on_book_update( proct, nsecs,
                        u->sec_id, u->mkt_id, book, agg, u );

                if ( err != 0 )  { log_notice( "agent_on_book_update() with trade returned %d", err ); }
            }
        }
        else
        {
            err = agent_on_book_update( proct, nsecs,
                    u->sec_id, u->mkt_id, book, agg, u );

            if ( err != 0 )  { log_notice( "agent_on_book_update() returned %d", err ); }
        }
    }

    if ( err != 0 )
    { log_notice( "mbp_sigcall_i(): has error value %d", err ); }

    return err;
}


int mbp_sigcall_post_i (
        mbp_sigproc_t proct,
        uint64_t nsecs )
{
    if ( NULL == agent_on_post )
    {
        return 0;
    }

    int err = agent_on_post( proct, nsecs );

    if ( err != 0 )
    { log_notice( "agent_on_post() returned %d", err ); }

    return err;

}


void mbp_sigfree_i ( mbp_sigproc_t proct )
{
    int err = agent_fini( proct );
    dlclose( agentlib );

    if ( err != 0 )
    { log_notice( "agent_fini() returned %d", err ); }
}

