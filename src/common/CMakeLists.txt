add_library( mrtl_common SHARED functions.c rbtree.c country.c task.c types.c moving_averages.c lookup_table.c ring_buffer.c lag_record.c )
target_link_libraries( mrtl_common dbw pq odbc ${CFGDB_LIB} )

add_executable( test_common test.c )
target_link_libraries( test_common mrtl_common iniparser mlog m )

install(TARGETS mrtl_common
    ARCHIVE DESTINATION lib64
    LIBRARY DESTINATION lib64
    RUNTIME DESTINATION bin
)

