// Copyright 2023 Markus Anders
// This file is part of dejavu 2.0.
// See LICENSE for extended copyright information.

#ifndef DEJAVU_INPROCESS_H
#define DEJAVU_INPROCESS_H

#include "dfs.h"
#include "bfs.h"
#include "rand.h"
#include "components.h"

namespace dejavu::search_strategy {

    /**
     * \brief Inprocessing for symmetry detection
     *
     * Reads a graph and aspects of the state of the individualization-refinement search. Then, it tries to simplify
     * the graph according to the given state, as well as further techniques such as invariants.
     */
    class inprocessor {
    public:
        // statistics
        big_number s_grp_sz; /**< group size */
        int h_splits_hint = INT32_MAX;

        std::vector<std::pair<int, int>> inproc_can_individualize; /**< vertices that can be individualized           */

        /**
         * Computes an invariant using a `shallow` breadth-first search.
         *
         * @param g the graph
         * @param local_state state used to perform IR computations
         * @param inv place to store the invariant
         * @param orbit_partition (partial) orbit partition of the graph, if available
         * @param depth depth of trace to look at
         * @param lower_depth whether to try to lower the depth, if a smaller distinguishing depth is found
         */
        static void shallow_bfs_invariant(sgraph* g, ir::controller &local_state, worklist_t<unsigned long>& inv,
                                          groups::orbit& orbit_partition,
                                          int depth = 8, bool lower_depth = true) {
            local_state.use_reversible(true);
            local_state.use_trace_early_out(lower_depth); // TODO bad for groups128, otherwise seems fine
            local_state.use_increase_deviation(false);
            local_state.use_split_limit(true, depth); // 20

            bool repeat = true;
            int best_depth = depth;
            while (repeat) {
                repeat = false;
                local_state.use_split_limit(true, depth);
                for (int i = 0; i < g->v_size; ++i) {
                    if(!orbit_partition.represents_orbit(i)) continue;

                    local_state.T->set_hash(0);
                    local_state.reset_trace_equal();
                    const int col = local_state.c->vertex_to_col[i];
                    const int col_sz = local_state.c->ptn[col] + 1;
                    if (col_sz >= 2 && col_sz != orbit_partition.orbit_size(i)) {
                        local_state.move_to_child(g, i);
                        inv[i] = local_state.T->get_hash();
                        const int splits = local_state.get_number_of_splits();
                        local_state.move_to_parent();
                        if(100 * i < g->v_size && lower_depth && splits < best_depth &&
                           col == (*local_state.compare_base)[0].color) {
                            best_depth = splits;
                        }
                        if(100 * i >= g->v_size && lower_depth && best_depth < depth) {
                            depth = best_depth;
                            lower_depth = false;
                            repeat = true;
                            break;
                        }
                    } else {
                        inv[i] = 0;
                    }
                }

                for (int i = 0; i < g->v_size; ++i) inv[i] = inv[orbit_partition.find_orbit(i)];
            }

            local_state.use_trace_early_out(false);
            local_state.use_increase_deviation(false);
            local_state.use_split_limit(false);
        }
        std::vector<std::pair<int, int>> inproc_maybe_individualize; /**< vertices that can be individualized         */

        std::vector<int>                 inproc_fixed_points;      /**< vertices fixed by inprocessing                */
        std::vector<int> nodes;

        worklist hash;

        void sort_nodes_map(std::vector<unsigned long>* map, int* colmap) {
            struct comparator_map {
                std::vector<unsigned long> *map;
                int* colmap;

                explicit comparator_map(std::vector<unsigned long> *map, int* colmap) {
                    this->map = map;
                    this->colmap = colmap;
                }

                bool operator()(const int &a, const int &b) const {
                    return (colmap[a] < colmap[b]) || ((colmap[a] == colmap[b]) && ((*map)[a] < (*map)[b]));
                }
            };
            auto c = comparator_map(map, colmap);
            std::sort(nodes.begin(), nodes.end(), c);
        }

