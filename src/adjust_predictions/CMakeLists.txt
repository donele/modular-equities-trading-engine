add_library(mrtl_adjust_predictions SHARED adjust_predictions.c)
#target_link_libraries(mrtl_types )

./src/agent/CMakeLists.txt
add_library( mrtl_agent_loader SHARED load_agent_library.c )
target_link_libraries( mrtl_agent_loader mrtl_common )

add_library( mrtl_sampler SHARED sampler.c load_strategy_library.c )
target_link_libraries( mrtl_sampler mrtl_common m )

add_library( mrtl_simulator SHARED simulator.c load_strategy_library.c )
target_link_libraries( mrtl_simulator mrtl_common mrtl_model_mart m )

add_library( mrtl_live_client SHARED live.c load_strategy_library.c )
target_link_libraries( mrtl_live_client mrtl_common mrtl_model_mart m )

install(TARGETS mrtl_agent_loader mrtl_sampler mrtl_simulator mrtl_live_client
    ARCHIVE DESTINATION lib64
    LIBRARY DESTINATION lib64
    RUNTIME DESTINATION bin
)
