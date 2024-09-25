#include <mrtl/model/mart/mart.h>
#include <mrtl/common/functions.h>
#include <mlog.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>


int regression_tree_predict ( struct RegressionTree * t, float * dst, float * features )
{
    struct RegressionTreeNode * n = &t->nodes[0];

    while ( n->feature_index >= 0 )
    {
        n = ( features[n->feature_index] <= n->value ) ?
            &t->nodes[n->left_child] :
            &t->nodes[n->right_child];
    }

    *dst = n->value;

    return 0;
}


int regression_tree_fini ( struct RegressionTree * t )
{
    free( t->nodes );
    t->node_count = 0;
    return 0;
}


int line_to_ints ( int * ints, char * line )
{
    int i = 0;
    char * save_ptr;
    char * tok = strtok_r( line, "= ", &save_ptr );  // ignore the first token, it is the line title
    tok = strtok_r( NULL, "= ", &save_ptr );
    while ( tok != NULL )
    {
        ints[i] = atoi( tok );
        tok = strtok_r( NULL, "= ", &save_ptr );
        ++i;
    }

    return i;
}


int line_to_floats ( float * floats, char * line )
{
    int i = 0;
    char * save_ptr;
    char * tok = strtok_r( line, "= ", &save_ptr );  // ignore the first token, it is the line title
    tok = strtok_r( NULL, "= ", &save_ptr );
    while ( tok != NULL )
    {
        floats[i] = atof( tok );
        tok = strtok_r( NULL, "= ", &save_ptr );
        ++i;
    }

    return i;
}


int construct_tree (
        struct RegressionTree * t,
        size_t leaf_count,
        int * split_feature,
        float * threshold,
        int * left_child,
        int * right_child,
        float * leaf_value )
{
    size_t num_branches = t->node_count - leaf_count;

    size_t li = num_branches;  // leaf iterator, leaves start after branches in array.

    // Iterate through the internal (branch) nodes.
    for ( size_t bi = 0; bi < num_branches; bi++ )
    {
        struct RegressionTreeNode * n = &t->nodes[bi];
        n->value = threshold[bi];

        // The feature_index needs to be shifted by the number of columns left
        // of input0 in the fitting data, less the target column.
        //
        // So, if there are 9 columns before input0, but one of those is the
        // target column, then the shift should be: 9 - 1 = 8
        n->feature_index = split_feature[bi] - 8;

        int lci = left_child[bi];
        if ( lci > 0 )
        {
            n->left_child = lci;
        }
        else
        {
            // Left child is a leaf.
            struct RegressionTreeNode * lc = &t->nodes[li];
            lc->feature_index = -1;
            lc->left_child    = -1;
            lc->right_child   = -1;
            lc->value         = leaf_value[~lci];

            n->left_child = li;
            li++;
        }

        int rci = right_child[bi];
        if ( rci > 0 )
        {
            n->right_child = rci;
        }
        else
        {
            // Right child is a leaf.
            struct RegressionTreeNode * rc = &t->nodes[li];
            rc->feature_index = -1;
            rc->left_child    = -1;
            rc->right_child   = -1;
            rc->value         = leaf_value[~rci];

            n->right_child = li;
            li++;
        }

        log_debug( "branch id: %d    lc: %d    rc: %d", bi, n->left_child, n->right_child );
    }

    log_debug( "node count: %d    li: %d", t->node_count, li );

    return 0;
}


