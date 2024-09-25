#include <mrtl/common/country.h>
#include <mrtl/common/functions.h>
#include <mrtl/common/constants.h>
#include <mlog.h>


int country_init ( struct Country * c, dictionary * cfg )
{
    const char * name = iniparser_getstring( cfg, "country:name", NULL );
    if ( NULL == name )
    {
        log_error( "Config file is missing country:name" );
        return -1;
    }
    strncpy( c->name, name, sizeof(c->name)-1 );

    if ( !strncmp( "JP", c->name, 4 ) )
    { c->market_letter = 'T'; }
    else
    { log_error( "country_init(): Unknown country: %s", c->name ); }

    double open_time_hours = iniparser_getdouble( cfg, "country:open_time_hours", -1 );
    if ( open_time_hours < 0 )
    {
        log_error( "Config file is missing country:open_time_hours" );
        return -1;
    }
    c->open_time = open_time_hours * HOURS;

    double close_time_hours = iniparser_getdouble( cfg, "country:close_time_hours", -1 );
    if ( close_time_hours < 0 )
    {
        log_error( "Config is missing country:close_time_hours" );
        return -1;
    }
    c->close_time = close_time_hours * HOURS;

    double lunch_start_hours = iniparser_getdouble( cfg, "country:lunch_start_hours", -1 );
    if ( lunch_start_hours < 0 )
    {
        log_notice( "Config is missing country:lunch_start_hours" );
        c->lunch_start = 0;
    }
    else
    { c->lunch_start = lunch_start_hours * HOURS; }

    double lunch_end_hours = iniparser_getdouble( cfg, "country:lunch_end_hours", -1 );
    if ( lunch_end_hours < 0 )
    {
        log_notice( "Config is missing country:lunch_end_hours" );
        c->lunch_end = 0;
    }
    else
    { c->lunch_end = lunch_end_hours * HOURS; }

    c->exchange_count = iniparser_getint( cfg, "country:exchange_count", 0 );
    if ( 0 == c->exchange_count )
    {
        log_error( "Config is missing country:exchange_count" );
        return -1;
    }

    const char * dbhost = iniparser_getstring( cfg, "country:database_host", NULL );
    if ( NULL == dbhost )
    {
        log_error( "Config is missing country:database_host" );
        return -1;
    }
    strncpy( c->dbhost, dbhost, sizeof(c->dbhost)-1 );

    const char * dbname = iniparser_getstring( cfg, "country:database_name", NULL );
    if ( NULL == dbname )
    {
        log_error( "Config is missing country:database_name" );
        return -1;
    }
    strncpy( c->dbname, dbname, sizeof(c->dbname)-1 );


    return 0;
}


bool country_has_lunch ( const struct Country * c )
{
    return ( c->lunch_start > 0 ) && ( c->lunch_start < c->lunch_end );
}


bool country_during_lunch ( const struct Country * c, uint64_t nsecs )
{
    return
        country_has_lunch( c )      &&
        ( c->lunch_start <= nsecs ) &&
        ( nsecs <= c->lunch_end );
}


bool country_during_continuous_session ( const struct Country * c, uint64_t nsecs )
{
    return
        c->open_time <= nsecs &&
        nsecs < c->close_time &&
        !country_during_lunch( c, nsecs );
}


int country_date_ok ( const struct Country * c, int idate )
{
    int ok = 0;

    size_t max_sql_length = 1024;
    char sql [max_sql_length];

    int * idates = NULL;
    size_t rcnt = 0;

    struct db_fld fields [] = {
        { "idate", -1, 0, DBINT32, 0 }
    };

    db_t db;

    if ( !strncmp("US", c->name, 4) )
    {
        snprintf( sql, max_sql_length,
                "SELECT idate FROM tickDataElvOK "
                    "WHERE dataok=1 "
                        "AND arcaOK=1 "
                        "AND nyseOK=1 "
                        "AND inetOK=1 "
                        "AND batsOK=1 "
                        "AND edgxOK=1 "
                        "AND edgaOK=1 "
                        "AND bxOK=1 "
                        "AND byxOK=1 "
                        "AND psxOK=1 "
                        "AND idate = %d",
                        idate );

        equitydata_connect( &db, c );
    }
    else if ( !strncmp("JP", c->name, 4) )
    {
        snprintf( sql, max_sql_length,
                "SELECT idate FROM tickdataok "
                    "WHERE dataok = 1 "
                        "AND market = 'T' "
                        "AND idate = %d",
                        idate );

        hfstock_connect( &db, c );
    }
    else
    {
        log_error("country_date_ok(): Unknown country: %s", c->name);
        db_close( db );
        return -1;
    }

    int db_read_stat;
    if ( (db_read_stat = db_read( db, sql, fields, 1, sizeof(int), &idates, &rcnt )) < 0 )
    {
        log_error( "country_date_ok(): Error reading database, db_read() returned %d", db_read_stat );
        db_close( db );
        return -1;
    }

    db_close( db );

    if ( rcnt == 1 )
    { ok = 1; }

    if ( idates )
    { free ( idates ); }

    return ok;
}


int country_previous_trading_day ( const struct Country * c, int idate )
{
    size_t max_sql_length = 1024;
    char sql [max_sql_length];

    int * idates = NULL;
    size_t rcnt = 0;

    struct db_fld fields [] = {
        { "idate", -1, 0, DBINT32, 0 }
    };

    char market_code [8];

    if ( !strncmp("US", c->name, 4) )
    { strcpy( market_code, "U" );  /* TODO: This is wrong. */ }
    else if ( !strncmp("JP", c->name, 4) )
    { strcpy( market_code, "T" ); }
    else
    {
        log_error( "country_previous_trading_day(): Unknown country: %s", c->name );
        return -1;
    }

    snprintf( sql, max_sql_length,
        "SELECT date2int(MAX(dt)) AS idate FROM prevdays((SELECT int2date(%d)),'%s','1 month')",
        idate, market_code );

    db_t db;
    hfstock_connect( &db, c );

    int db_read_stat;
    if ( (db_read_stat = db_read( db, sql, fields, 1, sizeof(int), &idates, &rcnt ) ) < 0 )
    {
        log_error( "country_previous_trading_day(): Error reading database db_read() returned %d", db_read_stat );
        db_close( db );
        return -1;
    }

    db_close( db );

    int previous_idate = -1;

    if ( rcnt == 1 )
    { previous_idate = idates[0]; }

    if ( idates )
    { free( idates ); }

    return previous_idate;
}

