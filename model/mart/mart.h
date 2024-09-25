#ifndef _MRTL_MODEL_MART_H_
#define _MRTL_MODEL_MART_H_

#include <stddef.h>
#include <stdint.h>


struct RegressionTreeNode
{
    float value;  // holds split value if node, estimate value if leaf.
    int16_t feature_index;  // if < 0, this is a leaf.
    int16_t left_child;
    int16_t right_child;
};


struct RegressionTree
{
    struct RegressionTreeNode * nodes;
    size_t node_count;
};

int regression_tree_predict ( struct RegressionTree *, float *, float * );

int regression_tree_fini ( struct RegressionTree *  );


struct Mart
{
    struct RegressionTree * trees;
    size_t tree_count;
};

int mart_load_lightgbm_from_file ( struct Mart *, const char * );

int mart_predict ( struct Mart *, float *, float * );

int mart_fini ( struct Mart * );

#endif  // _MRTL_MODEL_MART_H_

