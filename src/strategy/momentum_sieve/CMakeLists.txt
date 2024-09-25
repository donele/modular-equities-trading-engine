add_library( mrtl_momentum_sieve SHARED symbol_data.c features.c targets.c init.c predict.c generate_orders.c )
target_link_libraries( mrtl_momentum_sieve mrtl_model_mart mrtl_common m )

install(TARGETS mrtl_momentum_sieve
    ARCHIVE DESTINATION lib64
    LIBRARY DESTINATION lib64
    RUNTIME DESTINATION bin
)./src/strategy/classic/CMakeLists.txt
add_library( mrtl_classic SHARED features.c generate_orders.c init.c misc.c predict.c symbol_data.c targets.c)
target_link_libraries( mrtl_classic mrtl_model_mart mrtl_common m )
./src/model/mart/CMakeLists.txt
add_library( mrtl_model_mart SHARED mart.c )
target_link_libraries( mrtl_model_mart mlog )

add_executable( test test.c )
target_link_libraries( test mrtl_model_mart mrtl_common m )

install(TARGETS mrtl_model_mart
    ARCHIVE DESTINATION lib64
    LIBRARY DESTINATION lib64
    RUNTIME DESTINATION bin
)
