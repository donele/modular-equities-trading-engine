#ifndef _MRTL_AGENT_LOAD_STRATEGY_LIBRARY_H_
#define _MRTL_AGENT_LOAD_STRATEGY_LIBRARY_H_

#include <mrtl/strategy/function_signatures.h>


int load_strategy_library (
        const char * library_file_name,
        void ** library,
        strategy_init_func                           * strategy_init,
        strategy_predict_func                        * strategy_predict,
        strategy_adjust_predictions_func             * strategy_adjust_predictions,
        strategy_init_symbol_datas_func              * strategy_init_symbol_datas,
        strategy_on_trade_func                       * strategy_on_trade,
        strategy_on_book_update_func                 * strategy_on_book_update,
        strategy_generate_features_func              * strategy_generate_features,
        strategy_calculate_target_func               * strategy_calculate_target,
        strategy_generate_orders_func                * strategy_generate_orders,
        strategy_write_sample_header_func            * strategy_write_sample_header
        );

#endif  // _MRTL_AGENT_LOAD_STRATEGY_LIBRARY_H_