        /**
         * Refines the given IR state according to the given invariant.
         *
         * @param g the graph
         * @param local_state the IR state
         * @param inv the node invariant
         */
        void split_with_invariant(sgraph* g, ir::controller &local_state, worklist_t<unsigned long>& inv) {
            hash.allocate(g->v_size);

            int num_of_hashs = 0;
            for(int i = 0; i < g->v_size; ++i) nodes.push_back(i);
            sort_nodes_map(inv.get_array(), local_state.c->vertex_to_col);
            unsigned long last_inv = -1;
            int last_col  = local_state.c->vertex_to_col[0];
            for(int i = 0; i < g->v_size; ++i) {
                const int v      = nodes[i];
                const unsigned long v_inv = inv[v];
                const int  v_col = local_state.c->vertex_to_col[v];
                if(last_col != v_col) {
                    last_col = v_col;
                    last_inv = v_inv;
                    ++num_of_hashs;
                }
                if(v_inv != last_inv) {
                    last_inv = v_inv;
                    ++num_of_hashs;
                }
                hash[v] = num_of_hashs;
            }

            g->initialize_coloring(local_state.c, hash.get_array());
        }

        void sort_nodes_map(unsigned long* map, int* colmap) {
            struct comparator_map {
                unsigned long *map;
                int* colmap;

                explicit comparator_map(unsigned long *map, int* colmap) {
                    this->map = map;
                    this->colmap = colmap;
                }

                bool operator()(const int &a, const int &b) const {
                    return (colmap[a] < colmap[b]) || ((colmap[a] == colmap[b]) && (map[a] < map[b]));
                }
            };
            auto c = comparator_map(map, colmap);
            std::sort(nodes.begin(), nodes.end(), c);
        }

        /**
         * Computes an invariant using a `shallow` breadth-first search for 2 consecutive levels.
         *
         * @param g the graph
         * @param local_state state used to perform IR computations
         * @param inv place to store the invariant
         */
        static void shallow_bfs_invariant2(sgraph* g, ir::controller &local_state, worklist_t<unsigned long>& inv) {
            local_state.use_reversible(true);
            local_state.use_trace_early_out(false);
            local_state.use_increase_deviation(true);
            local_state.use_split_limit(true, 8);

            markset original_colors(g->v_size);
            for(int _col = 0; _col < g->v_size;) {
                const int _col_sz = local_state.c->ptn[_col] + 1;
                original_colors.set(_col);
                _col += _col_sz;
            }

            for(int i = 0; i < g->v_size; ++i) {
                local_state.T->set_hash(0);
                local_state.reset_trace_equal();
                const int col = local_state.c->vertex_to_col[i];
                const int col_sz = local_state.c->ptn[col] + 1;
                if(col_sz >= 2) {
                    local_state.move_to_child(g, i);
                    inv[i] += local_state.T->get_hash();
                    for(int _col = 0; _col < g->v_size;) {
                        const int _col_sz = local_state.c->ptn[_col] + 1;
                        if (_col_sz >= 2 && _col_sz <= 16 && !original_colors.get(_col)) {
                            for (int jj = _col; jj < _col + _col_sz; ++jj) {
                                const int j = local_state.c->lab[jj];
                                local_state.move_to_child(g, j);
                                inv[i] += local_state.T->get_hash();
                                local_state.move_to_parent();
                            }
                        }
                        _col += _col_sz;
                    }
                    local_state.move_to_parent();
                } else {
                    inv[i] = 0;
                }
            }

            local_state.use_trace_early_out(false);
            local_state.use_increase_deviation(false);
            local_state.use_split_limit(false);
        }

        /**
         * Give hint as to how deep we need to look for shallow invariants.
         * @param splits_hint the hint
         */
        void set_splits_hint(int splits_hint) {
            h_splits_hint = splits_hint;
        }