int mart_load_lightgbm_from_file ( struct Mart * m, const char * filename )
{
    char * s = NULL;
    int res = file_to_string( &s, filename );

    if ( res < 0 )
    {
        log_error( "mart_load_lightgbm_from_file() unable to load model from %s", filename );
        return res;
    }

    int max_trees = 8192;

    m->trees = calloc ( max_trees, sizeof(struct RegressionTree) );
    struct RegressionTree * tree = NULL;
    size_t max_node_count = 0;
    int * split_feature = NULL;
    float * threshold = NULL;
    int * left_child = NULL;
    int * right_child = NULL;
    float * leaf_value = NULL;

    enum Block { NONE_BLOCK, HEADER_BLOCK, TREE_BLOCK, DONE_BLOCK };

    int tree_num = -1;
    int num_leaves;
    enum Block block = NONE_BLOCK;
    char * line_save;
    char * line = strtok_r( s, "\n\r", &line_save );

    while ( line != NULL && block != DONE_BLOCK )
    {
        if ( !strcmp("tree", line) )
        { block = HEADER_BLOCK; }
        else if ( !strcmp("end of trees", line) )
        {
            if ( tree )
            {
                construct_tree( tree, num_leaves, split_feature, threshold,
                        left_child, right_child, leaf_value );
                log_debug( "mart_load_lightgbm_from_file() constructed tree %d", tree_num );
            }
            block = DONE_BLOCK;
        }
        else if ( strstr(line, "Tree=") == line )
        {
            if ( tree )
            {
                construct_tree( tree, num_leaves, split_feature, threshold,
                        left_child, right_child, leaf_value );
                log_debug( "mart_load_lightgbm_from_file() constructed tree %d", tree_num );
            }

            // Prepare to populate the next tree.
            sscanf( line, "Tree=%d", &tree_num );

            if ( tree_num >= max_trees )
            {
                log_error( "mart_load_lightgbm_from_file() trying to read tree %d, but max_trees = %d",
                        tree_num, max_trees);
                return -1;
            }

            //log_notice("found Tree= on line: %s for tree %d", line, tree_num);
            tree = &m->trees[tree_num];
            tree->node_count = 0;
            block = TREE_BLOCK;
        }
        else if ( block == TREE_BLOCK )
        {
            if ( strstr(line, "num_leaves") == line )
            {
                sscanf( line, "num_leaves=%d", &num_leaves );
                tree->node_count = 2 * num_leaves - 1;
                tree->nodes = calloc ( tree->node_count, sizeof(struct RegressionTreeNode) );
                if ( tree->node_count > max_node_count )
                {
                    max_node_count = tree->node_count;

                    split_feature = (int *) split_feature ?
                        realloc( split_feature, max_node_count*sizeof(int)) :
                        calloc( max_node_count, sizeof(int) );

                    threshold = (float *) threshold ?
                        realloc( threshold, max_node_count*sizeof(float)) :
                        calloc( max_node_count, sizeof(float) );

                    left_child = (int *) left_child ?
                        realloc( left_child, max_node_count*sizeof(int)) :
                        calloc( max_node_count, sizeof(int) );

                    right_child = (int *) right_child ?
                        realloc( right_child, max_node_count*sizeof(int)) :
                        calloc( max_node_count, sizeof(int) );

                    leaf_value = (float *) leaf_value ?
                        realloc( leaf_value, max_node_count*sizeof(float)) :
                        calloc( max_node_count, sizeof(float) );
                }
                log_debug( "tree %d has %d nodes", tree_num, tree->node_count );
            }
            else if ( strstr(line, "split_feature") == line )
            {
                int value_count = line_to_ints( split_feature, line );
                log_debug( "Parsed %d values for split_feature", value_count );
            }
            else if ( strstr(line, "threshold") == line )
            {
                int value_count = line_to_floats( threshold, line );
                log_debug( "Parsed %d values for threshold", value_count );
            }
            else if ( strstr(line, "left_child") == line )
            {
                int value_count = line_to_ints( left_child, line );
                log_debug( "Parsed %d values for left_child", value_count );
            }
            else if ( strstr(line, "right_child") == line )
            {
                int value_count = line_to_ints( right_child, line );
                log_debug( "Parsed %d values for right_child", value_count );
            }
            else if ( strstr(line, "leaf_value") == line )
            {
                int value_count = line_to_floats( leaf_value, line );
                log_debug( "Parsed %d values for leaf_value", value_count );
            }
        }

        line = strtok_r( NULL, "\n\r", &line_save );
    }

    free( s );

    if ( split_feature )  { free( split_feature ); }
    if ( threshold )      { free( threshold ); }
    if ( left_child )     { free( left_child ); }
    if ( right_child )    { free( right_child ); }
    if ( leaf_value )     { free( leaf_value ); }

    m->tree_count = tree_num + 1;

    m->trees = (struct RegressionTree *) realloc ( m->trees, m->tree_count*sizeof(struct RegressionTree) );

    log_notice( "mart_load_lightgbm_from_file() loaded %lu trees from %s",
            m->tree_count, filename );

    return 0;
}


int mart_predict ( struct Mart * m, float * dst, float * features )
{
    *dst = 0.f;

    for ( size_t i = 0; i < m->tree_count; i++ )
    {
        float prediction;
        regression_tree_predict( &m->trees[i], &prediction, features );
        *dst += prediction;
    }

    return 0;
}


int mart_fini ( struct Mart * m )
{
    for ( size_t t = 0; t < m->tree_count; t++ )
    {
        regression_tree_fini( &m->trees[t] );
    }

    free( m->trees );
    m->trees = NULL;
    m->tree_count = 0;

    return 0;
}