        /**
         * Inprocess the (colored) graph using all the available solver data.
         *
         * @param g graph
         * @param tree currently available  ir tree
         * @param group currently available group of symmetries
         * @param local_state workspace to perform individualization&refinement in
         * @param root_save the current coloring of the IR tree root
         * @param budget a limit on the budget
         *
         * @return whether any of the inprocessing techniques succeeded
         */
        bool inprocess(sgraph *g, ir::shared_tree &tree, groups::compressed_schreier &group, ir::controller &local_state,
                       ir::limited_save &root_save, [[maybe_unused]] int budget, bool use_bfs_inprocess,
                       bool use_shallow_inprocess, bool use_shallow_quadratic_inprocess, groups::orbit& orbit_partition) {
            local_state.load_reduced_state(root_save);

            const int cell_prev = root_save.get_coloring()->cells; /*< keep track how many cells we have initially*/
            bool touched_coloring = false; /*< whether we change the root_save or not, i.e., whether we change
                                            *  anything */

            // computes a shallow breadth-first invariant
            if (use_shallow_inprocess && !(tree.get_finished_up_to() >= 1 && use_bfs_inprocess)) {
                bool changed = false;
                int its = 0;
                int depth = std::max(std::min(h_splits_hint - 3, 16), 4);
                //int depth = h_splits_hint;
                do {
                    const int cell_last = local_state.c->cells;
                    worklist_t<unsigned long> inv(g->v_size);

                    nodes.reserve(g->v_size);
                    nodes.clear();

                    for (int i = 0; i < g->v_size; ++i) inv[i] = 0;
                    shallow_bfs_invariant(g, local_state, inv, orbit_partition, depth, !changed);
                    split_with_invariant(g, local_state, inv);

                    const int cell_after = local_state.c->cells;
                    changed = cell_after != cell_last;
                    if (changed) local_state.refine(g);
                    depth *= 2;
                    its += 1;
                } while(changed && g->v_size != local_state.c->cells && its < 3);
            }

            // computes a shallow breadth-first invariant for 2 levels
            if (use_shallow_quadratic_inprocess) {
                worklist_t<unsigned long> inv(g->v_size);

                nodes.reserve(g->v_size);
                nodes.clear();

                for(int i = 0; i < g->v_size; ++i) inv[i] = 0;
                shallow_bfs_invariant2(g, local_state, inv);
                split_with_invariant(g, local_state, inv);

                const int cell_after = local_state.c->cells;
                if (cell_after != cell_prev) {
                    local_state.refine(g);
                }
            }

            // applies the computed breadth-first levels as a node invariant
            if (tree.get_finished_up_to() >= 1 && use_bfs_inprocess) {
                markset is_pruned(g->v_size);
                tree.mark_first_level(is_pruned);
                tree.make_node_invariant(); // "compresses" node invariant from all levels into first level
                hash.allocate(g->v_size);

                nodes.reserve(g->v_size);
                nodes.clear();

                int num_of_hashs = 0;

                for(int i = 0; i < g->v_size; ++i) nodes.push_back(i);
                sort_nodes_map(tree.get_node_invariant(), local_state.c->vertex_to_col);
                unsigned long last_inv = (*tree.get_node_invariant())[0];
                int last_col           = local_state.c->vertex_to_col[0];
                for(int i = 0; i < g->v_size; ++i) {
                    const int v               = nodes[i];
                    const unsigned long v_inv = (*tree.get_node_invariant())[v];
                    const int  v_col = local_state.c->vertex_to_col[v];
                    if(last_col != v_col) {
                        last_col = v_col;
                        last_inv = v_inv;
                        ++num_of_hashs;
                    }
                    if(v_inv != last_inv) {
                        last_inv = v_inv;
                        ++num_of_hashs;
                    }
                    hash[v] = num_of_hashs;
                }

                g->initialize_coloring(local_state.c, hash.get_array());
                const int cell_after = local_state.c->cells;
                if (cell_after != cell_prev) {
                    local_state.refine(g);
                }
            }

            // performs individualizations if some of the initial cells of coloring coincide with their colors
            group.determine_potential_individualization(&inproc_can_individualize, local_state.get_coloring());
            if (!inproc_can_individualize.empty() || !inproc_maybe_individualize.empty()) {
                int num_inds = 0;
                for (auto &i: inproc_can_individualize) {
                    const int ind_v = i.first;
                    const int ind_col = local_state.c->vertex_to_col[ind_v];
                    assert(i.second == local_state.c->ptn[ind_col] + 1);
                    s_grp_sz.multiply(local_state.c->ptn[ind_col] + 1);
                    local_state.move_to_child_no_trace(g, ind_v);
                    inproc_fixed_points.push_back(ind_v);
                    ++num_inds;
                }
                for (auto &i: inproc_maybe_individualize) {
                    const int ind_v      = i.first;
                    const int ind_col    = local_state.c->vertex_to_col[ind_v];
                    const int ind_col_sz = local_state.c->ptn[ind_col] + 1;
                    const int orb_sz_det = i.second;
                    if(ind_col_sz > 1 && ind_col_sz == orb_sz_det) {
                        s_grp_sz.multiply(ind_col_sz);
                        local_state.move_to_child_no_trace(g, ind_v);
                        inproc_fixed_points.push_back(ind_v);
                        ++num_inds;
                    }
                }
                if(num_inds > 0) orbit_partition.reset();
            }

            inproc_can_individualize.clear();
            inproc_maybe_individualize.clear();

            // did inprocessing succeed? if so, save the new state and report the change
            if(cell_prev != local_state.c->cells) {
                local_state.save_reduced_state(root_save);
                touched_coloring = true;
            }

            return touched_coloring;
        }
    };
}

#endif //DEJAVU_INPROCESS_H
