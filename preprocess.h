#ifndef SASSY_H
#define SASSY_H

#define SASSY_VERSION_MAJOR 1
#define SASSY_VERSION_MINOR 1

#include "graph.h"
#include <vector>
#include <iomanip>
#include <ctime>

namespace sassy {
    // ring queue for pairs of integers
    class ring_pair {
        std::pair<int, int> *arr = 0;
        bool init = false;
        int arr_sz = -1;
        int front_pos = -1;
        int back_pos = -1;

    public:
        void initialize(int size) {
            if(init)
                delete[] arr;
            arr = new std::pair<int, int>[size];
            arr_sz = size;
            back_pos = 0;
            front_pos = 0;
            init = true;
        }

        void push_back(std::pair<int, int> value) {
            arr[back_pos] = value;
            back_pos = (back_pos + 1) % arr_sz;
        }

        std::pair<int, int> *front() {
            return &arr[front_pos];
        }

        void pop() {
            front_pos = (front_pos + 1) % arr_sz;
        }

        [[nodiscard]] bool empty() const {
            return (front_pos == back_pos);
        }

        ~ring_pair() {
            if (init)
                delete[] arr;
        }

        void reset() {
            front_pos = back_pos;
        }
    };

    enum selector_type {
        SELECTOR_FIRST, SELECTOR_LARGEST, SELECTOR_SMALLEST, SELECTOR_TRACES, SELECTOR_RANDOM
    };

    class selector {
        int skipstart = 0;
        int hint = -1;
        int hint_sz = -1;

        ring_pair largest_cache;
        dejavu::ds::work_list non_trivial_list;
        int init = false;

    public:
        int seeded_select_color(coloring *c, int seed) {
            std::vector<int> cells;
            for (int i = 0; i < c->ptn_sz;) {
                if (c->ptn[i] > 0) {
                    cells.push_back(i);
                }
                i += c->ptn[i] + 1;
            }
            if (cells.size() == 0) {
                return -1;
            } else {
                int target_cell = seed % cells.size();
                return cells[target_cell];
            }
        }

        int select_color_largest(coloring *c) {
            if (!init) {
                largest_cache.initialize(c->lab_sz);
                non_trivial_list.allocate(c->lab_sz);
                init = true;
            }

            while (!largest_cache.empty()) {
                std::pair<int, int> *elem = largest_cache.front();
                if (c->ptn[elem->first] == elem->second) {
                    assert(c->ptn[elem->first] > 0);
                    return elem->first;
                }
                largest_cache.pop();
            }

            int largest_cell = -1;
            int largest_cell_sz = -1;
            bool only_trivial = true;

            assert(skipstart < c->ptn_sz);
            for (int i = skipstart; i < c->ptn_sz;) {
                assert(c->vertex_to_col[c->lab[i]] == i);
                assert(i < c->ptn_sz);
                if (c->ptn[i] != 0 && only_trivial) {
                    skipstart = i;
                    only_trivial = false;
                }
                if (c->ptn[i] > largest_cell_sz && c->ptn[i] > 0) {
                    largest_cell = i;
                    largest_cell_sz = c->ptn[i];
                    largest_cache.reset();
                    largest_cache.push_back(std::pair<int, int>(i, c->ptn[i]));
                } else if (c->ptn[i] == largest_cell_sz) {
                    largest_cache.push_back(std::pair<int, int>(i, c->ptn[i]));
                }

                i += c->ptn[i] + 1;
            }
            assert(c->ptn[largest_cell] > 0);
            return largest_cell;
        }

        int select_color_smallest(coloring *c) {
            int smallest_cell = -1;
            int smallest_cell_sz = c->lab_sz + 1;
            bool only_trivial = true;
            for (int i = skipstart; i < c->ptn_sz; i += c->ptn[i] + 1) {
                if (c->ptn[i] != 0) {
                    if (only_trivial) {
                        skipstart = i;
                        only_trivial = false;
                    }
                    if ((c->ptn[i] + 1) < smallest_cell_sz) {
                        smallest_cell = i;
                        smallest_cell_sz = (c->ptn[i] + 1);
                        if (smallest_cell_sz == 2) {
                            break;
                        }
                    }
                }
            }
            return smallest_cell;
        }

        int select_color_first(coloring *c) {
            int first_cell = -1;

            for (int i = skipstart; i < c->ptn_sz;) {
                if (c->ptn[i] > 0) {
                    skipstart = i;
                    first_cell = i;
                    break;
                }
                i += c->ptn[i] + 1;
            }
            return first_cell;
        }

        void empty_cache() {
            skipstart = 0;
            hint = -1;
            hint_sz = -1;
            largest_cache.reset();
        }

        int
        select_color_dynamic(dejavu::sgraph *g, coloring *c, selector_type cell_selector_type) {
            if (c->cells == g->v_size)
                return -1;
            switch (cell_selector_type) {
                case SELECTOR_RANDOM:
                    return seeded_select_color(c, 0);
                case SELECTOR_LARGEST:
                    return select_color_largest(c);
                case SELECTOR_SMALLEST:
                    return select_color_smallest(c);
                case SELECTOR_TRACES:
                    return select_color_largest(c);
                case SELECTOR_FIRST:
                default:
                    return select_color_first(c);
            }
        }
    };

    class preprocessor;
    thread_local preprocessor* save_preprocessor;

    enum preop {
        deg01, deg2ue, deg2ma, qcedgeflip, probeqc, probe2qc, probeflat, redloop
    };

    // preprocessor for symmetry detection
    // see README.md for usage!
    class preprocessor {
    public:
        long double base = 1;
        int exp     = 0;
        int domain_size;

        bool CONFIG_PRINT = false;
        bool CONFIG_IR_FULL_INVARIANT = false; // uses a complete invariant and no certification if enabled
        bool CONFIG_IR_IDLE_SKIP = true;  // blueprints
        bool CONFIG_IR_INDIVIDUALIZE_EARLY = false; // experimental feature, based on an idea by Adolfo Piperno
        bool CONFIG_PREP_DEACT_PROBE = false; // preprocessor: no probing
        bool CONFIG_PREP_DEACT_DEG01 = false; // preprocessor: no degree 0,1 processing
        bool CONFIG_PREP_DEACT_DEG2 = false;  // preprocessor: no degree 2   processing
        bool CONFIG_IR_REFINE_EARLYOUT_LATE = false;
        bool CONFIG_TRANSLATE_ONLY = false;

    private:
        //inline static preprocessor* save_preprocessor;
        dejavu_hook*                 saved_hook;

        coloring c;
        dejavu::work_list automorphism;
        dejavu::work_list automorphism_supp;

        dejavu::work_list aux_automorphism;
        dejavu::work_list aux_automorphism_supp;

        bool layers_melded = false;

        bool skipped_preprocessing = false;

        dejavu::mark_set del;
        dejavu::mark_set del_e;

        dejavu::groups::orbit orbit;

        std::vector<std::vector<int>> translation_layers;
        std::vector<std::vector<int>> backward_translation_layers;

        std::vector<int> ind_cols;
        std::vector<int> cell_cnt;

        std::vector<int>              backward_translation;
        std::vector<std::vector<int>> recovery_strings;
        int avg_support_sparse_ir        = 0;
        int avg_reached_end_of_component = 0;
        double avg_end_of_comp           = 0.0;

        std::vector<std::vector<int>> add_edge_buff;
        dejavu::work_list worklist_deg0;
        dejavu::work_list worklist_deg1;
        dejavu::mark_set add_edge_buff_act;
        dejavu::ir::refinement* R1;

        dejavu::mark_set touched_color_cache;
        dejavu::work_list touched_color_list_cache;

        std::vector<int> g_old_v;
        dejavu::work_list edge_scratch;

        std::vector<int> translate_layer_fwd;
        std::vector<int> translate_layer_bwd;

        std::vector<int> quotient_component_worklist_col;
        std::vector<int> quotient_component_worklist_col_sz;
        std::vector<int> quotient_component_worklist_v;
        std::vector<std::pair<int, int>> quotient_component_worklist_boundary;
        std::vector<std::pair<int, int>> quotient_component_worklist_boundary_swap;
        std::vector<int> quotient_component_workspace;

        std::vector<int> quotient_component_touched;
        std::vector<int> quotient_component_touched_swap;

        dejavu::mark_set seen_vertex;
        dejavu::mark_set seen_color;
        bool init_quotient_arrays = false;
        std::vector<int> worklist;

        dejavu::work_list _automorphism;
        dejavu::work_list _automorphism_supp;
        std::vector<int> save_colmap;
        dejavu::mark_set  touched_color;
        dejavu::work_list touched_color_list;

        dejavu::work_list before_move;

        bool ir_quotient_component_init = false;

        std::vector<int> save_colmap_v_to_col;
        std::vector<int> save_colmap_v_to_lab;
        std::vector<int> save_colmap_ptn;
    public:
        // for a vertex v of reduced graph, return corresponding vertex of the original graph
        int translate_back(int v) {
            const int layers = static_cast<int>(translation_layers.size());
            for (int l = layers - 1; l >= 0; --l) {
                v = backward_translation_layers[l][v];
            }
            return v;
        }

        // keeps track of the group size generated by automorphisms found so far
        void multiply_to_group_size(int n) {
            base *= n;
            while (base > 10) {
                base = base / 10;
                exp += 1;
            }
        }

        // keeps track of the group size generated by automorphisms found so far
        void multiply_to_group_size(double _base, int _exp) {
            base *= _base;
            exp += _exp;
            if(std::fpclassify(base) == FP_INFINITE ||  std::fpclassify(base) == FP_NAN) {
                return;
            }
            while (base > 10) {
                base = base / 10;
                exp += 1;
            }
        }

    private:
        // combine translation layers
        void meld_translation_layers() {
            if (layers_melded)
                return;
            const int layers = static_cast<int>(translation_layers.size());
            if(layers == 0) {
                backward_translation_layers.emplace_back();
                backward_translation_layers[0].reserve(domain_size);
                for(int i = 0; i < domain_size; ++i) {
                    backward_translation_layers[0].push_back(i);
                }
                layers_melded = true;
                return;
            }


            backward_translation.reserve(layers - 1);
            const int reduced_size = static_cast<int>(backward_translation_layers[layers - 1].size());
            for (int i = 0; i < reduced_size; ++i)
                backward_translation.push_back(i);
            for (int i = 0; i < reduced_size; ++i) {
                int next_v = i;
                for (int l = layers - 1; l >= 0; --l) {
                    next_v = backward_translation_layers[l][next_v];
                }
                backward_translation[i] = next_v;
            }
            layers_melded = true;
        }

        // reset internal automorphism structure to the identity
        static void reset_automorphism(int *rautomorphism, int nsupp, const int *supp) {
            for (int i = 0; i < nsupp; ++i) {
                rautomorphism[supp[i]] = supp[i];
            }
        };

        // are vert1 and vert2 adjacent in g?
        static bool is_adjacent(dejavu::sgraph*g, int vert1, int vert2) {
            if(g->d[vert1] < g->d[vert2]) {
                for(int i = 0; i < g->d[vert1]; ++i) {
                    if(g->e[g->v[vert1] + i] == vert2)
                        return true;
                }
                return false;
            } else {
                for(int i = 0; i < g->d[vert2]; ++i) {
                    if(g->e[g->v[vert2] + i] == vert1)
                        return true;
                }
                return false;
            }
        }

        // check for degree 2 matchings between color classes and mark for deletion
        void red_deg2_path_size_1(dejavu::sgraph *g, int *colmap) {
            if (g->v_size <= 1 || CONFIG_PREP_DEACT_DEG2)
                return;

            del.reset();
            int found_match = 0;

            coloring test_col;
            g->initialize_coloring(&test_col, colmap);

            assure_ir_quotient_init(g);

            touched_color_cache.reset();

            worklist_deg0.reset();
            worklist_deg1.reset();

            for (int i = 0; i < g->v_size; ++i) {
                worklist_deg1.push_back(-1);
                worklist_deg0.push_back(0);
            }

            add_edge_buff_act.reset();
            //add_edge_buff.reserve(g->v_size);
            //for (int i = 0; i < g->v_size; ++i)
             //   add_edge_buff.emplace_back(); // could do this smarter... i know how many edges end up here

            for (int i = 0; i < g->v_size;) {
                const int test_v = test_col.lab[i];
                const int path_col_sz = test_col.ptn[i];
                if (g->d[test_v] == 2) {
                    const int n1 = g->e[g->v[test_v] + 0];
                    const int n2 = g->e[g->v[test_v] + 1];
                    if (g->d[n1] != 2 && g->d[n2] != 2) {
                        // relevant path of length 1
                        const int col_n1 = test_col.vertex_to_col[n1];
                        const int col_n2 = test_col.vertex_to_col[n2];

                        const int col_sz_n1 = test_col.ptn[test_col.vertex_to_col[n1]];
                        const int col_sz_n2 = test_col.ptn[test_col.vertex_to_col[n2]];

                        if (col_sz_n1 != col_sz_n2 || col_sz_n1 != path_col_sz || col_n1 == col_n2) {
                            i += test_col.ptn[i] + 1;
                            continue;
                        }

                        bool already_matched_n1_n2 = false;

                        const int already_match_pt1 = g->v[test_col.lab[col_n1]];
                        const int already_match_pt2 = g->v[test_col.lab[col_n2]];

                        if (touched_color_cache.get(col_n1) && touched_color_cache.get(col_n2)) {
                            for (int j = 0; j < worklist_deg0[col_n1]; ++j) {
                                if (edge_scratch[already_match_pt1 + j] == col_n2)
                                    already_matched_n1_n2 = true;
                            }
                        }

                        if (touched_color_cache.get(col_n1) && touched_color_cache.get(col_n2) &&
                            already_matched_n1_n2 && worklist_deg0[col_n1] == 1 && worklist_deg0[col_n2] == 1) {
                            // const bool matching_same_cols1 =
                            //         test_col.vertex_to_col[worklist_deg1[n1]] == test_col.vertex_to_col[n2];
                            // const bool matching_same_cols2 =
                            //        test_col.vertex_to_col[worklist_deg1[n2]] == test_col.vertex_to_col[n1];
                            // const bool match_same_cols = matching_same_cols1 && matching_same_cols2;

                            bool check_if_match = true;
                            for (int f = 0; f < test_col.ptn[i] + 1; ++f) {
                                const int _test_v = test_col.lab[i + f];
                                const int _n1 = g->e[g->v[_test_v] + 0];
                                const int _n2 = g->e[g->v[_test_v] + 1];
                                check_if_match = check_if_match && (worklist_deg1[_n1] == _n2);
                                if (!check_if_match)
                                    break;
                            }

                            if (check_if_match) {
                                found_match += test_col.ptn[i] + 1;
                                for (int f = 0; f < test_col.ptn[i] + 1; ++f) {
                                    const int _test_v = test_col.lab[i + f];
                                    del.set(_test_v);
                                }
                                for (int f = 0; f < test_col.ptn[i] + 1; ++f) {
                                    const int _test_v = test_col.lab[i + f];
                                    const int _n1 = g->e[g->v[_test_v] + 0];
                                    const int _n2 = g->e[g->v[_test_v] + 1];
                                    int can_n;
                                    if (test_col.vertex_to_col[_n1] < test_col.vertex_to_col[_n2])
                                        can_n = _n1;
                                    else
                                        can_n = _n2;
                                    const int orig_test_v = translate_back(_test_v);
                                    const int orig_n1 = translate_back(can_n);
                                    recovery_strings[orig_n1].push_back(orig_test_v);
                                    for (size_t s = 0; s < recovery_strings[orig_test_v].size(); ++s)
                                        recovery_strings[orig_n1].push_back(
                                                recovery_strings[orig_test_v][s]);
                                }
                            }


                            i += test_col.ptn[i] + 1;
                            continue;
                        }

                        if (touched_color_cache.get(col_n1) && touched_color_cache.get(col_n2) &&
                            already_matched_n1_n2) {
                            i += test_col.ptn[i] + 1;
                            continue;
                        }

                        const int col_endpoint2 = colmap[n2]; // colmap?
                        bool col_cycle = false;
                        for (int f = 0; f < g->d[n1]; ++f) {
                            const int col_other = colmap[g->e[g->v[n1] + f]];
                            if (col_other == col_endpoint2) {
                                col_cycle = true;
                                break;
                            }
                        }

                        if (col_cycle) {
                            i += test_col.ptn[i] + 1;
                            continue;
                        }

                        edge_scratch[already_match_pt1 +
                                     worklist_deg0[col_n1]] = col_n2; // overwrites itself, need canonical vertex for color
                        edge_scratch[already_match_pt2 + worklist_deg0[col_n2]] = col_n1;
                        ++worklist_deg0[col_n1];
                        ++worklist_deg0[col_n2];

                        touched_color_cache.set(col_n1);
                        touched_color_cache.set(col_n2);

                        found_match += test_col.ptn[i] + 1;

                        //const size_t debug_str_sz = recovery_strings[translate_back(test_v)].size();

                        for (int f = 0; f < test_col.ptn[i] + 1; ++f) {
                            const int _test_v = test_col.lab[i + f];
                            assert(g->d[_test_v] == 2);
                            const int _n1 = g->e[g->v[_test_v] + 0];
                            const int _n2 = g->e[g->v[_test_v] + 1];
                            worklist_deg1[_n1] = _n2;
                            worklist_deg1[_n2] = _n1;
                            for (size_t t = 0; t < add_edge_buff[_n2].size(); ++t)
                                assert(add_edge_buff[_n2][t] != _n1);
                            add_edge_buff[_n2].push_back(_n1);
                            add_edge_buff_act.set(_n2);
                            add_edge_buff[_n1].push_back(_n2);
                            add_edge_buff_act.set(_n1);
                            del.set(_test_v);

                            int can_n;
                            if (test_col.vertex_to_col[_n1] < test_col.vertex_to_col[_n2])
                                can_n = _n1;
                            else
                                can_n = _n2;
                            const int orig_test_v = translate_back(_test_v);
                            const int orig_n1 = translate_back(can_n);
                            //assert(debug_str_sz == recovery_strings[orig_test_v].size());
                            recovery_strings[orig_n1].push_back(orig_test_v);
                            for (size_t s = 0; s < recovery_strings[orig_test_v].size(); ++s) {
                                recovery_strings[orig_n1].push_back(recovery_strings[orig_test_v][s]);
                            }
                        }
                    }
                }
                i += test_col.ptn[i] + 1;
            }

            worklist_deg0.reset();
            worklist_deg1.reset();
            touched_color_cache.reset();
            //PRINT("(prep-red) deg2_matching, vertices deleted: " << found_match << "/" << g->v_size);
        }

        // in g, walk from 'start' (degree 2) until vertex of not-degree 2 is reached
        // never walks to 'block', if adjacent to 'start'
        // watch out! won't terminate on cycles
        static int walk_cycle(dejavu::sgraph *g, const int start, const int block, dejavu::mark_set* path_done,
                              dejavu::work_list* path) {
            int current_vertex = start;
            int last_vertex    = block;

            int length = 1;

            if(path)
                path->push_back(block);
            if(path_done)
                path_done->set(block);

            while(true) {
                if (g->d[current_vertex] != 2) {
                    length = -1;
                    break;
                }
                ++length;

                if(path)
                    path->push_back(current_vertex);

                if(path_done)
                    path_done->set(current_vertex);

                const int next_vertex1 = g->e[g->v[current_vertex] + 0];
                const int next_vertex2 = g->e[g->v[current_vertex] + 1];
                int next_vertex = next_vertex1;
                if(next_vertex1 == last_vertex)
                    next_vertex = next_vertex2;

                if(next_vertex == block)
                    break;

                last_vertex    = current_vertex;
                current_vertex = next_vertex;
            }

            return length;
        }

        // in g, walk from 'start' (degree 2) until vertex of not-degree 2 is reached
        // never walks to 'block', if adjacent to 'start'
        // watch out! won't terminate on cycles
        static std::pair<int, int> walk_to_endpoint(dejavu::sgraph *g, const int start, const int block,
                                                    dejavu::mark_set* path_done) {
            int current_vertex = start;
            int last_vertex    = block;

            while(g->d[current_vertex] == 2) {
                if(path_done)
                    path_done->set(current_vertex);

                const int next_vertex1 = g->e[g->v[current_vertex] + 0];
                const int next_vertex2 = g->e[g->v[current_vertex] + 1];
                int next_vertex = next_vertex1;
                if(next_vertex1 == last_vertex)
                    next_vertex = next_vertex2;

                last_vertex    = current_vertex;
                current_vertex = next_vertex;
            }

            return {current_vertex, last_vertex};
        }

        void order_edgelist(dejavu::sgraph *g) {
            memcpy(edge_scratch.get_array(), g->e, g->e_size*sizeof(int));

            int epos = 0;
            for(int i = 0; i < g->v_size; ++i) {
                const int eptr = g->v[i];
                const int deg  = g->d[i];
                g->v[i] = epos;
                for(int j = eptr; j < eptr + deg; ++j) {
                    g->e[epos] = edge_scratch[j];
                    ++epos;
                }
            }
        }

        // in g, walk from 'start' (degree 2) until vertex of not-degree 2 is reached
        // never walks to 'block', if adjacent to 'start'
        // watch out! won't terminate on cycles
        static int walk_to_endpoint_collect_path(dejavu::sgraph *g, const int start, const int block, dejavu::work_list* path) {
            int current_vertex = start;
            int last_vertex    = block;

            while(g->d[current_vertex] == 2) {
                if(path)
                    path->push_back(current_vertex);

                const int next_vertex1 = g->e[g->v[current_vertex] + 0];
                const int next_vertex2 = g->e[g->v[current_vertex] + 1];
                int next_vertex = next_vertex1;
                if(next_vertex1 == last_vertex)
                    next_vertex = next_vertex2;

                last_vertex    = current_vertex;
                current_vertex = next_vertex;
            }

            assert(g->d[current_vertex] != 2);
            return current_vertex;
        }

        // color-wise degree2 unique endpoint algorithm
        void red_deg2_unique_endpoint_new(dejavu::sgraph *g, int *colmap) {
            if (g->v_size <= 1 || CONFIG_PREP_DEACT_DEG2)
                return;

            dejavu::mark_set color_test(g->v_size);

            dejavu::mark_set color_unique(g->v_size);

            coloring col;
            g->initialize_coloring(&col, colmap);

            add_edge_buff_act.reset();
            for (int i = 0; i < g->v_size; ++i) {
                add_edge_buff[i].clear();
            }

            del.reset();

            worklist_deg1.reset();

            dejavu::work_list endpoint_cnt(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                endpoint_cnt.push_back(0);
            }

            dejavu::mark_set path_done(g->v_size);
            dejavu::work_list color_pos(g->v_size);
            dejavu::work_list filter(g->v_size);
            dejavu::work_list not_unique(g->v_size);
            dejavu::work_list not_unique_analysis(g->v_size);
            dejavu::work_list path_list(g->v_size);
            dejavu::work_list path(g->v_size);
            dejavu::work_list connected_paths(g->e_size);
            dejavu::work_list connected_endpoints(g->e_size);

            // collect and count endpoints
            int total_paths = 0;
            int total_deleted_vertices = 0;

            for (int i = 0; i < g->v_size; ++i) {
                if(g->d[i] == 2) {
                    if(path_done.get(i))
                        continue;
                    const int n1 = g->e[g->v[i] + 0];
                    const int n2 = g->e[g->v[i] + 1];
                    // n1 or n2 is endpoint? then walk to other endpoint and collect information...
                    if(g->d[n1] != 2) {
                        const auto other_endpoint = walk_to_endpoint(g, i, n1, &path_done);
                        if(other_endpoint.first == n1) // big self-loop
                            continue;
                        connected_paths[g->v[n1] + endpoint_cnt[n1]]     = i;
                        connected_endpoints[g->v[n1] + endpoint_cnt[n1]] = other_endpoint.first;
                        ++endpoint_cnt[n1];
                        connected_paths[g->v[other_endpoint.first]     + endpoint_cnt[other_endpoint.first]] = other_endpoint.second;
                        connected_endpoints[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = n1;
                        ++endpoint_cnt[other_endpoint.first];
                        assert(other_endpoint.first != n1);
                        assert(g->d[other_endpoint.first] != 2);
                        assert(n1 != other_endpoint.first);
                        ++total_paths;
                    } else if(g->d[n2] != 2) {
                        const auto other_endpoint = walk_to_endpoint(g, i, n2, &path_done);
                        if(other_endpoint.first == n2) // big self-loop
                            continue;
                        connected_paths[g->v[n2] + endpoint_cnt[n2]] = i;
                        connected_endpoints[g->v[n2] + endpoint_cnt[n2]] = other_endpoint.first;
                        ++endpoint_cnt[n2];
                        connected_paths[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = other_endpoint.second;
                        connected_endpoints[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = n2;
                        ++endpoint_cnt[other_endpoint.first];
                        assert(other_endpoint.first != n2);
                        assert(g->d[other_endpoint.first] != 2);
                        assert(n2 != other_endpoint.first);
                        ++total_paths;
                    }
                }
            }

            path_done.reset();

            // iterate over color classes
            for(int i = 0; i < g->v_size;) {
                const int color      = i;
                const int color_size = col.ptn[color] + 1;
                i += color_size;
                const int test_vertex = col.lab[color];
                const int endpoints   = endpoint_cnt[test_vertex];

                //if(endpoints != 1)
                //    continue;

                for(int j = 0; j < color_size; ++j) {
                    assert(endpoint_cnt[col.lab[color + j]] == endpoints);
                }

                if(endpoints == 0) {
                    continue;
                }

                color_test.reset();
                color_unique.reset();

                // check which neighbouring paths have unique color
                for(int j = 0; j < endpoints; ++j) {
                    const int neighbour     = connected_paths[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    if(color_test.get(neighbour_col)) {
                        color_unique.set(neighbour_col); // means "not unique"
                    }
                    color_test.set(neighbour_col);
                }

                filter.reset();
                not_unique.reset();
                // filter to indices with unique colors
                for(int j = 0; j < endpoints; ++j) {
                    const int neighbour     = connected_paths[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    if(!color_unique.get(neighbour_col)) { // if unique
                        filter.push_back(j); // add index to filter
                    } else {
                        not_unique.push_back(neighbour);
                        not_unique.push_back(connected_endpoints[g->v[test_vertex] + j]);
                    }
                }

                path.reset();
                color_test.reset();
                color_unique.reset();

                // check which neighbouring endpoints have unique color and are NOT connected to `color`
                for(int j = 0; j < g->d[test_vertex]; ++j) {
                    const int neighbour     = g->e[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    color_test.set(neighbour_col);
                }

                // also self-connections of color are forbidden
                color_test.set(color);

                for(int k = 0; k < filter.cur_pos; ++k) {
                    const int j = filter[k];
                    const int neighbour_endpoint = connected_endpoints[g->v[test_vertex] + j];
                    const int neighbour_col      = col.vertex_to_col[neighbour_endpoint];
                    if(color_test.get(neighbour_col)) {
                        color_unique.set(neighbour_col);
                    }
                    color_test.set(neighbour_col);
                }

                // filter to indices with unique colors
                int write_pos = 0;
                for(int k = 0; k < filter.cur_pos; ++k) {
                    const int j = filter[k];
                    const int neighbour     = connected_endpoints[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    if(!color_unique.get(neighbour_col)) {
                        filter[write_pos] = j;
                        ++write_pos;
                    }
                }
                filter.cur_pos = write_pos;


                // encode filter into color_unique, such that we can detect corresponding neighbours for all vertices
                // of color class
                color_unique.reset();
                for(int k = 0; k < filter.cur_pos; ++k) {
                    const int j = filter[k];
                    const int filter_to_col = col.vertex_to_col[connected_paths[g->v[test_vertex] + j]];
                    color_unique.set(filter_to_col);
                    filter[k] = filter_to_col;
                    //color_unique.set(col.vertex_to_col[connected_endpoints[filter[k]]]);
                }

                // order colors here already to access color_pos in O(1) later
                filter.sort();
                for(int k = 0; k < filter.cur_pos; ++k) {
                    color_pos[filter[k]] = k;
                }

                const int num_paths = filter.cur_pos;

                // do next steps for all vertices in color classes...
                int reduced_verts = 0;
                [[maybe_unused]] int reduced_verts_last = 0;

                for(int j = 0; j < color_size; ++j) {
                    const int vertex = col.lab[color + j];

                    [[maybe_unused]] int sanity_check = 0;
                    // paths left in filter (color_unique) are now collected for reduction
                    color_test.reset();
                    for(int k = 0; k < g->d[vertex]; ++k) {
                        const int neighbour     = g->e[g->v[vertex] + k];
                        const int neighbour_col = col.vertex_to_col[neighbour];
                        if(color_unique.get(neighbour_col)) {
                            const int pos = color_pos[neighbour_col];
                            path_list[pos] = neighbour;
                            ++sanity_check;
                            assert(!color_test.get(neighbour_col));
                            color_test.set(neighbour_col);
                        }
                    }

                    assert(sanity_check == num_paths);

                    for(int k = 0; k < num_paths; ++k) {
                        reduced_verts = 0;
                        const int path_start_vertex = path_list[k];
                        assert(g->d[path_start_vertex] == 2);
                        if(path_done.get(path_start_vertex)) {
                            continue;
                        }

                        // get path and endpoint
                        path.reset();
                        const int other_endpoint =
                                walk_to_endpoint_collect_path(g, path_start_vertex, vertex, &path);
                        assert(path.cur_pos > 0);
                        assert(vertex != other_endpoint);
                        assert(other_endpoint != path_start_vertex);

                        // mark path for deletion
                        for(int l = 0; l < path.cur_pos; ++l) {
                            const int del_v = path[l];
                            assert(g->d[del_v] == 2);
                            assert(!del.get(del_v));
                            del.set(del_v);
                            assert(!path_done.get(del_v));
                            path_done.set(del_v);
                            ++total_deleted_vertices;
                        }

                        // connect endpoints of path with new edge
                        assert(!is_adjacent(g, vertex, other_endpoint));
                        add_edge_buff[other_endpoint].push_back(vertex);
                        add_edge_buff_act.set(other_endpoint);
                        add_edge_buff[vertex].push_back(other_endpoint);
                        add_edge_buff_act.set(vertex);

                        // write path into recovery_strings
                        const int unique_endpoint_orig = translate_back(vertex);
                        // attach all represented vertices of path to unique_endpoint_orig in canonical fashion
                        //path.sort_after_map(colmap); // should not be necessary
                        recovery_strings[unique_endpoint_orig].reserve(2*(recovery_strings[unique_endpoint_orig].size() + path.cur_pos));
                        for (int l = 0; l < path.cur_pos; ++l) {
                            assert(path[l] >= 0);
                            assert(path[l] < g->v_size);
                            const int path_v_orig = translate_back(path[l]);
                            assert(path_v_orig >= 0);
                            assert(path_v_orig < domain_size);
                            recovery_strings[unique_endpoint_orig].push_back(path_v_orig);
                            recovery_strings[unique_endpoint_orig].insert(
                                    recovery_strings[unique_endpoint_orig].end(),
                                    recovery_strings[path_v_orig].begin(),
                                    recovery_strings[path_v_orig].end());
                        }
                    }

                    reduced_verts = static_cast<int>(recovery_strings[translate_back(vertex)].size());

                    if(j > 0) {
                        assert(reduced_verts == reduced_verts_last);
                    }
                    reduced_verts_last = reduced_verts;
                }
            }

            //PRINT("(prep-red) deg2_unique_endpoint, vertices deleted: " << total_deleted_vertices << "/" << g->v_size);
        }

        // color cycles according to their size
        // remove uniform colored cycles
        static void red_deg2_color_cycles(dejavu::sgraph *g, int *colmap) {
            coloring col;
            g->initialize_coloring(&col, colmap);
            for(int i = 0; i < g->v_size; ++i) {
                colmap[i] = col.vertex_to_col[i];
            }

            dejavu::mark_set path_done(g->v_size);
            dejavu::work_list cycle_length(g->v_size);
            dejavu::work_list recolor_nodes(g->v_size);
            dejavu::work_list path(g->v_size);

            for (int i = 0; i < g->v_size; ++i) {
                if(g->d[i] == 2) {
                    if(path_done.get(i))
                        continue;
                    const int n1 = g->e[g->v[i] + 0];
                    const int n2 = g->e[g->v[i] + 1];

                    if(g->d[n1] != 2 || g->d[n2] != 2)
                        continue;

                    path.reset();
                    int cycle_length = walk_cycle(g, i, n1, &path_done, &path);
                    if(cycle_length == -1) {
                        continue;
                    } else {
                        for(int j = 0; j < path.cur_pos; ++j) {
                            colmap[path[j]] = colmap[path[j]] + g->v_size * cycle_length; // maybe this should receive a more elegant solution
                        }
                    }
                }
            }
        }

        // TODO: re-write algorithm color-wise
        void red_deg2_trivial_connect(dejavu::sgraph* g, int* colmap) {
            if (g->v_size <= 1 || CONFIG_PREP_DEACT_DEG2)
                return;

            dejavu::mark_set color_test(g->v_size);
            dejavu::mark_set color_unique(g->v_size);

            coloring col;
            g->initialize_coloring(&col, colmap);

            add_edge_buff_act.reset();
            for (int i = 0; i < g->v_size; ++i) {
                add_edge_buff[i].clear();
            }

            del.reset();

            worklist_deg1.reset();

            dejavu::work_list endpoint_cnt(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                endpoint_cnt.push_back(0);
            }

            dejavu::mark_set path_done(g->v_size);
            dejavu::work_list color_pos(g->v_size);
            dejavu::work_list filter(g->v_size);
            dejavu::work_list not_unique(2*g->v_size);
            dejavu::work_list not_unique_analysis(g->v_size);
            dejavu::work_list path(g->v_size);
            dejavu::work_list connected_paths(g->e_size);
            dejavu::work_list connected_endpoints(g->e_size);
            dejavu::work_list neighbour_list(g->v_size);
            dejavu::work_list neighbour_to_endpoint(g->v_size);

            for (int i = 0; i < g->v_size; ++i) {
                if(g->d[i] == 2) {
                    if(path_done.get(i))
                        continue;
                    const int n1 = g->e[g->v[i] + 0];
                    const int n2 = g->e[g->v[i] + 1];
                    // n1 or n2 is endpoint? then walk to other endpoint and collect information...
                    if(g->d[n1] != 2) {
                        const auto other_endpoint = walk_to_endpoint(g, i, n1, &path_done);
                        if(other_endpoint.first == n1) // big self-loop
                            continue;
                        connected_paths[g->v[n1] + endpoint_cnt[n1]]     = i;
                        connected_endpoints[g->v[n1] + endpoint_cnt[n1]] = other_endpoint.first;
                        ++endpoint_cnt[n1];
                        connected_paths[g->v[other_endpoint.first]     + endpoint_cnt[other_endpoint.first]] = other_endpoint.second;
                        connected_endpoints[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = n1;
                        ++endpoint_cnt[other_endpoint.first];
                        assert(other_endpoint.first != n1);
                        assert(g->d[other_endpoint.first] != 2);
                        assert(n1 != other_endpoint.first);
                    } else if(g->d[n2] != 2) {
                        const auto other_endpoint = walk_to_endpoint(g, i, n2, &path_done);
                        if(other_endpoint.first == n2) // big self-loop
                            continue;
                        connected_paths[g->v[n2] + endpoint_cnt[n2]] = i;
                        connected_endpoints[g->v[n2] + endpoint_cnt[n2]] = other_endpoint.first;
                        ++endpoint_cnt[n2];
                        connected_paths[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = other_endpoint.second;
                        connected_endpoints[g->v[other_endpoint.first] + endpoint_cnt[other_endpoint.first]] = n2;
                        ++endpoint_cnt[other_endpoint.first];
                        assert(other_endpoint.first != n2);
                        assert(g->d[other_endpoint.first] != 2);
                        assert(n2 != other_endpoint.first);
                    }
                }
            }

            path_done.reset();

            // iterate over color classes
            for(int i = 0; i < g->v_size;) {
                const int color = i;
                const int color_size = col.ptn[color] + 1;
                i += color_size;
                const int test_vertex = col.lab[color];
                const int endpoints = endpoint_cnt[test_vertex];

                //if(endpoints != 1)
                //    continue;

                /*for (int j = 0; j < color_size; ++j) {
                    const int other_from_color = col.lab[color + j];
                    assert(endpoint_cnt[other_from_color] == endpoints);
                }*/

                if (endpoints == 0) {
                    continue;
                }

                color_test.reset();
                color_unique.reset();

                // check which neighbouring paths have unique color
                for (int j = 0; j < endpoints; ++j) {
                    const int neighbour = connected_paths[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    if (color_test.get(neighbour_col)) {
                        color_unique.set(neighbour_col); // means "not unique"
                    }
                    color_test.set(neighbour_col);
                }

                filter.reset();
                not_unique.reset();
                // filter to indices with unique colors
                for (int j = 0; j < endpoints; ++j) {
                    const int neighbour = connected_paths[g->v[test_vertex] + j];
                    const int neighbour_col = col.vertex_to_col[neighbour];
                    if (!color_unique.get(neighbour_col)) { // if unique
                        filter.push_back(j); // add index to filter
                    } else {
                        not_unique.push_back(neighbour);
                        assert(connected_endpoints[g->v[test_vertex] + j] >= 0);
                        assert(connected_endpoints[g->v[test_vertex] + j] < g->v_size);
                        not_unique.push_back(connected_endpoints[g->v[test_vertex] + j]);
                    }
                }

                color_test.reset();
                color_unique.reset();

                // remove trivial connections
                for (int kk = 0; kk < not_unique.cur_pos; kk += 2) {
                    const int endpoint = not_unique[kk + 1];
                    const int endpoint_col = col.vertex_to_col[endpoint];
                    not_unique_analysis[endpoint_col] = 0;
                }
                for (int kk = 0; kk < not_unique.cur_pos; kk += 2) {
                    const int endpoint = not_unique[kk + 1];
                    const int endpoint_col = col.vertex_to_col[endpoint];
                    ++not_unique_analysis[endpoint_col];
                }
                for (int kk = 0; kk < not_unique.cur_pos; kk += 2) {
                    const int neighbour = not_unique[kk];
                    const int endpoint  = not_unique[kk + 1];
                    const int endpoint_col    = col.vertex_to_col[endpoint];
                    [[maybe_unused]] const int endpoint_col_sz = col.ptn[endpoint_col] + 1;
                    path.reset();
                    if (!color_test.get(endpoint_col)) {
                        color_test.set(endpoint_col);

                        if (not_unique_analysis[endpoint_col] == col.ptn[endpoint_col] + 1) {
                            // check that path endpoints dont contain duplicates
                            bool all_unique = true;
                            color_unique.reset();
                            for (int jj = 1; jj < not_unique.cur_pos; jj += 2) {
                                const int _endpoint = not_unique[jj];
                                const int _endpoint_col = col.vertex_to_col[endpoint];
                                if (_endpoint_col == endpoint_col) {
                                    if (color_unique.get(_endpoint)) {
                                        all_unique = false;
                                        break;
                                    }
                                    color_unique.set(_endpoint);
                                }
                            }

                            if (all_unique && col.ptn[endpoint_col] + 1 == not_unique_analysis[endpoint_col] && color < endpoint_col) { // col.ptn[endpoint_col] + 1 == 2 && color_size == 2 && only_once
                                // TODO: make sure it's not doubly-connected to one of the vertices (need to check this for every vertex, actually)?
                                const int path_col = col.vertex_to_col[neighbour];
                                [[maybe_unused]] const int path_col_sz = col.ptn[path_col] + 1;

                                assert(path_col_sz == (not_unique_analysis[endpoint_col] * color_size));
                                assert(endpoint_col_sz == not_unique_analysis[endpoint_col]);


                                int endpoint_neighbour_col = -1;

                                for(int jjj = 0; jjj < color_size; ++jjj ) {
                                    const int col1_vertj = col.lab[color + jjj];

                                    // now go through and remove paths...
                                    neighbour_list.reset();
                                    for (int jj = 0; jj < g->d[col1_vertj]; ++jj) {
                                        const int _neighbour = g->e[g->v[col1_vertj] + jj];
                                        const int _neighbour_col = col.vertex_to_col[_neighbour];
                                        //const int _neighbour_col_sz = col.ptn[_neighbour_col] + 1;

                                        if (_neighbour_col == path_col) {
                                            neighbour_list.push_back(_neighbour);
                                            const auto other_endpoint = walk_to_endpoint(g, _neighbour, col1_vertj, nullptr);
                                            neighbour_to_endpoint[_neighbour] = other_endpoint.first;
                                            if(endpoint_neighbour_col == -1) {
                                                endpoint_neighbour_col = col.vertex_to_col[other_endpoint.second];
                                            }
                                        }
                                    }

                                    neighbour_list.sort_after_map(neighbour_to_endpoint.get_array());

                                    // now go through and remove paths...
                                    for (int jj = 0; jj < neighbour_list.cur_pos; ++jj) {
                                        const int _neighbour = neighbour_list[jj];

                                        path.reset();
                                        int _endpoint = walk_to_endpoint_collect_path(g, _neighbour,
                                                                                      col1_vertj,
                                                                                      &path);

                                        // mark path for deletion
                                        for (int l = 0; l < path.cur_pos; ++l) {
                                            const int del_v = path[l];
                                            assert(g->d[del_v] == 2);
                                            assert(!del.get(del_v));
                                            del.set(del_v);
                                            assert(!path_done.get(del_v));
                                            path_done.set(del_v);
                                        }

                                        const int vert_orig = translate_back(col1_vertj);

                                        recovery_strings[vert_orig].reserve(
                                                recovery_strings[vert_orig].size() +
                                                path.cur_pos);
                                        for (int l = 0; l < path.cur_pos; ++l) {
                                            assert(path[l] >= 0);
                                            assert(path[l] < g->v_size);
                                            const int path_v_orig = translate_back(path[l]);
                                            assert(path_v_orig >= 0);
                                            assert(path_v_orig < domain_size);
                                            recovery_strings[vert_orig].push_back(path_v_orig);
                                            for(size_t rsi = 0; rsi < recovery_strings[path_v_orig].size(); ++rsi) {
                                                recovery_strings[vert_orig].push_back(recovery_strings[path_v_orig][rsi]);
                                            }
                                        }


                                        // write path into recovery_string of _endpoint
                                        const int endpoint_orig = translate_back(_endpoint);
                                        recovery_strings[endpoint_orig].reserve(
                                                recovery_strings[endpoint_orig].size() +
                                                path.cur_pos);
                                        for (int l = 0; l < path.cur_pos; ++l) {
                                            assert(path[l] >= 0);
                                            assert(path[l] < g->v_size);
                                            const int path_v_orig = translate_back(path[l]);
                                            assert(path_v_orig >= 0);
                                            assert(path_v_orig < domain_size);
                                            recovery_strings[endpoint_orig].push_back(-path_v_orig);
                                            for(size_t rsi = 0; rsi < recovery_strings[path_v_orig].size(); ++rsi) {
                                                recovery_strings[endpoint_orig].push_back(-abs(recovery_strings[path_v_orig][rsi]));
                                            }
                                        }

                                        assert(col.vertex_to_col[_endpoint] == endpoint_col);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        void write_canonical_recovery_string_to_automorphism(const int from, const int to) {
            assert(recovery_strings[from].size() == recovery_strings[to].size());
            for (size_t i = 0; i < recovery_strings[to].size(); ++i) {
                const int str_from = recovery_strings[from][i];
                const int str_to = recovery_strings[to][i];
                automorphism[str_to] = str_from;
                automorphism[str_from] = str_to;
                automorphism_supp.push_back(str_from);
                automorphism_supp.push_back(str_to);
            }
        }

        // reduce vertices of degree 1 and 0, outputs corresponding automorphisms
        void red_deg10_assume_cref(dejavu::sgraph *g, int *colmap, dejavu_hook* consume) {
            g->initialize_coloring(&c, colmap);
            if (CONFIG_PREP_DEACT_DEG01)
                return;

            worklist_deg0.reset();
            worklist_deg1.reset();

            dejavu::mark_set is_parent(g->v_size);

            g_old_v.clear();
            g_old_v.reserve(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                g_old_v.push_back(g->d[i]);
            }

            dejavu::work_list pair_match(g->v_size);
            dejavu::work_list parentlist(g->v_size);
            dejavu::work_list childcount(g->v_size);
            for (int i = 0; i < g->v_size; ++i)
                childcount.push_back(0);

            dejavu::work_list childcount_prev(g->v_size);
            for (int i = 0; i < g->v_size; ++i)
                childcount_prev.push_back(0);

            dejavu::work_list_t<std::pair<int, int>> stack1(g->v_size);
            dejavu::work_list map(g->v_size);

            assert(_automorphism_supp.cur_pos == 0);


            for (int i = 0; i < c.ptn_sz;) {
                const int v = c.lab[i];
                switch (g_old_v[v]) {
                    case 0:
                        worklist_deg0.push_back(v);
                        break;
                    case 1:
                        if (c.ptn[c.vertex_to_col[v]] > 0)
                            worklist_deg1.push_back(v);
                        break;
                    default:
                        break;
                }
                i += c.ptn[i] + 1;
            }

            while (!worklist_deg1.empty()) {
                const int v_child = worklist_deg1.pop_back();
                if (del.get(v_child))
                    continue;
                if (g_old_v[v_child] != 1)
                    continue;

                const int v_child_col = c.vertex_to_col[v_child];
                const int child_col_sz = c.ptn[v_child_col] + 1;
                bool is_pairs = false;
                bool permute_parents_instead = false;
                if (child_col_sz == 1) {
                    del.set(v_child);
                    continue;
                } else {
                    parentlist.reset();
                    is_parent.reset();
                    //int last_parent_child_count = -1;
                    for (int i = v_child_col; i < v_child_col + child_col_sz; ++i) {
                        int child = c.lab[i];
                        //int next_child = -1;
                        //if (i < v_child_col + child_col_sz - 1)
                        //    next_child = c.lab[i + 1];

                        // search for parent
                        const int e_pos_child = g->v[child];
                        int parent = g->e[e_pos_child];

                        if (is_pairs && del.get(child))
                            continue;

                        int search_parent = 0;
                        while (del.get(parent)) {
                            ++search_parent;
                            parent = g->e[e_pos_child + search_parent];
                        }

                        assert(is_pairs ? c.vertex_to_col[parent] == c.vertex_to_col[child] : true);
                        if (c.vertex_to_col[parent] == c.vertex_to_col[child]) {
                            is_pairs = true;
                            del.set(child);
                            del.set(parent);
                            pair_match[child] = parent;
                            pair_match[parent] = child;
                            if (parent < child) {
                                parentlist.push_back(parent);
                            } else {
                                parentlist.push_back(child);
                            }
                            continue;
                        }

                        del.set(child);

                        // save canonical info for parent
                        edge_scratch[g->v[parent] + childcount[parent]] = child;

                        ++childcount[parent];

                        if (!is_parent.get(parent)) {
                            is_parent.set(parent);
                            childcount_prev[parent] = childcount[parent] - 1;
                            parentlist.push_back(parent);
                        }

                        // adjust parent degree
                        g_old_v[parent] -= 1;
                        if (g_old_v[parent] == 1 && i == v_child_col) {
                            worklist_deg1.push_back(parent);
                        } else if (g_old_v[parent] == 0 && i == v_child_col) {
                            worklist_deg0.push_back(parent);
                        }

                        assert(g_old_v[parent] >= 0);
                    }
                }

                if (is_pairs) {
                    for (int j = 0; j < parentlist.cur_pos; ++j) {
                        const int first_pair_parent = parentlist[j];
                        const int pair_from = first_pair_parent; // need to use both childlist and canonical recovery again
                        const int pair_to = pair_match[pair_from];

                        stack1.reset();
                        map.reset();
                        map.push_back(pair_from);
                        stack1.push_back(std::pair<int, int>(g->v[pair_from], g->v[pair_from] + childcount[pair_from]));
                        while (!stack1.empty()) {
                            std::pair<int, int> from_to = stack1.pop_back();
                            int from = from_to.first;
                            const int to = from_to.second;
                            for (int f = from; f < to; ++f) {
                                const int next = edge_scratch[f];
                                //const int next = g->e[f];
                                const int from_next = g->v[next];
                                const int to_next = g->v[next] + childcount[next];
                                map.push_back(next);
                                assert(next != pair_to);
                                if (from_next != to_next)
                                    stack1.push_back(std::pair<int, int>(from_next, to_next));
                            }
                        }

                        multiply_to_group_size(2);

                        assert(c.vertex_to_col[pair_from] == c.vertex_to_col[pair_to]);
                        assert(pair_from != pair_to);
                        int pos = 0;

                        //automorphism_supp.reset();
                        // descending shared_tree of child_to while writing automorphism
                        stack1.reset();
                        assert(map[pos] != pair_to);
                        //const int to_1 = translate_back(pair_to);
                        //const int from_1 = translate_back(map[pos]);
                        const int v_to_1   = pair_to;
                        const int v_from_1 = map[pos];
                        assert(automorphism[v_to_1] == v_to_1);
                        assert(automorphism[v_from_1] == v_from_1);

                        _automorphism[v_from_1] = v_to_1;
                        _automorphism[v_to_1] = v_from_1;
                        _automorphism_supp.push_back(v_from_1);
                        _automorphism_supp.push_back(v_to_1);

                        /*automorphism[from_1] = to_1;
                        automorphism[to_1] = from_1;
                        automorphism_supp.push_back(from_1);
                        automorphism_supp.push_back(to_1);
                        write_canonical_recovery_string_to_automorphism(to_1, from_1);*/
                        ++pos;
                        // child_to and child_from could have canonical strings when translated back
                        assert(childcount[pair_to] == childcount[pair_from]);
                        stack1.push_back(std::pair<int, int>(g->v[pair_to], g->v[pair_to] + childcount[pair_to]));
                        while (!stack1.empty()) {
                            std::pair<int, int> from_to = stack1.pop_back();
                            int from = from_to.first;
                            const int to = from_to.second;
                            for (int f = from; f < to; ++f) {
                                const int next = edge_scratch[f];
                                //const int next = g->e[f];
                                const int from_next = g->v[next];
                                const int to_next = g->v[next] + childcount[next];
                                ++from;
                                assert(next >= 0);
                                assert(next < g->v_size);
                                assert(map[pos] != next);

                                //const int to_2 = translate_back(next);
                                //const int from_2 = translate_back(map[pos]);
                                const int v_to_2 = next;
                                const int v_from_2 = map[pos];

                                assert(_automorphism[v_to_2] == v_to_2);
                                assert(_automorphism[v_from_2] == v_from_2);
                                _automorphism[v_from_2] = v_to_2;
                                _automorphism[v_to_2] = v_from_2;
                                _automorphism_supp.push_back(v_from_2);
                                _automorphism_supp.push_back(v_to_2);

                                /*assert(automorphism[to_2] == to_2);
                                assert(automorphism[from_2] == from_2);
                                automorphism[from_2] = to_2;
                                automorphism[to_2] = from_2;
                                automorphism_supp.push_back(from_2);
                                automorphism_supp.push_back(to_2);
                                write_canonical_recovery_string_to_automorphism(to_2, from_2);*/
                                ++pos;
                                if (from_next != to_next) // there was a semicolon here, should have been bug
                                    stack1.push_back(std::pair<int, int>(from_next, to_next));
                            }
                        }

                        assert(pos == map.cur_pos);

                        assert(g_old_v[pair_to] == 1);
                        assert(g_old_v[pair_from] == 1);
                        assert(del.get(pair_to));
                        assert(del.get(pair_from));
                        /*(*consume)(domain_size, automorphism.get_array(), automorphism_supp.cur_pos,
                                automorphism_supp.get_array());
                        reset_automorphism(automorphism.get_array(), automorphism_supp.cur_pos,
                                           automorphism_supp.get_array());
                        automorphism_supp.reset();*/

                        pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos, _automorphism_supp.get_array(),
                                 consume);

                        reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                           _automorphism_supp.get_array());
                        _automorphism_supp.reset();
                    }

                    for (int j = 0; j < parentlist.cur_pos; ++j) {
                        const int first_pair_parent = parentlist[j];
                        const int pair_from = first_pair_parent; // need to use both childlist and canonical recovery again
                        const int pair_to = pair_match[pair_from];

                        const int original_parent = translate_back(first_pair_parent);
                        const int original_pair_to = translate_back(pair_to);
                        recovery_strings[original_parent].push_back(original_pair_to);
                        for (size_t s = 0; s < recovery_strings[original_pair_to].size(); ++s)
                            recovery_strings[original_parent].push_back(
                                    recovery_strings[original_pair_to][s]);

                        // also write stack of original_pair_to to canonical recovery string of original_parent, since
                        // recovery_strings has not been updated with progress made in this routine
                        stack1.reset();
                        stack1.push_back(std::pair<int, int>(g->v[pair_to], g->v[pair_to] + childcount[pair_to]));
                        while (!stack1.empty()) {
                            std::pair<int, int> from_to = stack1.pop_back();
                            int from = from_to.first;
                            const int to = from_to.second;
                            for (int f = from; f < to; ++f) {
                                const int next = edge_scratch[f];
                                //const int next = g->e[f];
                                const int from_next = g->v[next];
                                const int to_next = g->v[next] + childcount[next];
                                const int orig_next = translate_back(next);
                                recovery_strings[original_parent].push_back(orig_next);
                                for (size_t s = 0; s < recovery_strings[orig_next].size(); ++s)
                                    recovery_strings[original_parent].push_back(
                                            recovery_strings[orig_next][s]);
                                if (from_next != to_next)
                                    stack1.push_back(std::pair<int, int>(from_next, to_next));
                            }
                        }
                    }

                    permute_parents_instead = true;
                }

                while (!parentlist.empty()) {
                    int parent, childcount_from, childcount_to, child_from;
                    if (!permute_parents_instead) {
                        parent = parentlist.pop_back();
                        childcount_from = childcount_prev[parent];
                        childcount_to = childcount[parent];
                    } else {
                        parent = -1;
                        childcount_from = 0;
                        childcount_to = parentlist.cur_pos;
                    }
                    // automorphism 1: long cycle (c1 ... cn)
                    assert(childcount_to - childcount_from > 0);
                    if (childcount_to - childcount_from == 1) {
                        if (permute_parents_instead)
                            break;
                        continue;
                    }
                    if (!permute_parents_instead) {
                        child_from = edge_scratch[g->v[parent] + childcount_from];
                        //child_from = g->e[g->v[parent] + childcount_from];
                    } else {
                        child_from = parentlist[0];
                    }

                    // descending shared_tree of child_from while writing map
                    stack1.reset();
                    map.reset(); // TODO: could be automorphism_supp, then reset automorphism_supp only half-way
                    map.push_back(child_from);
                    stack1.push_back(std::pair<int, int>(g->v[child_from], g->v[child_from] + childcount[child_from]));
                    while (!stack1.empty()) {
                        std::pair<int, int> from_to = stack1.pop_back();
                        int from = from_to.first;
                        const int to = from_to.second;
                        for (int f = from; f < to; ++f) {
                            const int next = edge_scratch[f];
                            //const int next = g->e[f];
                            const int from_next = g->v[next];
                            const int to_next = g->v[next] + childcount[next];
                            map.push_back(next);
                            assert(next != parent);
                            if (from_next != to_next)
                                stack1.push_back(std::pair<int, int>(from_next, to_next));
                        }
                    }
                    int j = 2;
                    for (int i = childcount_from + 1; i < childcount_to; ++i) {
                        multiply_to_group_size(j);
                        ++j;
                        int child_to;
                        if (!permute_parents_instead) {
                            child_to = edge_scratch[g->v[parent] + i];
                            //child_to = g->e[g->v[parent] + i];
                        } else {
                            child_to = parentlist[i];
                        }
                        assert(c.vertex_to_col[child_from] == c.vertex_to_col[child_to]);
                        assert(child_from != child_to);
                        int pos = 0;

                        _automorphism_supp.reset();

                        // descending shared_tree of child_to while writing automorphism
                        stack1.reset();
                        assert(map[pos] != child_to);

                        const int v_to_1 = child_to;
                        const int v_from_1 = map[pos];
                        assert(_automorphism[v_to_1] == v_to_1);
                        assert(_automorphism[v_from_1] == v_from_1);
                        _automorphism[v_from_1] = v_to_1;
                        _automorphism[v_to_1] = v_from_1;
                        _automorphism_supp.push_back(v_from_1);
                        _automorphism_supp.push_back(v_to_1);

                        /*const int to_1 = translate_back(child_to);
                        const int from_1 = translate_back(map[pos]);
                        assert(automorphism[to_1] == to_1);
                        assert(automorphism[from_1] == from_1);
                        automorphism[from_1] = to_1;
                        automorphism[to_1] = from_1;
                        automorphism_supp.push_back(from_1);
                        automorphism_supp.push_back(to_1);
                        write_canonical_recovery_string_to_automorphism(to_1, from_1);*/
                        ++pos;
                        // child_to and child_from could have canonical strings when translated back
                        assert(childcount[child_to] == childcount[child_from]);
                        stack1.push_back(std::pair<int, int>(g->v[child_to], g->v[child_to] + childcount[child_to]));
                        while (!stack1.empty()) {
                            std::pair<int, int> from_to = stack1.pop_back();
                            int from = from_to.first;
                            const int to = from_to.second;
                            for (int f = from; f < to; ++f) {
                                const int next = edge_scratch[f];
                                //const int next      = g->e[f];
                                const int from_next = g->v[next];
                                const int to_next = g->v[next] + childcount[next];
                                ++from;
                                assert(next >= 0);
                                assert(next < g->v_size);
                                assert(map[pos] != next);

                                const int v_to_2 = next;
                                const int v_from_2 = map[pos];
                                assert(_automorphism[v_to_2] == v_to_2);
                                assert(_automorphism[v_from_2] == v_from_2);
                                _automorphism[v_from_2] = v_to_2;
                                _automorphism[v_to_2] = v_from_2;
                                _automorphism_supp.push_back(v_from_2);
                                _automorphism_supp.push_back(v_to_2);

                                /*const int to_2 = translate_back(next);
                                const int from_2 = translate_back(map[pos]);
                                assert(automorphism[to_2] == to_2);
                                assert(automorphism[from_2] == from_2);
                                automorphism[from_2] = to_2;
                                automorphism[to_2] = from_2;
                                automorphism_supp.push_back(from_2);
                                automorphism_supp.push_back(to_2);
                                write_canonical_recovery_string_to_automorphism(to_2, from_2);*/
                                ++pos;
                                if (from_next != to_next) // there was a semicolon here, should have been bug
                                    stack1.push_back(std::pair<int, int>(from_next, to_next));
                            }
                        }

                        assert(pos == map.cur_pos);

                        assert(g_old_v[child_to] == 1);
                        assert(g_old_v[child_from] == 1);
                        assert(del.get(child_to));
                        assert(del.get(child_from));
                        /*(*consume)(domain_size, automorphism.get_array(), automorphism_supp.cur_pos,
                                automorphism_supp.get_array());
                        reset_automorphism(automorphism.get_array(), automorphism_supp.cur_pos,
                                           automorphism_supp.get_array());
                        automorphism_supp.reset();*/
                        pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos, _automorphism_supp.get_array(),
                                 consume);

                        reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                           _automorphism_supp.get_array());
                        _automorphism_supp.reset();
                    }
                    if (permute_parents_instead) {
                        break;      // :-)
                    }
                }
                parentlist.reset();
            }

            if (g->v_size > 1) {
                // search for parents that still remain in the graph, and rewrite childlist structure into canonical string
                for (int _i = 0; _i < g->v_size; ++_i) {
                    if (!del.get(_i) && childcount[_i] > 0 && c.ptn[c.vertex_to_col[_i]] > 0) {
                        // should it be childcount[_i] > 0 or childcount[_i] >= 0? was childcount[_i] >= 0 but that doesnt make sense?
                        stack1.reset();
                        stack1.push_back(std::pair<int, int>(g->v[_i], g->v[_i] + childcount[_i]));
                        const int orig_i = translate_back(_i);
                        recovery_strings[orig_i].reserve(
                                recovery_strings[orig_i].size() + childcount[_i]);
                        while (!stack1.empty()) {
                            std::pair<int, int> from_to = stack1.pop_back();
                            int from = from_to.first;
                            const int to = from_to.second;
                            if (from == to) {
                                continue;
                            } else {
                                const int next = edge_scratch[from];
                                //const int next = g->e[from];
                                const int from_next = g->v[next];
                                const int to_next = g->v[next] + childcount[next];
                                ++from;
                                const int orig_next = translate_back(next);
                                recovery_strings[orig_i].push_back(orig_next);
                                // write canonical recovery string of orig_next into orig_i, since it is now represented by
                                // orig_i
                                assert(next != _i);
                                assert(orig_next != orig_i);
                                for (size_t j = 0; j < recovery_strings[orig_next].size(); ++j)
                                    recovery_strings[orig_i].push_back(
                                            recovery_strings[orig_next][j]);
                                stack1.push_back(std::pair<int, int>(from, to));
                                stack1.push_back(std::pair<int, int>(from_next, to_next));
                            }
                        }
                    }
                }
            }


            while (!worklist_deg0.empty()) {
                const int v_child = worklist_deg0.pop_back();
                if (del.get(v_child))
                    continue;
                assert(g_old_v[v_child] == 0);

                is_parent.reset();
                const int v_child_col = c.vertex_to_col[v_child];
                const int child_col_sz = c.ptn[v_child_col] + 1;
                const int parent_from = c.lab[v_child_col];
                del.set(parent_from);

                if (child_col_sz == 1)
                    continue;
                int j = 2;
                for (int i = v_child_col + 1; i < v_child_col + child_col_sz; ++i) {
                    multiply_to_group_size(j);
                    ++j;
                    const int parent_to = c.lab[i];
                    assert(g_old_v[parent_to] == 0);
                    del.set(parent_to);

                    assert(parent_from != parent_to);

                    assert(_automorphism_supp.cur_pos == 0);

                    _automorphism[parent_to] = parent_from;
                    _automorphism[parent_from] = parent_to;
                    _automorphism_supp.push_back(parent_from);
                    _automorphism_supp.push_back(parent_to);

                    pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                             _automorphism_supp.get_array(), consume);

                    reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                       _automorphism_supp.get_array());
                    _automorphism_supp.reset();

                    /*const int orig_parent_from = translate_back(parent_from);
                    const int orig_parent_to = translate_back(parent_to);

                    assert(recovery_strings[orig_parent_to].size() ==
                           recovery_strings[orig_parent_from].size());

                    automorphism[orig_parent_to] = orig_parent_from;
                    automorphism[orig_parent_from] = orig_parent_to;
                    automorphism_supp.push_back(orig_parent_from);
                    automorphism_supp.push_back(orig_parent_to);
                    for (size_t k = 0; k < recovery_strings[orig_parent_to].size(); ++k) {
                        const int str_from = recovery_strings[orig_parent_from][k];
                        const int str_to = recovery_strings[orig_parent_to][k];
                        automorphism[str_to] = str_from;
                        automorphism[str_from] = str_to;
                        automorphism_supp.push_back(str_from);
                        automorphism_supp.push_back(str_to);
                    }

                    (*consume)(domain_size, automorphism.get_array(), automorphism_supp.cur_pos,
                            automorphism_supp.get_array());
                    reset_automorphism(automorphism.get_array(), automorphism_supp.cur_pos,
                                       automorphism_supp.get_array());
                    automorphism_supp.reset();*/
                }
            }
        }

        // perform edge flips according to quotient graph
        void red_quotient_edge_flip(dejavu::sgraph *g, int *colmap) { // TODO could still optimize further ...
            if (g->v_size <= 1)
                return;

            del_e.reset();

            worklist_deg0.reset();
            worklist_deg1.reset();

            dejavu::mark_set connected_col(g->v_size);
            dejavu::mark_set is_not_matched(g->v_size);

            // int v_has_matching_color = 0;

            coloring test_col;
            g->initialize_coloring(&test_col, colmap);

            // int cnt = 0;
            int edge_flip_pot = 0;
            int edge_cnt = 0;

            std::vector<int> test_vec;
            for (int y = 0; y < g->v_size; ++y)
                test_vec.push_back(0);

            for (int i = 0; i < g->v_size;) {
                connected_col.reset();
                is_not_matched.reset();

                int connected_cols = 0;
                int connected_col_eq_sz = 0;
                int v = test_col.lab[i];
                for (int f = g->v[v]; f < g->v[v] + g->d[v]; ++f) {
                    const int v_neigh = g->e[f];
                    if (!connected_col.get(test_col.vertex_to_col[v_neigh])) {
                        assert(test_col.vertex_to_col[v_neigh] >= 0);
                        assert(test_col.vertex_to_col[v_neigh] < g->v_size);
                        assert(test_vec[test_col.vertex_to_col[v_neigh]] == 0);
                        connected_col.set(test_col.vertex_to_col[v_neigh]);
                        connected_cols += 1;
                        if (test_col.ptn[test_col.vertex_to_col[v_neigh]] == test_col.ptn[i])
                            connected_col_eq_sz += 1;
                    } else {
                        assert(test_vec[test_col.vertex_to_col[v_neigh]] > 0);
                        is_not_matched.set(test_col.vertex_to_col[v_neigh]);
                    }
                    test_vec[test_col.vertex_to_col[v_neigh]] += 1;
                }

                for (int ii = 0; ii < test_col.ptn[i] + 1; ++ii) {
                    const int vx = test_col.lab[i + ii];
                    for (int f = g->v[vx]; f < g->v[vx] + g->d[vx]; ++f) {
                        const int v_neigh = g->e[f];

                        assert(test_vec[test_col.vertex_to_col[v_neigh]] >= 0);
                        assert(test_vec[test_col.vertex_to_col[v_neigh]] < g->v_size);
                        if (test_vec[test_col.vertex_to_col[v_neigh]] ==
                            test_col.ptn[test_col.vertex_to_col[v_neigh]] + 1) {
                            edge_cnt += 1;
                            del_e.set(f); // mark edge for deletion (reverse edge is handled later automatically)
                        }

                        if (ii == 0) {
                            if (test_col.ptn[test_col.vertex_to_col[v_neigh]] == test_col.ptn[i] &&
                                test_vec[test_col.vertex_to_col[v_neigh]] ==
                                test_col.ptn[test_col.vertex_to_col[v_neigh]]) {
                                edge_flip_pot += (test_vec[test_col.vertex_to_col[v_neigh]] * (test_col.ptn[i] + 1)) -
                                                 (test_col.ptn[i] + 1);
                            }
                        }
                    }
                }

                for (int f = g->v[v]; f < g->v[v] + g->d[v]; ++f) {
                    const int v_neigh = g->e[f];
                    test_vec[test_col.vertex_to_col[v_neigh]] = 0;
                }

                i += test_col.ptn[i] + 1;
            }
        }

        void copy_coloring_to_colmap(const coloring *c, int *colmap) {
            for (int i = 0; i < c->lab_sz; ++i) {
                colmap[i] = c->vertex_to_col[i];
            }
        }

        // deletes vertices marked in 'del'
        // assumes that g->v points to g->e in ascending order
        void perform_del(dejavu::sgraph *g, int *colmap) {
            // copy some stuff
            g_old_v.clear();
            translate_layer_fwd.clear();
            translate_layer_bwd.clear();

            translate_layer_bwd.reserve(backward_translation_layers[backward_translation_layers.size() - 1].size());
            translate_layer_fwd.reserve(g->v_size);
            for (size_t i = 0; i < backward_translation_layers[backward_translation_layers.size() - 1].size(); ++i)
                translate_layer_bwd.push_back(
                        backward_translation_layers[backward_translation_layers.size() - 1][i]); // TODO this is shit

            // create translation array from old graph to new graph vertices
            int cnt = 0;
            int new_vsize = 0;
            // int pos_now = 0;
            for (int i = 0; i < g->v_size; ++i) {
                if (!del.get(i)) {
                    translate_layer_fwd.push_back(cnt);
                    const int translate_back = translate_layer_bwd[translate_layer_fwd.size() - 1];
                    backward_translation_layers[backward_translation_layers.size() - 1][cnt] = translate_back;
                    ++cnt;
                    ++new_vsize;
                } else {
                    translate_layer_fwd.push_back(-1);
                }
            }

            if (new_vsize == 0 || new_vsize == 1) {
                g->v_size = 0;
                g->e_size = 0;
                return;
            }

            backward_translation_layers[backward_translation_layers.size() - 1].resize(cnt);

            g_old_v.reserve(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                g_old_v.push_back(g->v[i]);
            }

            // make graph smaller using the translation array
            int epos = 0;
            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];

                if (new_v >= 0) {
                    int new_d = 0;
                    assert(new_v < new_vsize);
                    g->v[new_v] = epos;
                    for (int j = g_old_v[old_v]; j < g_old_v[old_v] + g->d[old_v]; ++j) {
                        const int ve = g->e[j];                          // assumes ascending order!
                        const int new_ve = translate_layer_fwd[ve];
                        if (new_ve >= 0) {
                            assert(new_ve < new_vsize);
                            assert(new_ve >= 0);
                            ++new_d;
                            assert(j >= epos);
                            g->e[epos] = new_ve;                         // assumes ascending order!
                            ++epos;
                        }
                    }

                    g_old_v[old_v] = new_d;
                }
            }

            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];
                assert(old_v >= new_v);
                if (new_v >= 0) {
                    assert(old_v >= 0);
                    assert(new_v >= 0);
                    assert(old_v < g->v_size);
                    assert(new_v < new_vsize);
                    colmap[new_v] = colmap[old_v];
                }
            }

            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];
                if (new_v >= 0) {
                    g->d[new_v] = g_old_v[old_v];
                }
            }

            assert(new_vsize <= g->v_size);
            assert(epos <= g->e_size);

            g->e_size = epos;
            g->v_size = new_vsize;
            del.reset();
        }

        // deletes vertices marked in 'del'
        // assumes that g->v points to g->e in ascending order
        void perform_del_edge(dejavu::sgraph *g) {
            if (g->v_size <= 1)
                return;

            // int pre_esize = g->e_size;
            // copy some stuff
            g_old_v.clear();
            g_old_v.reserve(g->v_size);

            for (int i = 0; i < g->v_size; ++i) {
                g_old_v.push_back(g->v[i]);
            }

            // create translation array from old graph to new graph vertices
            // int cnt = 0;
            int new_vsize = g->v_size;

            // make graph smaller using the translation array
            int epos = 0;
            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = old_v;

                if (new_v >= 0) {
                    int new_d = 0;
                    g->v[new_v] = epos;
                    for (int j = g_old_v[old_v]; j < g_old_v[old_v] + g->d[old_v]; ++j) {
                        const int ve = g->e[j];
                        const int new_ve = ve;
                        if (!del_e.get(j)) {
                            assert(new_ve < new_vsize);
                            assert(new_ve >= 0);
                            ++new_d;
                            g->e[epos] = new_ve;
                            ++epos;
                        }
                    }

                    g->d[new_v] = new_d;
                }
            }

            g->e_size = epos;
            g->v_size = new_vsize;
        }

        // deletes vertices marked in 'del'
        // for vertices v where add_edge_buff_act[v] is set, in turn adds edges add_edge_buff_act[v]
        // assumes that g->v points to g->e in ascending order
        // assumes that degree of a vertex stays the same or gets smaller
        void perform_del_add_edge(dejavu::sgraph *g, int *colmap) {
            if (g->v_size <= 1)
                return;

            // copy some stuff
            g_old_v.clear();
            translate_layer_fwd.clear();
            translate_layer_bwd.clear();

            for (size_t i = 0; i < backward_translation_layers[backward_translation_layers.size() - 1].size(); ++i)
                translate_layer_bwd.push_back(backward_translation_layers[backward_translation_layers.size() - 1][i]);

            // create translation array from old graph to new graph vertices
            int cnt = 0;
            int new_vsize = 0;
            for (int i = 0; i < g->v_size; ++i) {
                worklist_deg0[i] = colmap[i];
                if (!del.get(i)) {
                    translate_layer_fwd.push_back(cnt);
                    const int translate_back = translate_layer_bwd[translate_layer_fwd.size() - 1];
                    backward_translation_layers[backward_translation_layers.size() - 1][cnt] = translate_back;
                    ++cnt;
                    ++new_vsize;
                } else {
                    //translation_layers[fwd_ind].push_back(-1);
                    translate_layer_fwd.push_back(-1);
                }
            }

            if (new_vsize == g->v_size)
                return;

            if (new_vsize == 0 || new_vsize == 1) {
                g->v_size = 0;
                g->e_size = 0;
                return;
            }

            g_old_v.reserve(g->v_size);

            for (int i = 0; i < g->v_size; ++i) {
                g_old_v.push_back(g->v[i]);
            }

            backward_translation_layers[backward_translation_layers.size() - 1].resize(cnt);

            // make graph smaller using the translation array
            int epos = 0;
            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[old_v];

                if (new_v >= 0) {
                    int new_d = 0;
                    g->v[new_v] = epos;
                    for (int j = g_old_v[old_v]; j < g_old_v[old_v] + g->d[old_v]; ++j) {
                        const int ve = g->e[j];
                        const int new_ve = translate_layer_fwd[ve];
                        if (new_ve >= 0) {
                            assert(new_ve < new_vsize);
                            assert(new_ve >= 0);
                            ++new_d;
                            g->e[epos] = new_ve;
                            ++epos;
                        }
                    }
                    if (add_edge_buff_act.get(old_v)) {
                        while (!add_edge_buff[old_v].empty()) {
                            const int edge_to_old = add_edge_buff[old_v].back();
                            add_edge_buff[old_v].pop_back();
                            //const int edge_to_old = add_edge_buff[old_v];
                            assert(add_edge_buff_act.get(edge_to_old));
                            //const int edge_to_new = translation_layers[fwd_ind][edge_to_old];
                            const int edge_to_new = translate_layer_fwd[edge_to_old];
                            assert(edge_to_old >= 0);
                            assert(edge_to_old <= domain_size);
                            assert(edge_to_new >= 0);
                            assert(edge_to_new <= new_vsize);
                            ++new_d;
                            g->e[epos] = edge_to_new;
                            ++epos;
                        }
                    }
                    g_old_v[old_v] = new_d;
                }
            }

            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];
                if (new_v >= 0) {
                    g->d[new_v] = g_old_v[old_v];
                }
            }

            g->e_size = epos;

            // adapt colmap for remaining vertices
            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                //const int new_v = translation_layers[fwd_ind][i];
                const int new_v = translate_layer_fwd[i];
                assert(new_v < new_vsize);
                if (new_v >= 0) {
                    assert(colmap[old_v] >= 0);
                    //assert(colmap[old_v] < domain_size);
                    //colmap[new_v] = colmap[old_v];
                    colmap[new_v] = worklist_deg0[old_v];
                }
            }

            g->v_size = new_vsize;

            for (int i = 0; i < g->v_size; ++i) {
                assert(g->d[i] > 0 ? g->v[i] < g->e_size : true);
                assert(g->d[i] > 0 ? g->v[i] >= 0 : true);
                assert(g->d[i] >= 0);
                assert(g->d[i] < g->v_size);
            }
            for (int i = 0; i < g->e_size; ++i) {
                assert(g->e[i] < g->v_size);
                assert(g->e[i] >= 0);
            }

            add_edge_buff_act.reset();
            del.reset();
        }

        // marks all discrete vertices in 'del'
        void mark_discrete_for_deletion(dejavu::sgraph *g, int *colmap) {
            //int discrete_cnt = 0;
            worklist_deg0.reset();
            for (int i = 0; i < domain_size; ++i) {
                worklist_deg0.push_back(0);
            }
            for (int i = 0; i < g->v_size; ++i) {
                assert(colmap[i] < domain_size);
                worklist_deg0[colmap[i]]++;
            }
            for (int i = 0; i < g->v_size; ++i) {
                if (worklist_deg0[colmap[i]] == 1)
                    del.set(i);
            }
            worklist_deg0.reset();
        }

        // deletes all discrete vertices
        void perform_del_discrete(dejavu::sgraph *g, int *colmap) {
            if (g->v_size <= 1)
                return;

            int discrete_cnt = 0;
            worklist_deg0.reset();
            for (int i = 0; i < domain_size; ++i) { // TODO: this is shit
                worklist_deg0.push_back(0);
            }
            for (int i = 0; i < g->v_size; ++i) {
                assert(colmap[i] < domain_size);
                worklist_deg0[colmap[i]]++;
            }
            for (int i = 0; i < g->v_size; ++i) {
                discrete_cnt += (worklist_deg0[colmap[i]] == 1);
            }
            if (discrete_cnt == g->v_size) {
                g->v_size = 0;
                g->e_size = 0;
                return;
            }
            if (discrete_cnt == 0) {
                return;
            }

            // copy some stuff
            g_old_v.clear();
            translate_layer_fwd.clear();
            translate_layer_bwd.clear();

            for (size_t i = 0; i < backward_translation_layers[backward_translation_layers.size() - 1].size(); ++i)
                translate_layer_bwd.push_back(backward_translation_layers[backward_translation_layers.size() - 1][i]);

            g_old_v.reserve(g->v_size);

            for (int i = 0; i < g->v_size; ++i) {
                g_old_v.push_back(g->v[i]);
            }

            // create translation array from old graph to new graph vertices
            int cnt = 0;
            int new_vsize = 0;
            for (int i = 0; i < g->v_size; ++i) {
                if (worklist_deg0[colmap[i]] != 1) {
                    translate_layer_fwd.push_back(cnt);
                    const int translate_back = translate_layer_bwd[translate_layer_fwd.size() - 1];
                    backward_translation_layers[backward_translation_layers.size() - 1][cnt] = translate_back;
                    ++cnt;
                    ++new_vsize;
                } else {
                    translate_layer_fwd.push_back(-1);
                }
            }

            backward_translation_layers[backward_translation_layers.size() - 1].resize(cnt);

            // make graph smaller using the translation array
            int epos = 0;
            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];

                if (new_v >= 0) {
                    int new_d = 0;
                    assert(new_v < new_vsize);
                    g->v[new_v] = epos;
                    for (int j = g_old_v[old_v]; j < g_old_v[old_v] + g->d[old_v]; ++j) {
                        const int ve = g->e[j];                          // assumes ascending order!
                        const int new_ve = ve>=0?translate_layer_fwd[ve]:-1;
                        if (new_ve >= 0) {
                            assert(new_ve < new_vsize);
                            assert(new_ve >= 0);
                            ++new_d;
                            assert(j >= epos);
                            g->e[epos] = new_ve;                         // assumes ascending order!
                            ++epos;
                        }
                    }

                    g_old_v[old_v] = new_d;
                }
            }

            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];
                assert(old_v >= new_v);
                if (new_v >= 0) {
                    assert(old_v >= 0);
                    assert(new_v >= 0);
                    assert(old_v < g->v_size);
                    assert(new_v < new_vsize);
                    colmap[new_v] = colmap[old_v];
                }
            }

            for (int i = 0; i < g->v_size; ++i) {
                const int old_v = i;
                const int new_v = translate_layer_fwd[i];
                if (new_v >= 0) {
                    g->d[new_v] = g_old_v[old_v];
                }
            }

            assert(new_vsize <= g->v_size);
            assert(epos <= g->e_size);

            g->e_size = epos;
            g->v_size = new_vsize;
            del.reset();
        }

        // select a color class in a quotient component
        // assumes internal state of quotient component is up-to-date
        std::pair<int, int>
        select_color_component(coloring *c1, int component_start_pos, int component_end_pos, int hint) {
            int cell = -1;
            bool only_discrete_prev = true;
            int _i = component_start_pos;
            if (hint >= 0)
                _i = hint;
            for (; _i < component_end_pos; ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }

                for (int i = col; i < col + col_sz + 1; ++i) {
                    if (c1->vertex_to_col[c1->lab[i]] != i)
                        continue; // not a color
                    if (c1->ptn[i] > 0 && only_discrete_prev) {
                        //start_search_here = i;
                        only_discrete_prev = false;
                    }
                    if (c1->ptn[i] >= 1) {
                        cell = i;
                        break;
                    }
                }
                if (cell != -1)
                    break;
            }
            return {cell, _i};
        }

        // select a color class in a quotient component
        // assumes internal state of quotient component is up-to-date
        std::pair<int, int>
        select_color_component_large(coloring *c1, int component_start_pos, int component_end_pos, int hint) {
            int cell = -1;
            bool only_discrete_prev = true;
            int _i = component_start_pos;
            if (hint >= 0)
                _i = hint;
            int continue_buffer = 2;
            for (; _i < component_end_pos; ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }

                for (int i = col; i < col + col_sz + 1; ++i) {
                    int largest_cell_sz = INT32_MIN;
                    if (c1->vertex_to_col[c1->lab[i]] != i)
                        continue; // not a color
                    if (c1->ptn[i] > 0 && only_discrete_prev) {
                        hint = _i;
                        only_discrete_prev = false;
                    }
                    if (c1->ptn[i] >= 1 && c1->ptn[i] > largest_cell_sz) {
                        cell = i;
                        largest_cell_sz = c1->ptn[i];
                    }
                }
                if (cell != -1) {
                    --continue_buffer;
                    if (continue_buffer >= 0)
                        break;
                }
            }
            return {cell, _i};
        }

        // select a color class in a quotient component
        // assumes internal state of quotient component is up-to-date
        std::pair<int, int> select_color_component_large_touched(coloring *c1, int component_start_pos,
                                                                 int component_end_pos, int hint,
                                                                 dejavu::mark_set *touched_set) {
            int cell = -1;
            bool only_discrete_prev = true;
            int _i = component_start_pos;
            if (hint >= 0) {
                _i = hint;
            } else {
                /*int largest_cell = -1;
            int lookahead = 12;
            for(int j = 0; j < touched_list->cur_pos; ++j) {
                const int cand_col = (*touched_list)[j];
                if(c1->ptn[cand_col] > 0 && c1->ptn[cand_col] > largest_cell) {
                    cell = cand_col;
                    largest_cell = c1->ptn[cand_col];
                }
                if(cell != -1) {
                    --lookahead;
                    if(lookahead <= 0)
                        break;
                }
            }
            if(cell != -1)
                return {cell, -1};*/
            }
            int continue_buffer = 4;
            for (; _i < component_end_pos; ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }

                for (int i = col; i < col + col_sz + 1;) {
                    int largest_cell_sz = INT32_MIN;
                    /*if(c1->vertex_to_col[c1->lab[i]] != i)
                    continue; // not a color*/
                    if (c1->ptn[i] > 0 && only_discrete_prev) {
                        hint = _i;
                        only_discrete_prev = false;
                    }
                    if (c1->ptn[i] >= 1 && (c1->ptn[i] > largest_cell_sz ||
                                            (touched_set->get(i) && (cell == -1 || !touched_set->get(cell))))) {
                        cell = i;
                        largest_cell_sz = c1->ptn[i];
                    }
                    i += c1->ptn[i] + 1;
                }
                if (cell != -1) {
                    --continue_buffer;
                    if (continue_buffer <= 0)
                        break;
                }
            }
            return {cell, hint};
        }


        unsigned long* invariant_acc = nullptr;
        dejavu::mark_set*  internal_touched_color;
        dejavu::work_list* internal_touched_color_list;
        std::function<dejavu::ir::type_split_color_hook> my_split_hook;

        bool split_hook(const int old_color, const int new_color, const int) {
            // record colors that were changed
            if (!internal_touched_color->get(new_color)) {
                internal_touched_color->set(new_color);
                internal_touched_color_list->push_back(new_color);
            }

            if (!internal_touched_color->get(old_color)) {
                internal_touched_color->set(old_color);
                internal_touched_color_list->push_back(old_color);
            }

            *invariant_acc = add_to_hash(*invariant_acc, new_color);

            return true;
        }

        std::function<dejavu::ir::type_split_color_hook> self_split_hook() {
            return [this](auto && PH1, auto && PH2, auto && PH3) { return
                    split_hook(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                               std::forward<decltype(PH3)>(PH3)); };
        }

        void refine_coloring(dejavu::sgraph *g, coloring *c, unsigned long* invariant,
                             int init_color_class, int cell_early, dejavu::mark_set *set_touched_color,
                             dejavu::work_list *set_touched_color_list) {
            invariant_acc = invariant;
            internal_touched_color      = set_touched_color;
            internal_touched_color_list = set_touched_color_list;

            R1->refine_coloring(g, c, init_color_class, cell_early, my_split_hook);
        }

        // TODO: flat version that stays on first level of IR shared_tree? or deep version that goes as deep as possible while preserving an initial color?
        // performs sparse probing for all color classes, but only for 1 individualization
        void sparse_ir_probe(dejavu::sgraph *g, int *colmap, dejavu_hook* consume, selector_type sel_type) {
            if (g->v_size <= 1 || CONFIG_PREP_DEACT_PROBE)
                return;

            std::vector<int> individualize_later;

            coloring c1;
            g->initialize_coloring(&c1, colmap); // could re-order to reduce overhead when not applicable
            for (int i = 0; i < g->v_size; ++i)
                colmap[i] = c1.vertex_to_col[i];

            coloring c2;
            c2.copy(&c1);
            c2.copy_ptn(&c1);

            for (int i = 0; i < g->v_size; ++i) {
                assert(c2.vertex_to_col[i] == c1.vertex_to_col[i]);
                assert(c2.vertex_to_lab[i] == c1.vertex_to_lab[i]);
                assert(c2.lab[i] == c1.lab[i]);
                assert(c2.ptn[i] == c1.ptn[i]);
            }

            coloring original_c;
            original_c.copy(&c1);
            original_c.copy_ptn(&c1);

            coloring color_cache;
            color_cache.copy(&c1);
            color_cache.copy_ptn(&c1);

            selector S;

            //dejavu::mark_set  touched_color(g->v_size);
            //dejavu::work_list touched_color_list(g->v_size);
            touched_color.reset();
            touched_color_list.reset();

            unsigned long I1 = 0;
            unsigned long I2 = 0;

            bool certify = true;
            S.empty_cache();

            while (true) {
                // select a color class
                const int cell = S.select_color_dynamic(g, &c1, sel_type);
                if (cell == -1)
                    break;

                const int cell_sz = c1.ptn[cell];
                bool is_orig_color    = original_c.vertex_to_col[original_c.lab[cell]] == cell;
                bool is_orig_color_sz = original_c.ptn[cell] == cell_sz;

                orbit.reset();
                touched_color.reset();
                touched_color_list.reset();

                for (int i = 0; i < g->v_size; ++i) {
                    assert(c2.vertex_to_col[i] == c1.vertex_to_col[i]);
                    assert(c2.vertex_to_lab[i] == c1.vertex_to_lab[i]);
                    assert(c2.lab[i] == c1.lab[i]);
                    assert(c2.ptn[i] == c1.ptn[i]);
                }
                assert(c1.cells == c2.cells);

                const int ind_v1 = c1.lab[cell];
                assert(c1.vertex_to_col[ind_v1] == cell);
                assert(c1.ptn[cell] > 0);
                const unsigned long acc_prev = I1;
                I2 = acc_prev;
                I1 = acc_prev;
                const int init_c1 = dejavu::ir::refinement::individualize_vertex(&c1, ind_v1);

                bool all_certified = true;

                touched_color.set(cell);
                touched_color.set(init_c1);
                touched_color_list.push_back(cell);
                touched_color_list.push_back(init_c1);

                //R1->refine_coloring(g, &c1, &I1, init_c1, -1, &touched_color, &touched_color_list);
                refine_coloring(g, &c1, &I1, init_c1, -1, &touched_color,
                                &touched_color_list);

                // color cache to reset c2 back "before" individualization
                for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                    const int _c = touched_color_list[j];
                    int f = 0;
                    while (f < c1.ptn[_c] + 1) {
                        const int i = _c + f;
                        ++f;
                        color_cache.ptn[i] = c2.ptn[i];
                        color_cache.lab[i] = c2.lab[i];
                        color_cache.vertex_to_col[color_cache.lab[i]] = c2.vertex_to_col[c2.lab[i]];
                        color_cache.vertex_to_lab[color_cache.lab[i]] = c2.vertex_to_lab[c2.lab[i]];
                    }
                }
                color_cache.cells = c2.cells;

                if (!is_orig_color || !is_orig_color_sz) {// no point in looking for automorphisms
                    all_certified = false;
                }

                bool hard_reset = false;

                // repeat for all in color class
                for (int it_c = 1; (it_c < cell_sz + 1) && all_certified; ++it_c) {
                    const int ind_v2 = c2.lab[cell + it_c];
                    if (orbit.are_in_same_orbit(ind_v1, ind_v2)) {
                        continue;
                    }
                    //assert(ind_v1 == ind_v2);
                    assert(c2.lab[cell + it_c] == color_cache.lab[cell + it_c]);
                    assert(c2.vertex_to_col[ind_v2] == cell);
                    assert(c2.ptn[cell] > 0);
                    assert(c2.ptn[cell] == cell_sz);
                    I2 = acc_prev;
                    const int init_c2 = dejavu::ir::refinement::individualize_vertex(&c2, ind_v2);
                    assert(init_c1 == init_c2);
                    //R1->refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color, &touched_color_list);
                    refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color,
                                    &touched_color_list);

                    if (I1 != I2) {
                        if (cell_sz == 1) individualize_later.push_back(ind_v1);
                        all_certified = false;
                        hard_reset = true;
                        break;
                    }

                    _automorphism_supp.reset();
                    if (c1.cells != g->v_size) { // touched_colors doesn't work properly when early-out is used
                        // read automorphism
                        for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                            const int _c = touched_color_list[j];
                            int f = 0;
                            while (f < c1.ptn[_c] + 1) {
                                const int i = _c + f;
                                ++f;
                                if (c1.lab[i] != c2.lab[i]) {
                                    _automorphism[c1.lab[i]] = c2.lab[i];
                                    _automorphism_supp.push_back(c1.lab[i]);
                                }
                            }
                        }
                    } else {
                        for (int i = 0; i < g->v_size; ++i) {
                            if (c1.lab[i] != c2.lab[i]) {
                                _automorphism[c1.lab[i]] = c2.lab[i];
                                _automorphism_supp.push_back(c1.lab[i]);
                            }
                        }
                    }

                    certify = R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                             _automorphism_supp.cur_pos,
                                                             _automorphism_supp.get_array());
                    assert(certify ? R1->certify_automorphism(g, _automorphism.get_array()) : true);
                    all_certified = certify && all_certified;
                    if (certify) {
                        pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                 _automorphism_supp.get_array(),
                                 consume);
                        add_automorphism_to_orbit(&orbit, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                                  _automorphism_supp.get_array());
                        // reset c2 to color_cache
                        c2.cells = color_cache.cells;
                        if (c2.cells != g->v_size) {
                            for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                const int _c = touched_color_list[j];
                                int f = 0;
                                while (f < c1.ptn[_c] + 1) {
                                    const int i = _c + f;
                                    ++f;
                                    c2.ptn[i] = color_cache.ptn[i];
                                    c2.lab[i] = color_cache.lab[i];
                                    c2.vertex_to_col[c2.lab[i]] = color_cache.vertex_to_col[color_cache.lab[i]];
                                    c2.vertex_to_lab[c2.lab[i]] = color_cache.vertex_to_lab[color_cache.lab[i]];
                                }
                            }
                        } else {
                            // can't happen?
                        }
                    } else {
                        hard_reset = true;
                    }

                    reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                       _automorphism_supp.get_array());
                    _automorphism_supp.reset();
                }

                // reset c2 to c1
                const int pre_c2_cells = c2.cells;
                c2.cells = c1.cells;
                if (c1.cells != g->v_size && pre_c2_cells != g->v_size && !hard_reset) {
                    for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                        const int _c = touched_color_list[j];
                        int f = 0;
                        while (f < c1.ptn[_c] + 1) {
                            const int i = _c + f;
                            ++f;
                            c2.ptn[i] = c1.ptn[i];
                            c2.lab[i] = c1.lab[i];
                            c2.vertex_to_col[c2.lab[i]] = c1.vertex_to_col[c1.lab[i]];
                            c2.vertex_to_lab[c2.lab[i]] = c1.vertex_to_lab[c1.lab[i]];
                        }
                    }

                    for (int i = 0; i < g->v_size; ++i) {
                        assert(c2.vertex_to_col[i] == c1.vertex_to_col[i]);
                        assert(c2.vertex_to_lab[i] == c1.vertex_to_lab[i]);
                        assert(c2.lab[i] == c1.lab[i]);
                        assert(c2.ptn[i] == c1.ptn[i]);
                    }
                } else {
                    c2.copy_force(&c1);
                    c2.copy_ptn(&c1);

                    for (int i = 0; i < g->v_size; ++i) {
                        assert(c2.vertex_to_col[i] == c1.vertex_to_col[i]);
                        assert(c2.vertex_to_lab[i] == c1.vertex_to_lab[i]);
                        assert(c2.lab[i] == c1.lab[i]);
                        assert(c2.ptn[i] == c1.ptn[i]);
                    }
                }

                if (all_certified) {
                    assert(cell_sz == original_c.ptn[cell]);
                    multiply_to_group_size(cell_sz + 1);
                    individualize_later.push_back(ind_v1);
                }
            }

            if (!individualize_later.empty()) {
                //PRINT("(prep-red) sparse-ir-flat completed: " << individualize_later.size());
                for (size_t i = 0; i < individualize_later.size(); ++i) {
                    const int ind_vert = individualize_later[i];
                    const int init_c = R1->individualize_vertex(&original_c, ind_vert);
                    //R1->refine_coloring(g, &original_c, &I1, init_c, -1, nullptr, nullptr);
                    R1->refine_coloring(g, &original_c, init_c);
                }
                for (int i = 0; i < g->v_size; ++i) {
                    colmap[i] = original_c.vertex_to_col[i];
                }
            }
        }

        // given automorphism of reduced graph, reconstructs automorphism of the original graph
        // does not optimize for consecutive calls
        void pre_hook(int, const int *_automorphism, int _supp, const int *_automorphism_supp, dejavu_hook* hook) {
            if(hook == nullptr)
                return;

            automorphism_supp.reset();

            bool use_aux_auto = false;

            for (int i = 0; i < _supp; ++i) {
                const int v_from = _automorphism_supp[i];
                const int orig_v_from = translate_back(v_from);
                const int v_to = _automorphism[v_from];
                assert(v_from != v_to);
                const int orig_v_to = translate_back(v_to);
                assert(v_from >= 0);
                assert(v_to >= 0);
                assert(orig_v_from < domain_size);
                assert(orig_v_from >= 0);
                assert(orig_v_to < domain_size);
                assert(orig_v_to >= 0);
                assert(automorphism[orig_v_from] == orig_v_from);
                automorphism[orig_v_from] = orig_v_to;
                automorphism_supp.push_back(orig_v_from);

                assert(recovery_strings[orig_v_to].size() == recovery_strings[orig_v_from].size());

                for (size_t j = 0; j < recovery_strings[orig_v_to].size(); ++j) {
                    const int v_from_t = recovery_strings[orig_v_from][j];
                    const int v_to_t   = recovery_strings[orig_v_to][j];
                    assert(v_from_t >  0?v_to_t >= 0:true);
                    assert(v_to_t   >  0?v_from_t >= 0:true);
                    assert(v_from_t <  0?v_to_t <= 0:true);
                    assert(v_to_t   <  0?v_from_t <= 0:true);
                    if(v_from_t >= 0 && v_to_t >= 0) {
                        assert(automorphism[v_from_t] == v_from_t);
                        automorphism[v_from_t] = v_to_t;
                        automorphism_supp.push_back(v_from_t);
                    } else {
                        const int abs_v_from_t = abs(v_from_t);
                        const int abs_v_to_t   = abs(v_to_t);

                        assert(aux_automorphism[abs_v_from_t] == abs_v_from_t);
                        aux_automorphism[abs_v_from_t] = abs_v_to_t;
                        aux_automorphism_supp.push_back(abs_v_from_t);

                        use_aux_auto = true;
                    }
                }
            }

            if(use_aux_auto) {
                for(int i = 0; i < aux_automorphism_supp.cur_pos; ++i) {
                    const int v_from = aux_automorphism_supp[i];
                    before_move[v_from]  = automorphism[aux_automorphism[v_from]];
                }
                for(int i = 0; i < aux_automorphism_supp.cur_pos; ++i) {
                    const int v_from = aux_automorphism_supp[i];
                    if(automorphism[v_from] == v_from) {
                        automorphism_supp.push_back(v_from);
                    }
                    assert(automorphism[v_from] != before_move[v_from]);
                    automorphism[v_from] = before_move[v_from];
                }
                reset_automorphism(aux_automorphism.get_array(), aux_automorphism_supp.cur_pos, aux_automorphism_supp.get_array());
                aux_automorphism_supp.reset();
            }


            if(hook != nullptr)
                (*hook)(domain_size, automorphism.get_array(), automorphism_supp.cur_pos, automorphism_supp.get_array());
            reset_automorphism(automorphism.get_array(), automorphism_supp.cur_pos, automorphism_supp.get_array());
            automorphism_supp.reset();
        }

    public:
        // given automorphism of reduced graph, reconstructs automorphism of the original graph
        void
        pre_hook_buffered(int _n, const int *_automorphism, int _supp, const int *_automorphism_supp, dejavu_hook* hook) {
            if(hook == nullptr) {
                return;
            }

            meld_translation_layers();
            automorphism_supp.reset();

            bool use_aux_auto = false;

            if(_supp >= 0) {
                for (int i = 0; i < _supp; ++i) {
                    const int v_from = _automorphism_supp[i];
                    assert(v_from >= 0 && v_from < domain_size);
                    const int orig_v_from = backward_translation[v_from];
                    const int v_to = _automorphism[v_from];
                    assert(v_from != v_to);
                    const int orig_v_to = backward_translation[v_to];
                    assert((unsigned int)v_from < backward_translation.size());
                    assert(v_from >= 0);
                    assert((unsigned int)v_to < backward_translation.size());
                    assert(v_to >= 0);
                    assert(orig_v_from < domain_size);
                    assert(orig_v_from >= 0);
                    assert(orig_v_to < domain_size);
                    assert(orig_v_to >= 0);
                    assert(automorphism[orig_v_from] == orig_v_from);
                    automorphism[orig_v_from] = orig_v_to;
                    automorphism_supp.push_back(orig_v_from);

                    assert(orig_v_to   < (int)recovery_strings.size());
                    assert(orig_v_from < (int)recovery_strings.size());
                    assert(recovery_strings[orig_v_to].size() ==
                           recovery_strings[orig_v_from].size());

                    for (size_t j = 0; j < recovery_strings[orig_v_to].size(); ++j) {
                        /*const int v_from_t = recovery_strings[orig_v_from][j];
                        const int v_to_t = recovery_strings[orig_v_to][j];
                        assert(automorphism[v_from_t] == v_from_t);
                        automorphism[v_from_t] = v_to_t;
                        automorphism_supp.push_back(v_from_t);*/
                        const int v_from_t = recovery_strings[orig_v_from][j];
                        const int v_to_t   = recovery_strings[orig_v_to][j];
                        assert(v_from_t >  0?v_to_t >= 0:true);
                        assert(v_to_t   >  0?v_from_t >= 0:true);
                        assert(v_from_t <  0?v_to_t <= 0:true);
                        assert(v_to_t   <  0?v_from_t <= 0:true);
                        if(v_from_t >= 0 && v_to_t >= 0) {
                            assert(automorphism[v_from_t] == v_from_t);
                            automorphism[v_from_t] = v_to_t;
                            automorphism_supp.push_back(v_from_t);
                        } else {
                            const int abs_v_from_t = abs(v_from_t);
                            const int abs_v_to_t   = abs(v_to_t);
                            //if(automorphism[abs_v_from_t] == abs_v_from_t)
                            //    automorphism_supp.push_back(abs_v_from_t);

                            aux_automorphism[abs_v_from_t] = abs_v_to_t;
                            aux_automorphism_supp.push_back(abs_v_from_t);

                            use_aux_auto = true;
                        }
                    }
                }
            } else {
                for (int i = 0; i < _n; ++i) {
                    const int v_from = i;
                    const int orig_v_from = backward_translation[v_from];
                    const int v_to = _automorphism[v_from];
                    if(v_from == v_to)
                        continue;
                    const int orig_v_to = backward_translation[v_to];
                    assert((unsigned int)v_from < backward_translation.size());
                    assert(v_from >= 0);
                    assert((unsigned int)v_to < backward_translation.size());
                    assert(v_to >= 0);
                    assert(orig_v_from < domain_size);
                    assert(orig_v_from >= 0);
                    assert(orig_v_to < domain_size);
                    assert(orig_v_to >= 0);
                    assert(automorphism[orig_v_from] == orig_v_from);
                    automorphism[orig_v_from] = orig_v_to;
                    automorphism_supp.push_back(orig_v_from);

                    assert(recovery_strings[orig_v_to].size() ==
                           recovery_strings[orig_v_from].size());

                    for (size_t j = 0; j < recovery_strings[orig_v_to].size(); ++j) {
                        const int v_from_t = recovery_strings[orig_v_from][j];
                        const int v_to_t   = recovery_strings[orig_v_to][j];
                        assert(v_from_t >  0?v_to_t >= 0:true);
                        assert(v_to_t   >  0?v_from_t >= 0:true);
                        assert(v_from_t <  0?v_to_t <= 0:true);
                        assert(v_to_t   <  0?v_from_t <= 0:true);
                        if(v_from_t >= 0 && v_to_t >= 0) {
                            assert(automorphism[v_from_t] == v_from_t);
                            automorphism[v_from_t] = v_to_t;
                            automorphism_supp.push_back(v_from_t);
                        } else {
                            const int abs_v_from_t = abs(v_from_t);
                            const int abs_v_to_t   = abs(v_to_t);
                            //if(automorphism[abs_v_from_t] == abs_v_from_t)
                            //    automorphism_supp.push_back(abs_v_from_t);

                            aux_automorphism[abs_v_from_t] = abs_v_to_t;
                            aux_automorphism_supp.push_back(abs_v_from_t);

                            use_aux_auto = true;
                        }
                    }
                }
            }

            if(use_aux_auto) {
                for(int i = 0; i < aux_automorphism_supp.cur_pos; ++i) {
                    const int v_from = aux_automorphism_supp[i];
                    before_move[v_from]  = automorphism[aux_automorphism[v_from]];
                }
                for(int i = 0; i < aux_automorphism_supp.cur_pos; ++i) {
                    const int v_from = aux_automorphism_supp[i];
                    if(automorphism[v_from] == v_from)
                        automorphism_supp.push_back(v_from);
                    automorphism[v_from] = before_move[v_from];
                }
                reset_automorphism(aux_automorphism.get_array(), aux_automorphism_supp.cur_pos, aux_automorphism_supp.get_array());
                aux_automorphism_supp.reset();
            }

            if(hook != nullptr) {
                (*hook)(domain_size, automorphism.get_array(), automorphism_supp.cur_pos, automorphism_supp.get_array());
            }
            reset_automorphism(automorphism.get_array(), automorphism_supp.cur_pos, automorphism_supp.get_array());
            automorphism_supp.reset();
        }

    private:
        // compute or update quotient components
        void compute_quotient_graph_components_update(dejavu::sgraph *g, coloring *c1) {
            if (!init_quotient_arrays) {
                seen_vertex.initialize(g->v_size);
                seen_color.initialize(g->v_size);

                worklist.reserve(g->v_size);

                quotient_component_worklist_v.reserve(g->v_size + 32);
                quotient_component_worklist_col.reserve(g->v_size + 32);
                quotient_component_worklist_col_sz.reserve(g->v_size + 32);
                init_quotient_arrays = true;

                quotient_component_worklist_boundary.reserve(64);
                quotient_component_worklist_boundary_swap.reserve(64);

                quotient_component_touched.reserve(64);
                quotient_component_touched_swap.reserve(64);
            }

            seen_vertex.reset();
            seen_color.reset();

            int touched_vertices = 0;
            int finished_vertices = 0;

            quotient_component_workspace.clear();
            quotient_component_worklist_col.clear();
            quotient_component_worklist_col_sz.clear();
            quotient_component_worklist_boundary_swap.swap(quotient_component_worklist_boundary);
            quotient_component_worklist_boundary.clear();
            quotient_component_touched_swap.clear();
            quotient_component_touched_swap.swap(quotient_component_touched);

            if (quotient_component_touched_swap.empty()) { // || (quotient_component_touched_swap.size() == quotient_component_worklist_boundary_swap.size())
                quotient_component_worklist_v.clear();
                for (int vs = 0; vs < g->v_size; ++vs) {
                    if (c1->ptn[c1->vertex_to_col[vs]] == 0) {// ignore discrete vertices
                        ++finished_vertices;
                        seen_vertex.set(vs);
                    }
                }

                int component = 0;
                int current_component_sz = 0;
                for (int vs = 0; vs < g->v_size; ++vs) {
                    /*if (c1->ptn[c1->vertex_to_col[vs]] == 0) {// ignore discrete vertices
                    seen_vertex.set(vs);
                    continue;
                }*/
                    if (finished_vertices == g->v_size) break;
                    if (seen_vertex.get(vs))
                        continue;
                    worklist.push_back(vs);

                    while (!worklist.empty()) {
                        const int next_v = worklist.back();
                        worklist.pop_back();
                        if (seen_vertex.get(next_v))
                            continue;
                        /*if (c1->ptn[c1->vertex_to_col[next_v]] == 0) {// ignore discrete vertices
                        seen_vertex.set(next_v);
                        continue;
                    }*/

                        ++touched_vertices;
                        ++finished_vertices;
                        current_component_sz += 1;
                        seen_vertex.set(next_v);
                        quotient_component_worklist_v.push_back(next_v);
                        for (int i = 0; i < g->d[next_v]; ++i) {
                            if (!seen_vertex.get(g->e[g->v[next_v] + i]))
                                worklist.push_back(g->e[g->v[next_v] + i]); // neighbours
                        }
                        const int col = c1->vertex_to_col[next_v];
                        assert(next_v < g->v_size);
                        assert(col < g->v_size);
                        if (!seen_color.get(col)) {
                            quotient_component_worklist_col.push_back(col);
                            quotient_component_worklist_col_sz.push_back(c1->ptn[col]);
                            seen_color.set(col);
                            for (int i = 0; i < c1->ptn[col] + 1; ++i) {
                                assert(col + i < g->v_size);
                                assert(c1->vertex_to_col[c1->lab[col + i]] == c1->vertex_to_col[next_v]);
                                if (c1->lab[col + i] != next_v) {
                                    if (!seen_vertex.get(c1->lab[col + i]))
                                        worklist.push_back(c1->lab[col + i]);
                                }
                            }
                        }
                    }
                    quotient_component_worklist_boundary.emplace_back(
                            std::pair<int, int>(quotient_component_worklist_col.size(),
                                                quotient_component_worklist_v.size()));
                    current_component_sz = 0;
                    ++component;
                }
            } else {
                // go component by component and only check old touched components
                size_t old_component = 0;
                size_t new_component = 0;
                size_t touched_comp_i = 0;
                quotient_component_worklist_col.clear();
                quotient_component_worklist_col_sz.clear();

                while (old_component < quotient_component_worklist_boundary_swap.size()) {
                    int next_touched_old_component = -1;
                    if (touched_comp_i < quotient_component_touched_swap.size()) {
                        next_touched_old_component = quotient_component_touched_swap[touched_comp_i];
                    }

                    if ((int)old_component == next_touched_old_component) {
                        ++touched_comp_i;

                        size_t component_vstart = 0;
                        //size_t component_cstart = 0;
                        if (old_component > 0) {
                            component_vstart = quotient_component_worklist_boundary_swap[old_component - 1].second;
                            //component_cstart = quotient_component_worklist_boundary_swap[old_component - 1].first;
                        }
                        const int component_vend = quotient_component_worklist_boundary_swap[old_component].second;
                        //const int component_cend = quotient_component_worklist_boundary_swap[old_component].first;

                        int v_write_pos = component_vstart;
                        quotient_component_workspace.clear();
                        int discrete_vertices = 0;
                        for (int vi = component_vstart; vi < component_vend; ++vi) {
                            const int vs = quotient_component_worklist_v[vi]; // need to copy this
                            if (c1->ptn[c1->vertex_to_col[vs]] ==
                                0) { // set up dummy component for all discrete vertices
                                assert(c1->vertex_to_col[vs] == c1->vertex_to_lab[vs]);
                                seen_vertex.set(vs);
                                ++v_write_pos;
                                ++discrete_vertices;
                            } else {
                                quotient_component_workspace.push_back(vs);
                            }
                        }

                        if (discrete_vertices > 0) {
                            quotient_component_worklist_boundary.emplace_back( // discrete vertex component
                                    std::pair<int, int>(quotient_component_worklist_col.size(),
                                                        v_write_pos));
                            ++new_component;
                        }

                        size_t current_component_sz = 0;
                        for (size_t vi = 0; vi < quotient_component_workspace.size(); ++vi) {
                            if (current_component_sz == quotient_component_workspace.size())
                                break;
                            const int vs = quotient_component_workspace[vi];
                            if (seen_vertex.get(vs))
                                continue;
                            worklist.push_back(vs);

                            while (!worklist.empty()) {
                                const int next_v = worklist.back();
                                worklist.pop_back();
                                if (seen_vertex.get(next_v))
                                    continue;
                                if (c1->ptn[c1->vertex_to_col[next_v]] == 0) {// ignore discrete vertices
                                    assert(c1->vertex_to_col[next_v] == c1->vertex_to_lab[next_v]);
                                    seen_vertex.set(next_v);
                                    continue;
                                }

                                ++touched_vertices;
                                current_component_sz += 1;
                                seen_vertex.set(next_v);
                                quotient_component_worklist_v[v_write_pos] = next_v;
                                ++v_write_pos;
                                for (int i = 0; i < g->d[next_v]; ++i) {
                                    if (!seen_vertex.get(g->e[g->v[next_v] + i]))
                                        worklist.push_back(g->e[g->v[next_v] + i]); // neighbours
                                }
                                const int col = c1->vertex_to_col[next_v];
                                assert(next_v < g->v_size);
                                assert(col < g->v_size);
                                if (!seen_color.get(col)) {
                                    quotient_component_worklist_col.push_back(col);
                                    quotient_component_worklist_col_sz.push_back(c1->ptn[col]);
                                    seen_color.set(col);
                                    for (int i = 0; i < c1->ptn[col] + 1; ++i) {
                                        assert(col + i < g->v_size);
                                        assert(c1->vertex_to_col[c1->lab[col + i]] == c1->vertex_to_col[next_v]);
                                        //if (c1->lab[col + i] != next_v) {
                                        if (!seen_vertex.get(c1->lab[col + i]))
                                            worklist.push_back(c1->lab[col + i]);
                                        //}
                                    }
                                }
                            }
                            quotient_component_touched.push_back(new_component);
                            quotient_component_worklist_boundary.emplace_back(
                                    std::pair<int, int>(quotient_component_worklist_col.size(),
                                                        v_write_pos));
                            current_component_sz = 0;
                            ++new_component;
                        }

                        assert(v_write_pos == quotient_component_worklist_boundary_swap[old_component].second);
                        ++old_component;
                    } else {
                        ++new_component;
                        quotient_component_worklist_boundary.emplace_back(
                                std::pair<int, int>(quotient_component_worklist_col.size(),
                                                    quotient_component_worklist_boundary_swap[old_component].second));
                        ++old_component; // ignore component, just push old boundaries
                    }
                }
            }
        }

        // initialize some data structures
        void assure_ir_quotient_init(dejavu::sgraph *g) {
            if (!ir_quotient_component_init) {
                touched_color.initialize(g->v_size);
                touched_color_list.allocate(g->v_size);
                touched_color_cache.initialize(g->v_size);
                touched_color_list_cache.allocate(g->v_size);
                orbit.initialize(g->v_size);
                ir_quotient_component_init = true;
            }
        }

        // perform sparse probing for color classes of size 2, num_paths number of times
        int sparse_ir_probe_sz2_quotient_components(dejavu::sgraph *g, int *colmap, dejavu_hook* consume, int num_paths) {
            if (g->v_size <= 1 || CONFIG_PREP_DEACT_PROBE)
                return 0;

            quotient_component_touched.clear();
            quotient_component_touched_swap.clear();

            save_colmap_v_to_col.clear();
            save_colmap_v_to_lab.clear();
            save_colmap_ptn.clear();

            save_colmap_v_to_col.reserve(g->v_size);
            save_colmap_v_to_lab.reserve(g->v_size);
            save_colmap_ptn.reserve(g->v_size);

            coloring c1;
            g->initialize_coloring(&c1, colmap);

            for (int i = 0; i < g->v_size; ++i)
                colmap[i] = c1.vertex_to_col[i];

            int global_automorphisms_found = 0;
            int save_cell_number = -1;

            assure_ir_quotient_init(g);

            coloring c2;
            c2.copy(&c1);

            coloring c3;
            c3.copy(&c1);

            int penalty = 0;

            int touched_support = g->v_size;
            for (int x = 0; x < num_paths - penalty; ++x) {
                if (x > 0 && quotient_component_touched.empty()) {
                    break;
                }

                compute_quotient_graph_components_update(g, &c1);
                touched_support = 0;

                int automorphisms_found = 0;

                quotient_component_touched_swap.clear();
                quotient_component_touched_swap.swap(quotient_component_touched);

                if (x == 0) {
                    for (size_t i = 0; i < quotient_component_worklist_boundary.size(); ++i)
                        quotient_component_touched_swap.push_back(i);
                }

                if (quotient_component_touched_swap.size() < 2) {
                    //break; // added this
                    ++penalty;
                }
                if (quotient_component_touched_swap.size() < 3)
                    ++penalty;

                touched_color.reset();
                touched_color_list.reset();

                unsigned long I1 = 0, I2 = 0;//, Ivec_wr, Ivec_rd;

                bool certify = true;
                size_t quotient_component_start_pos = 0;
                size_t quotient_component_start_pos_v = 0;

                int component = 0;
                size_t next_touched_component_i = 0;
                size_t next_touched_component = -1;
                if (next_touched_component_i < quotient_component_touched_swap.size()) {
                    next_touched_component = quotient_component_touched_swap[next_touched_component_i];
                } else {
                    component = quotient_component_worklist_boundary.size() - 1;
                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;
                }
                while (component < (int) next_touched_component) {
                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;
                    ++component;
                }

                assert(component < (int) quotient_component_worklist_boundary.size());

                int quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                int quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;

                assert(quotient_component_end_pos_v - 1 < (int) quotient_component_worklist_v.size());

                while (quotient_component_start_pos < quotient_component_worklist_col.size()) {
                    assert(quotient_component_start_pos_v < quotient_component_worklist_v.size());
                    int start_search_here = quotient_component_start_pos;

                    assert(quotient_component_worklist_col[quotient_component_start_pos] >= 0);
                    assert(quotient_component_worklist_v[quotient_component_start_pos_v] >= 0);
                    certify = true;
                    bool touched_current_component = false;

                    while (certify) {
                        // select a color class of size 2
                        bool only_discrete_prev = true;
                        int cell = -1;
                        for (int _i = start_search_here; _i < quotient_component_end_pos; ++_i) {
                            const int col = quotient_component_worklist_col[_i];
                            const int col_sz = quotient_component_worklist_col_sz[_i];

                            if (only_discrete_prev)
                                start_search_here = _i;

                            if (col == -1) {// reached end of component
                                break;
                            }
                            for (int i = col; i < col + col_sz + 1; ++i) {
                                if (c1.vertex_to_col[c1.lab[i]] != i)
                                    continue; // not a color
                                if (c1.ptn[i] > 0 && only_discrete_prev) {
                                    only_discrete_prev = false;
                                }
                                if (c1.ptn[i] == 1) {
                                    cell = i;
                                    break;
                                }
                            }

                            if (cell >= 0)
                                break;
                        }
                        if (cell == -1) {
                            break;
                        }

                        touched_color.reset();
                        touched_color_list.reset();

                        I1 = 0;
                        I2 = 0;

                        int ind_v1, ind_v2;

                        if (c1.ptn[cell] == c2.ptn[cell]) {
                            ind_v1 = c1.lab[cell];
                            ind_v2 = c1.lab[cell + 1];
                            const int init_c1 = R1->individualize_vertex(&c1, ind_v1);

                            touched_color.set(cell);
                            touched_color.set(init_c1);
                            touched_color_list.push_back(cell);
                            touched_color_list.push_back(init_c1);
                            //R1->refine_coloring(g, &c1, &I1, init_c1, -1, &touched_color, &touched_color_list);
                            refine_coloring(g, &c1, &I1, init_c1, -1, &touched_color, &touched_color_list);

                            const int init_c2 = R1->individualize_vertex(&c2, ind_v2);
                            //R1->refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color,&touched_color_list);
                            refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color,&touched_color_list);
                            I1 = 0;
                            I2 = 0;
                        } else {
                            I1 = 1;
                        }
                        if (I1 != I2) {
                            // could only use component
                            for (int i = 0; i < g->v_size; ++i) {
                                colmap[i] = c1.vertex_to_col[i];
                            }
                            I2 = I1;
                            c2.copy(&c1);
                            continue;
                        }

                        bool dont_bother_certify = false;

                        _automorphism_supp.reset();
                        if (c1.cells != g->v_size) { // touched_colors doesn't work properly when early-out is used
                            // read automorphism
                            for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                const int _c = touched_color_list[j];
                                int f = 0;
                                if (c1.ptn[_c] == 0) {
                                    while (f < c1.ptn[_c] + 1) {
                                        const int i = _c + f;
                                        ++f;
                                        if (c1.lab[i] != c2.lab[i]) {
                                            _automorphism[c1.lab[i]] = c2.lab[i];
                                            _automorphism_supp.push_back(c1.lab[i]);
                                        }
                                    }
                                } else {
                                    dont_bother_certify = true;
                                    break;
                                }
                            }
                        } else {
                            //for (int i = 0; i < g->v_size; ++i) {
                            for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                const int v = quotient_component_worklist_v[_i];
                                const int i = c2.vertex_to_lab[v];
                                if (c1.lab[i] != c2.lab[i]) {
                                    assert(i >= 0);
                                    assert(i < g->v_size);
                                    _automorphism[c1.lab[i]] = c2.lab[i];
                                    _automorphism_supp.push_back(c1.lab[i]);
                                }
                            }
                        }

                        //touched_support += _automorphism_supp.cur_pos;
                        touched_current_component = true;
                        certify = !dont_bother_certify &&
                                  R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                                 _automorphism_supp.cur_pos,
                                                                 _automorphism_supp.get_array());

                        assert(certify ? R1->certify_automorphism(g, _automorphism.get_array()) : true);
                        if (certify) {
                            ++automorphisms_found;
                            pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                     _automorphism_supp.get_array(),
                                     consume);
                            multiply_to_group_size(2);
                            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                               _automorphism_supp.get_array());
                            _automorphism_supp.reset();

                            if(touched_color_list.cur_pos ==
                               static_cast<int>(quotient_component_end_pos_v - quotient_component_start_pos_v)) {
                                touched_current_component = false;
                            } else {
                                reset_coloring_to_coloring_touched(g, &c2, &c1,
                                                                   static_cast<int>(quotient_component_start_pos_v),
                                                                   quotient_component_end_pos_v);
                            }
                            if (c1.cells == g->v_size) {
                                for (int _i = static_cast<int>(quotient_component_start_pos_v);
                                     _i < quotient_component_end_pos_v; ++_i) {
                                    const int v = quotient_component_worklist_v[_i];
                                    colmap[v] = c1.vertex_to_col[v];
                                }
                                touched_current_component = false;
                                break;
                            }
                        } else { // save entire coloring, including lab, ptn, v_to_lab
                            // TODO: switch to c3 method?
                            save_colmap_v_to_col.clear();
                            save_colmap_v_to_lab.clear();
                            save_colmap_ptn.clear();
                            for (int _i = static_cast<int>(quotient_component_start_pos_v);
                                 _i < quotient_component_end_pos_v; ++_i) {
                                const int v = quotient_component_worklist_v[_i];
                                save_colmap_v_to_col.push_back(c1.vertex_to_col[v]);
                                const int lab_pos = c1.vertex_to_lab[v];
                                save_colmap_v_to_lab.push_back(lab_pos);
                                save_colmap_ptn.push_back(c1.ptn[lab_pos]);
                                save_cell_number = c1.cells;
                                assert(c1.lab[lab_pos] == v);
                            }

                            int col = -1;

                            certify = false;
                            int last_failure = -1;

                            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                               _automorphism_supp.get_array());
                            _automorphism_supp.reset();

                            int len = 0;

                            int hint = -1;

                            while (true) {
                                // stay within component!
                                int singleton_check_pos = 0;
                                ind_cols.clear();
                                cell_cnt.clear();

                                //Ivec_wr.reset_compare_invariant();
                                //Ivec_wr.reset();
                                I1 = 0;

                                while (true) {
                                    ++len;
                                    auto const ret = select_color_component_large_touched(&c1,
                                                                                          quotient_component_start_pos,
                                                                                          quotient_component_end_pos,
                                                                                          hint, &touched_color);
                                    col = ret.first;
                                    hint = ret.second;
                                    if (col == -1) break;
                                    const int rpos = col;
                                    const int v1 = c1.lab[rpos];
                                    const int init_color_class1 = R1->individualize_vertex(&c1, v1);

                                    if (!touched_color.get(init_color_class1)) {
                                        touched_color.set(init_color_class1);
                                        touched_color_list.push_back(init_color_class1);
                                    }
                                    if (!touched_color.get(col)) {
                                        touched_color.set(col);
                                        touched_color_list.push_back(col);
                                    }

                                    //R1->refine_coloring(g, &c1, &Ivec_wr, init_color_class1, -1, &touched_color,
                                    //                    &touched_color_list);
                                    refine_coloring(g, &c1, &I1, init_color_class1, -1, &touched_color,
                                                    &touched_color_list);
                                    ind_cols.push_back(rpos);
                                    cell_cnt.push_back(c1.cells);
                                    int check;
                                    for (check = singleton_check_pos; check < touched_color_list.cur_pos; ++check) {
                                        if (c1.ptn[touched_color_list[check]] != 0) {
                                            singleton_check_pos = check;
                                            break;
                                        }
                                    }
                                    if (check == touched_color_list.cur_pos)
                                        break;
                                }

                                //Ivec_rd.reset_compare_invariant();
                                //Ivec_rd.set_compare_invariant(&Ivec_wr);
                                I2 = 0;
                                bool comp = true;

                                for (size_t j = 0; j < ind_cols.size(); ++j) {
                                    const int rpos = ind_cols[j];
                                    const int v2 = c2.lab[rpos];
                                    const int init_color_class2 = R1->individualize_vertex(&c2, v2);
                                    refine_coloring(g, &c2, &I2, init_color_class2, cell_cnt[j],
                                                              &touched_color, &touched_color_list);
                                }
                                comp = I1 == I2;

                                if (!comp) {
                                    break;
                                }

                                bool dont_bother_certify = false;
                                for (int j = 0;
                                     j < touched_color_list.cur_pos; ++j) { // check incremental singleton automorphisms
                                    const int _c = touched_color_list[j];
                                    int f = 0;
                                    if (c1.ptn[_c] == 0) {
                                        while (f < c1.ptn[_c] + 1) {
                                            const int i = _c + f;
                                            ++f;
                                            if (c1.lab[i] != c2.lab[i]) {
                                                _automorphism[c1.lab[i]] = c2.lab[i];
                                                _automorphism_supp.push_back(c1.lab[i]);
                                            }
                                        }
                                    } else {
                                        dont_bother_certify = true;
                                        //break;
                                    }
                                }

                                //bool check_last_failure_again;

                                certify = !dont_bother_certify;

                                if (certify && last_failure >= 0) {
                                    certify = R1->check_single_failure(g, colmap, _automorphism.get_array(),
                                                                      last_failure);
                                }

                                if (certify) {
                                    std::pair<bool, int> certify_fail = R1->certify_automorphism_sparse_report_fail(g,
                                                                                                                   colmap,
                                                                                                                   _automorphism.get_array(),
                                                                                                                   _automorphism_supp.cur_pos,
                                                                                                                   _automorphism_supp.get_array());
                                    certify = certify_fail.first;
                                    last_failure = certify_fail.second;
                                }

                                //reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos, _automorphism_supp.get_array());
                                //_automorphism_supp.reset();
                                touched_color.reset();
                                touched_color_list.reset();

                                if (certify) break;
                                if (col == -1) break;
                            }

                            if (!certify) {
                                reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                   _automorphism_supp.get_array());
                                _automorphism_supp.reset();

                                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                    const int i = quotient_component_worklist_v[_i];
                                    const int lab_p = c1.vertex_to_lab[i];
                                    assert(i >= 0);
                                    assert(i < g->v_size);
                                    if (c1.lab[lab_p] != c2.lab[lab_p]) {
                                        _automorphism[c1.lab[lab_p]] = c2.lab[lab_p];
                                        _automorphism_supp.push_back(c1.lab[lab_p]);
                                    }
                                }
                            }

                            certify = certify || R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                                                _automorphism_supp.cur_pos,
                                                                                _automorphism_supp.get_array());
                            if (certify) {
                                assert(R1->certify_automorphism(g, _automorphism.get_array()));
                                assert(_automorphism.get_array()[ind_v1] == ind_v2);
                                touched_current_component = true;
                                touched_support += _automorphism_supp.cur_pos;
                                ++automorphisms_found;
                                pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                         _automorphism_supp.get_array(),
                                         consume);
                                multiply_to_group_size(2);
                                if (c1.cells == g->v_size) {
                                    touched_current_component = false;
                                }
                            }
                            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                               _automorphism_supp.get_array());
                            _automorphism_supp.reset();

                            if (certify) {
                                for (int _i = quotient_component_start_pos_v;
                                     _i < quotient_component_end_pos_v; ++_i) { // could use touched?
                                    const int v = quotient_component_worklist_v[_i];
                                    colmap[v] = save_colmap_v_to_col[_i - quotient_component_start_pos_v];
                                }
                                for (int _i = quotient_component_start_pos_v;
                                     _i < quotient_component_end_pos_v; ++_i) { // could use touched?
                                    const int v = quotient_component_worklist_v[_i];
                                    c1.vertex_to_col[v] = save_colmap_v_to_col[_i - quotient_component_start_pos_v];
                                    const int lab_pos = save_colmap_v_to_lab[_i - quotient_component_start_pos_v];
                                    const int ptn_at_lab_pos = save_colmap_ptn[_i - quotient_component_start_pos_v];
                                    c1.vertex_to_lab[v] = lab_pos;
                                    c1.lab[lab_pos] = v;
                                    c1.ptn[lab_pos] = ptn_at_lab_pos;
                                }
                                c1.cells = save_cell_number;

                                for (int _i = quotient_component_start_pos_v;
                                     _i < quotient_component_end_pos_v; ++_i) { // could use touched?
                                    const int v = quotient_component_worklist_v[_i];
                                    const int lab_pos = save_colmap_v_to_lab[_i - quotient_component_start_pos_v];
                                    const int ptn_at_lab_pos = save_colmap_ptn[_i - quotient_component_start_pos_v];
                                    c2.vertex_to_col[v] = save_colmap_v_to_col[_i - quotient_component_start_pos_v];
                                    c2.vertex_to_lab[v] = lab_pos;
                                    c2.lab[lab_pos] = v;
                                    c2.ptn[lab_pos] = ptn_at_lab_pos;
                                }
                                c2.cells = save_cell_number;
                            } else {
                                // c1/c2 are now 'done' for this component, never touch it again
                                // can't reliably find automorphisms here
                                touched_current_component = false;
                            }

                            break;
                        }

                        reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                           _automorphism_supp.get_array());
                        _automorphism_supp.reset();

                        if (certify) {
                            if (c1.cells == g->v_size) {
                                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                    const int i = quotient_component_worklist_v[_i];
                                    assert(i >= 0);
                                    assert(i < g->v_size);
                                    colmap[i] = c1.vertex_to_col[i];
                                }
                            } else {
                                for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                    const int _c = touched_color_list[j];
                                    int f = 0;
                                    while (f < c1.ptn[_c] + 1) {
                                        assert(c1.vertex_to_col[c1.lab[_c + f]] == _c);
                                        colmap[c1.lab[_c + f]] = c1.vertex_to_col[c1.lab[_c +
                                                                                         f]]; // should be c1 or c2?
                                        ++f;
                                    }
                                }
                            }
                        }
                    }

                    if (c1.cells == g->v_size)
                        break;

                    if (touched_current_component) {
                        quotient_component_touched.push_back(component);
                        //if(touched_support*1.0 >= (g->v_size*1.0)/10) {
                        //del_discrete_edges_inplace_component(g, &c1, quotient_component_start_pos);
                        //}
                    }

                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;

                    ++component;

                    quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;

                    ++next_touched_component_i;
                    if (next_touched_component_i < quotient_component_touched_swap.size()) {
                        next_touched_component = quotient_component_touched_swap[next_touched_component_i];
                    }

                    while (component != (int) next_touched_component &&
                           quotient_component_start_pos < quotient_component_worklist_col.size()) {
                        quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                        quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;

                        ++component;

                        quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                        quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;
                    }
                }
                global_automorphisms_found += automorphisms_found;

                if (c1.cells == g->v_size)
                    break;
                if (automorphisms_found == 0)
                    break;
            }
            //PRINT("(prep-red) sparse-ir-2 completed: " << global_automorphisms_found);
            return global_automorphisms_found;
        }

        // adds a given automorphism to the orbit structure
        void add_automorphism_to_orbit(dejavu::groups::orbit *orbit, int *automorphism, int nsupp, int *supp) {
            for (int i = 0; i < nsupp; ++i) {
                orbit->combine_orbits(automorphism[supp[i]], supp[i]);
            }
        }

        // select a non-trivial color class within a given component
        int select_color_component_min_cost(dejavu::sgraph *g, coloring *c1, int component_start_pos, int component_end_pos,
                                            int max_cell_size) {
            bool only_discrete_prev = true;
            int cell = -1;
            // int cell_d = 1;
            for (int _i = component_start_pos; _i < component_end_pos; ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }
                for (int i = col; i < col + col_sz + 1; ++i) {
                    if (c1->vertex_to_col[c1->lab[i]] != i)
                        continue; // not a color
                    if (c1->ptn[i] > 0 && only_discrete_prev) {
                        //start_search_here = i;
                        only_discrete_prev = false;
                    }
                    const int i_d = g->d[c1->lab[i]];

                    if (c1->ptn[i] >= 1 && (i_d == 0)) {
                        cell = i;
                        break;
                    }
                    if (c1->ptn[i] >= 1 && c1->ptn[i] + 1 <= max_cell_size && (i_d != 1)) { // && (i_d != 1)
                        if (cell == -1 || c1->ptn[i] < c1->ptn[cell]) {
                            cell = i;
                            // cell_d = i_d;
                        }
                    }
                }

                if (cell != -1 && (g->d[c1->lab[cell]] == 0)) {
                    cell = -1;
                    break;
                }

                if (cell != -1 && c1->ptn[cell] < 3)
                    break;
            }

            return cell;
        }

        void reset_coloring_to_coloring_touched(dejavu::sgraph *g, coloring *c_to, coloring *c_from,
                                                int quotient_component_start_pos_v, int quotient_component_end_pos_v) {
            worklist_deg0.reset();

            if (g->v_size == c_to->cells || touched_color_list.cur_pos >=
                                            (0.25 * (quotient_component_end_pos_v - quotient_component_start_pos_v))) {
                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                    worklist_deg0.push_back(c_from->vertex_to_lab[quotient_component_worklist_v[_i]]);
                }
                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                    const int v = quotient_component_worklist_v[_i];
                    const int lab_pos = worklist_deg0[_i - quotient_component_start_pos_v];
                    c_to->vertex_to_lab[v] = lab_pos;
                    c_to->lab[lab_pos] = v;
                }
                for (int _i = 0; _i < worklist_deg0.cur_pos; ++_i) {
                    const int lab_pos = worklist_deg0[_i];
                    c_to->ptn[lab_pos] = c_from->ptn[lab_pos];
                }
                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                    const int v = quotient_component_worklist_v[_i];
                    c_to->vertex_to_col[v] = c_from->vertex_to_col[v];
                }
                c_to->cells = c_from->cells;
            } else {
                for (int _i = 0; _i < touched_color_list.cur_pos; ++_i) {
                    const int reset_col = touched_color_list[_i];
                    if (c_from->vertex_to_col[c_from->lab[reset_col]] == reset_col) {
                        for (int l = 0; l < c_from->ptn[reset_col] + 1; ++l) {
                            const int v = c_from->lab[reset_col + l];
                            const int lab_pos = c_from->vertex_to_lab[v];
                            assert(lab_pos == reset_col + l);
                            c_to->vertex_to_col[v] = c_from->vertex_to_col[v];
                            c_to->vertex_to_lab[v] = lab_pos;
                            c_to->lab[lab_pos] = v;
                            c_to->ptn[lab_pos] = c_from->ptn[lab_pos];
                        }
                    }
                }
                c_to->cells = c_from->cells;
            }
        }

        // perform sparse probing for color classes of bounded size, num_paths number of times
        int sparse_ir_probe_quotient_components(dejavu::sgraph *g, int *colmap, dejavu_hook* consume, int max_col_size,
                                                int num_paths) {
            if (g->v_size <= 1 || num_paths <= 0 || CONFIG_PREP_DEACT_PROBE)
                return 0;

            avg_support_sparse_ir = 0;
            avg_reached_end_of_component = 0;
            int avg_support_sparse_ir_num = 0;
            int avg_support_sparse_ir_num2 = 0;

            quotient_component_touched.clear();
            quotient_component_touched_swap.clear();

            coloring c1;
            g->initialize_coloring(&c1, colmap);

            for (int i = 0; i < g->v_size; ++i)
                colmap[i] = c1.vertex_to_col[i];

            int global_automorphisms_found = 0;

            assure_ir_quotient_init(g);

            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos, _automorphism_supp.get_array());
            _automorphism_supp.reset();

            coloring c2;
            c2.copy(&c1);
            c2.copy_ptn(&c1);

            coloring c3;
            c3.copy(&c1);
            c3.copy_ptn(&c1);

            int penalty = 0;

            for (int x = 0; x < num_paths - penalty; ++x) {
                if (x > 0 && quotient_component_touched.empty()) {
                    break;
                }

                compute_quotient_graph_components_update(g, &c1);

                int automorphisms_found = 0;

                quotient_component_touched_swap.clear();
                quotient_component_touched_swap.swap(quotient_component_touched);

                if (x == 0) {
                    for (size_t i = 0; i < quotient_component_worklist_boundary.size(); ++i)
                        quotient_component_touched_swap.push_back(i);
                }

                if (quotient_component_touched_swap.size() < 2) {
                    break; // added this
                    ++penalty;
                }
                if (quotient_component_touched_swap.size() < 3)
                    ++penalty;


                touched_color.reset();
                touched_color_list.reset();

                unsigned long I1 = 0, I2 = 0;

                bool certify = true;
                int quotient_component_start_pos = 0;
                int quotient_component_start_pos_v = 0;

                int component = 0;
                int next_touched_component_i = 0;
                int next_touched_component = -1;
                if (next_touched_component_i < (int) quotient_component_touched_swap.size()) {
                    next_touched_component = quotient_component_touched_swap[next_touched_component_i];
                } else {
                    component = quotient_component_worklist_boundary.size() - 1;
                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;
                }
                while (component < next_touched_component) {
                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;
                    ++component;
                }

                int quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                int quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;

                while (quotient_component_start_pos < (int) quotient_component_worklist_col.size()) {
                    assert(quotient_component_start_pos_v < (int) quotient_component_worklist_v.size());
                    // int start_search_here = quotient_component_start_pos;

                    assert(quotient_component_worklist_col[quotient_component_start_pos] >= 0);
                    assert(quotient_component_worklist_v[quotient_component_start_pos_v] >= 0);
                    certify = true;
                    bool touched_current_component = false;

                    while (certify) { // (component == next_touched_component || quotient_component_touched_swap.empty())
                        // select a color class of size 2
                        int cell = select_color_component_min_cost(g, &c1, quotient_component_start_pos,
                                                                   quotient_component_end_pos, max_col_size);
                        orbit.reset();

                        if (cell == -1) break;
                        const int cell_sz = c1.ptn[cell];

                        touched_color.reset();
                        touched_color_list.reset();

                        I1 = 0;
                        I2 = 0;

                        const int ind_v1 = c1.lab[cell];
                        const int ind_v2 = c1.lab[cell + 1];
                        const int init_c1 = R1->individualize_vertex(&c1, ind_v1);

                        touched_color.set(cell);
                        touched_color.set(init_c1);
                        touched_color_list.push_back(cell);
                        touched_color_list.push_back(init_c1);
                        assert(cell != init_c1);

                        //R1->refine_coloring(g, &c1, &I1, init_c1, -1, &touched_color, &touched_color_list);
                        refine_coloring(g, &c1, &I1, init_c1, -1,
                                        &touched_color, &touched_color_list);

                        if (c2.ptn[cell] != 0) {
                            const int init_c2 = R1->individualize_vertex(&c2, ind_v2);
                            //R1->refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color, &touched_color_list);
                            refine_coloring(g, &c2, &I2, init_c2, -1,
                                            &touched_color, &touched_color_list);
                        } else {
                            I2 = I1 + 1;
                        }

                        if (I1 != I2) {
                            I2 = I1;
                            // could use component!
                            c1.copy(&c3);
                            c2.copy(&c3);
                            touched_current_component = false;
                            break;
                        }

                        reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                           _automorphism_supp.get_array());
                        _automorphism_supp.reset();
                        if (g->v_size == c1.cells || touched_color_list.cur_pos >= (0.25 *
                                                                                    (quotient_component_end_pos_v -
                                                                                     quotient_component_start_pos_v))) {
                            //for (int i = 0; i < g->v_size; ++i) {
                            for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                assert(_i >= 0);
                                assert(_i < g->v_size);
                                const int v = quotient_component_worklist_v[_i];
                                assert(v >= 0);
                                assert(v < g->v_size);
                                const int i = c2.vertex_to_lab[v];
                                assert(i >= 0);
                                assert(i < g->v_size);
                                if (c1.lab[i] != c2.lab[i]) {
                                    _automorphism[c1.lab[i]] = c2.lab[i];
                                    _automorphism_supp.push_back(c1.lab[i]);
                                }
                            }
                        } else {
                            // read automorphism
                            for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                const int _c = touched_color_list[j];
                                int f = 0;
                                //if(c1.ptn[_c] == 0) {
                                while (f < c1.ptn[_c] + 1) {
                                    const int i = _c + f;
                                    ++f;
                                    if (c1.lab[i] != c2.lab[i]) {
                                        _automorphism[c1.lab[i]] = c2.lab[i];
                                        _automorphism_supp.push_back(c1.lab[i]);
                                    }
                                }
                                //}
                            }
                        }

                        certify = R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                                 _automorphism_supp.cur_pos,
                                                                 _automorphism_supp.get_array());
                        assert(certify ? R1->certify_automorphism(g, _automorphism.get_array()) : true);
                        bool all_certified = certify;
                        if (certify) {
                            avg_support_sparse_ir += _automorphism_supp.cur_pos;
                            avg_support_sparse_ir_num += 1;
                            pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                     _automorphism_supp.get_array(),
                                     consume);
                            assert(_automorphism[ind_v1] == ind_v2);
                            add_automorphism_to_orbit(&orbit, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                                      _automorphism_supp.get_array());

                            for (int k = 2; (k < cell_sz + 1) && all_certified; ++k) {
                                // reset c2 to before individualization
                                assert(touched_color_list.cur_pos <=
                                       quotient_component_end_pos_v - quotient_component_start_pos_v);
                                const int ind_vk = c3.lab[cell + k];
                                if (orbit.are_in_same_orbit(ind_v1, ind_vk)) {
                                    continue;
                                }

                                reset_coloring_to_coloring_touched(g, &c2, &c3, quotient_component_start_pos_v,
                                                                   quotient_component_end_pos_v);

                                // individualize and test cell + k
                                const int init_ck = R1->individualize_vertex(&c2, ind_vk);
                                I2 = 0;
                                //R1->refine_coloring(g, &c2, &I2, init_ck, -1, &touched_color, &touched_color_list);
                                refine_coloring(g, &c2, &I2, init_ck, -1, &touched_color, &touched_color_list);
                                reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                   _automorphism_supp.get_array());
                                _automorphism_supp.reset();

                                if (I1 == I2 && c1.cells == c2.cells) {
                                    if (c1.cells !=
                                        g->v_size) { // touched_colors doesn't work properly when early-out is used
                                        // read automorphism
                                        for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                            const int _c = touched_color_list[j];
                                            int f = 0;
                                            while (f < c1.ptn[_c] + 1) {
                                                const int i = _c + f;
                                                ++f;
                                                if (c1.lab[i] != c2.lab[i]) {
                                                    _automorphism[c1.lab[i]] = c2.lab[i];
                                                    _automorphism_supp.push_back(c1.lab[i]);
                                                }
                                            }
                                        }
                                    } else {
                                        //for (int i = 0; i < g->v_size; ++i) {
                                        for (int _i = quotient_component_start_pos_v;
                                             _i < quotient_component_end_pos_v; ++_i) {
                                            const int v = quotient_component_worklist_v[_i];
                                            const int i = c2.vertex_to_lab[v];
                                            if (c1.lab[i] != c2.lab[i]) {
                                                assert(i >= 0);
                                                assert(i < g->v_size);
                                                _automorphism[c1.lab[i]] = c2.lab[i];
                                                _automorphism_supp.push_back(c1.lab[i]);
                                            }
                                        }
                                    }
                                    certify = R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                                             _automorphism_supp.cur_pos,
                                                                             _automorphism_supp.get_array());
                                    if (certify) {
                                        avg_support_sparse_ir += _automorphism_supp.cur_pos;
                                        avg_support_sparse_ir_num += 1;
                                        pre_hook(g->v_size, _automorphism.get_array(),
                                                 _automorphism_supp.cur_pos,
                                                 _automorphism_supp.get_array(),
                                                 consume);
                                        assert(_automorphism[ind_v1] == ind_vk);
                                        add_automorphism_to_orbit(&orbit, _automorphism.get_array(),
                                                                  _automorphism_supp.cur_pos,
                                                                  _automorphism_supp.get_array());
                                        //reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                        //                   _automorphism_supp.get_array());
                                        //_automorphism_supp.reset();
                                    }
                                    reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                       _automorphism_supp.get_array());
                                    _automorphism_supp.reset();
                                } else {
                                    certify = false;
                                }
                                all_certified = certify && all_certified;
                            }

                            if (all_certified) {
                                automorphisms_found += cell_sz;
                                touched_current_component = true;
                                avg_support_sparse_ir_num2 += 1;
                                multiply_to_group_size(cell_sz + 1);
                                // reset c2 and c3 to c1
                                if (c1.cells != g->v_size) {
                                    reset_coloring_to_coloring_touched(g, &c2, &c1, quotient_component_start_pos_v,
                                                                       quotient_component_end_pos_v);
                                    reset_coloring_to_coloring_touched(g, &c3, &c1, quotient_component_start_pos_v,
                                                                       quotient_component_end_pos_v);
                                }
                            }
                        }

                        if (!all_certified) {
                            int col = -1;
                            certify = false;
                            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                               _automorphism_supp.get_array());
                            _automorphism_supp.reset();

                            // reset c2 to before individualization
                            assert(touched_color_list.cur_pos <=
                                   quotient_component_end_pos_v - quotient_component_start_pos_v);
                            reset_coloring_to_coloring_touched(g, &c2, &c3, quotient_component_start_pos_v,
                                                               quotient_component_end_pos_v);

                            I2 = 0;
                            const int init_c2 = R1->individualize_vertex(&c2, ind_v2);
                            //R1->refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color, &touched_color_list);
                            refine_coloring(g, &c2, &I2, init_c2, -1, &touched_color, &touched_color_list);

                            int num_inds = 0;
                            // int touched_start_pos = 0;

                            touched_color_cache.reset();
                            touched_color_list_cache.reset();

                            for (int k = 0; k < touched_color_list.cur_pos; ++k) {
                                touched_color_cache.set(touched_color_list[k]);
                                touched_color_list_cache[k] = touched_color_list[k];
                            }
                            touched_color_list_cache.cur_pos = touched_color_list.cur_pos;

                            int last_failure = -1;
                            int hint = -1;
                            ind_cols.clear();
                            cell_cnt.clear();
                            while (true) {
                                // stay within component!
                                auto const ret = select_color_component(&c1, quotient_component_start_pos,
                                                                        quotient_component_end_pos, hint);
                                col = ret.first;
                                hint = ret.second;
                                if (col == -1) break;
                                const int rpos = col + (0 % (c1.ptn[col] + 1));
                                const int v1 = c1.lab[rpos];
                                const int init_color_class1 = R1->individualize_vertex(&c1, v1);

                                if (!touched_color_cache.get(init_color_class1)) {
                                    touched_color_cache.set(init_color_class1);
                                    touched_color_list_cache.push_back(init_color_class1);
                                }
                                if (!touched_color_cache.get(col)) {
                                    touched_color_cache.set(col);
                                    touched_color_list_cache.push_back(col);
                                }

                                //R1->refine_coloring(g, &c1, &I1, init_color_class1, -1, &touched_color_cache,
                                //                    &touched_color_list_cache);
                                refine_coloring(g, &c1, &I1, init_color_class1, -1,
                                                &touched_color_cache,&touched_color_list_cache);


                                ind_cols.push_back(col);
                                cell_cnt.push_back(c1.cells);

                                //const int rpos = col + (intRand(0, INT32_MAX, selector_seed) % (c2.ptn[col] + 1));
                                const int v2 = c2.lab[rpos];
                                if (c2.ptn[col] == 0) {
                                    I2 = I1 + 1;
                                    certify = false;
                                    break;
                                }
                                const int init_color_class2 = R1->individualize_vertex(&c2, v2);
                                //R1->refine_coloring(g, &c2, &I2, init_color_class2, -1, &touched_color_cache,
                                //                    &touched_color_list_cache);
                                refine_coloring(g, &c2, &I2, init_color_class2, -1, &touched_color_cache,
                                                &touched_color_list_cache);

                                if (c1.cells == g->v_size) {
                                    for (int _i = quotient_component_start_pos_v;
                                         _i < quotient_component_end_pos_v; ++_i) {
                                        const int v = quotient_component_worklist_v[_i];
                                        const int v_col1 = c1.vertex_to_col[v];
                                        if (!touched_color_cache.get(v_col1) && !touched_color.get(v_col1)) {
                                            touched_color_cache.set(v_col1);
                                            touched_color_list_cache.push_back(v_col1);
                                        }
                                    }
                                }

                                //assert(I1.acc == I2.acc);

                                bool dont_bother_certify = false;
                                for (int j = 0; j <
                                                touched_color_list_cache.cur_pos; ++j) { // check incremental singleton automorphisms
                                    const int _c = touched_color_list_cache[j];
                                    if (!touched_color.get(_c)) {
                                        touched_color.set(_c);
                                        touched_color_list.push_back(_c);
                                    }
                                    int f = 0;
                                    assert(c1.cells != g->v_size || (c1.ptn[_c] == 0));
                                    if (c1.ptn[_c] == 0 && c2.ptn[_c] == 0) {
                                        while (f < c1.ptn[_c] + 1) {
                                            const int i = _c + f;
                                            ++f;
                                            if (c1.lab[i] != c2.lab[i] && _automorphism[c1.lab[i]] == c1.lab[i]) {
                                                _automorphism[c1.lab[i]] = c2.lab[i];
                                                _automorphism_supp.push_back(c1.lab[i]);
                                            }
                                        }
                                    } else {
                                        dont_bother_certify = true;
                                    }
                                }

                                ++num_inds;

                                certify = !dont_bother_certify;
                                if (certify && last_failure >= 0) {
                                    certify = R1->check_single_failure(g, colmap, _automorphism.get_array(),
                                                                      last_failure);
                                }

                                if (certify) {
                                    std::pair<bool, int> certify_fail =
                                            R1->certify_automorphism_sparse_report_fail(g, colmap,
                                                                                       _automorphism.get_array(),
                                                                                       _automorphism_supp.cur_pos,
                                                                                       _automorphism_supp.get_array());
                                    certify = certify_fail.first;
                                    last_failure = certify_fail.second;
                                }

                                touched_color_cache.reset();
                                touched_color_list_cache.reset();

                                if (certify) break;
                            }

                            if (!certify && (g->v_size == c1.cells)) {
                                reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                   _automorphism_supp.get_array());
                                _automorphism_supp.reset();
                                for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                    const int _c = touched_color_list[j];
                                    int f = 0;
                                    assert(c1.cells != g->v_size || (c1.ptn[_c] == 0));
                                    if (c1.ptn[_c] == 0) {
                                        while (f < c1.ptn[_c] + 1) {
                                            const int i = _c + f;
                                            ++f;
                                            if (c1.lab[i] != c2.lab[i]) {
                                                assert(_automorphism[c1.lab[i]] == c1.lab[i]);
                                                _automorphism[c1.lab[i]] = c2.lab[i];
                                                _automorphism_supp.push_back(c1.lab[i]);
                                            }
                                        }
                                    }
                                }
                                certify = R1->certify_automorphism_sparse(g, colmap, _automorphism.get_array(),
                                                                         _automorphism_supp.cur_pos,
                                                                         _automorphism_supp.get_array());
                            }

                            // bool certify_before = certify;

                            if (certify) {
                                assert(R1->certify_automorphism(g, _automorphism.get_array()));
                                avg_support_sparse_ir += _automorphism_supp.cur_pos;
                                avg_support_sparse_ir_num += 1;
                                pre_hook(g->v_size, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                         _automorphism_supp.get_array(),
                                         consume);
                                add_automorphism_to_orbit(&orbit, _automorphism.get_array(), _automorphism_supp.cur_pos,
                                                          _automorphism_supp.get_array());
                                // multiply_to_group_size(2);
                                all_certified = true;

                                reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                   _automorphism_supp.get_array());
                                _automorphism_supp.reset();

                                for (int k = 2; (k < cell_sz + 1) && all_certified; ++k) {
                                    const int ind_vk = c3.lab[cell + k];
                                    if (orbit.are_in_same_orbit(ind_v1, ind_vk)) {
                                        continue;
                                    }

                                    // reset c2 to before individualization
                                    assert(touched_color_list.cur_pos <=
                                           quotient_component_end_pos_v - quotient_component_start_pos_v);
                                    reset_coloring_to_coloring_touched(g, &c2, &c3, quotient_component_start_pos_v,
                                                                       quotient_component_end_pos_v);

                                    // individualize and test cell + k
                                    const int init_ck = R1->individualize_vertex(&c2, ind_vk);
                                    if (!touched_color.get(cell)) {
                                        touched_color.set(cell);
                                        touched_color_list.push_back(cell);
                                    }
                                    if (!touched_color.get(init_ck)) {
                                        touched_color.set(init_ck);
                                        touched_color_list.push_back(init_ck);
                                    }
                                    I2 = 0;
                                    //R1->refine_coloring(g, &c2, &I2, init_ck, -1, &touched_color, &touched_color_list);
                                    refine_coloring(g, &c2, &I2, init_ck, -1, &touched_color, &touched_color_list);
                                    reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                       _automorphism_supp.get_array());
                                    _automorphism_supp.reset();

                                    for (int i = 0; i < g->v_size; ++i) {
                                        assert(_automorphism[i] == i);
                                    }

                                    int repeat_num_inds = 0;

                                    bool should_add_ind = false;
                                    do {
                                        hint = -1;
                                        while (repeat_num_inds < num_inds) {
                                            // stay within component!
                                            /*auto ret = select_color_component(g, &c2, quotient_component_start_pos,
                                                                            quotient_component_end_pos, hint);
                                        col  = ret.first;
                                        hint = ret.second;*/

                                            col = ind_cols[repeat_num_inds];

                                            assert(repeat_num_inds < (int) ind_cols.size());
                                            assert(num_inds == (int) ind_cols.size());
                                            assert(col == ind_cols[repeat_num_inds]);

                                            if (col == -1) {
                                                break;
                                            }
                                            const int rpos =
                                                    col + (0 % (c2.ptn[col] + 1));
                                            if (c2.ptn[col] == 0) {
                                                I2 = I1 + 1;
                                                break;
                                            }
                                            const int v2 = c2.lab[rpos];
                                            const int init_color_class2 = R1->individualize_vertex(&c2, v2);
                                            if (!touched_color.get(init_color_class2)) {
                                                touched_color.set(init_color_class2);
                                                touched_color_list.push_back(init_color_class2);
                                            }
                                            if (!touched_color.get(col)) {
                                                touched_color.set(col);
                                                touched_color_list.push_back(col);
                                            }
                                            //R1->refine_coloring(g, &c2, &I2, init_color_class2, -1, &touched_color,
                                            //                   &touched_color_list); // cell_cnt[repeat_num_inds]
                                            refine_coloring(g, &c2, &I2, init_color_class2, -1, &touched_color,
                                                            &touched_color_list);
                                            ++repeat_num_inds;
                                        }

                                        should_add_ind = false;

                                        if (I1 == I2 && c1.cells == c2.cells) {
                                            for (int j = 0; j <
                                                            touched_color_list.cur_pos; ++j) { // check the singleton automorphism
                                                const int _c = touched_color_list[j];
                                                int f = 0;
                                                if (c1.ptn[_c] == 0) {
                                                    while (f < c1.ptn[_c] + 1) {
                                                        const int i = _c + f;
                                                        ++f;
                                                        if (c1.lab[i] != c2.lab[i]) {
                                                            //assert(_automorphism[c1.lab[i]] == c1.lab[i]);
                                                            _automorphism[c1.lab[i]] = c2.lab[i];
                                                            _automorphism_supp.push_back(c1.lab[i]);
                                                        }
                                                    }
                                                }
                                            }

                                            certify = R1->certify_automorphism_sparse(g, colmap,
                                                                                     _automorphism.get_array(),
                                                                                     _automorphism_supp.cur_pos,
                                                                                     _automorphism_supp.get_array());
                                            should_add_ind = !certify;
                                        } else {
                                            certify = false;
                                        }

                                        if (should_add_ind) {
                                            auto ret = select_color_component(&c1, quotient_component_start_pos,
                                                                              quotient_component_end_pos, -1);
                                            col = ret.first;
                                            if (col == -1) {
                                                should_add_ind = false;
                                                break;
                                            }
                                            const int rpos = col + (0 % (c1.ptn[col] + 1));
                                            const int v1 = c1.lab[rpos];
                                            const int init_color_class1 = R1->individualize_vertex(&c1, v1);
                                            //R1->refine_coloring(g, &c1, &I1, init_color_class1, -1, &touched_color,
                                            //                    &touched_color_list);
                                            refine_coloring(g, &c1, &I1, init_color_class1, -1, &touched_color,
                                                            &touched_color_list);

                                            ind_cols.push_back(col);
                                            cell_cnt.push_back(c1.cells);
                                            num_inds += 1;

                                            reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                               _automorphism_supp.get_array());
                                            _automorphism_supp.reset();
                                        }
                                    } while (should_add_ind);

                                    all_certified = certify && all_certified;
                                    if (certify) {
                                        avg_support_sparse_ir += _automorphism_supp.cur_pos;
                                        avg_support_sparse_ir_num += 1;
                                        pre_hook(g->v_size, _automorphism.get_array(),
                                                 _automorphism_supp.cur_pos,
                                                 _automorphism_supp.get_array(),
                                                 consume);
                                        assert(_automorphism[ind_v1] == ind_vk);
                                        add_automorphism_to_orbit(&orbit, _automorphism.get_array(),
                                                                  _automorphism_supp.cur_pos,
                                                                  _automorphism_supp.get_array());
                                    }
                                    reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                       _automorphism_supp.get_array());
                                    _automorphism_supp.reset();
                                }

                                if (all_certified) {
                                    automorphisms_found += cell_sz + 1;
                                    multiply_to_group_size(cell_sz + 1);
                                    touched_current_component = true;

                                    avg_support_sparse_ir_num2 += 1;
                                    auto const ret = select_color_component(&c1, quotient_component_start_pos,
                                                                            quotient_component_end_pos, -1);
                                    const int test_col = ret.first;
                                    if (test_col == -1) {
                                        avg_reached_end_of_component += 1;
                                        touched_current_component = false;
                                    }
                                }
                            } else {
                                reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                                   _automorphism_supp.get_array());
                                _automorphism_supp.reset();
                            }

                            certify = all_certified;

                            if (certify) {
                                // int f = 0;
                                I1 = 0;
                                assert(touched_color.get(c3.vertex_to_col[ind_v1]));
                                const int init_c3 = R1->individualize_vertex(&c3, ind_v1);
                                assert(touched_color.get(init_c3));
                                [[maybe_unused]] const int touched_col_prev = touched_color_list.cur_pos;
                                //R1->refine_coloring(g, &c3, &I1, init_c3, -1, &touched_color, &touched_color_list);
                                refine_coloring(g, &c3, &I1, init_c3, -1, &touched_color, &touched_color_list);
                                assert(touched_col_prev == touched_color_list.cur_pos);

                                assert(touched_color_list.cur_pos <=
                                       quotient_component_end_pos_v - quotient_component_start_pos_v);
                                if (g->v_size == c1.cells || touched_color_list.cur_pos >= (0.5 *
                                                                                            (quotient_component_end_pos_v -
                                                                                             quotient_component_start_pos_v))) {
                                    for (int _i = quotient_component_start_pos_v;
                                         _i < quotient_component_end_pos_v; ++_i) {
                                        const int v = quotient_component_worklist_v[_i];
                                        colmap[v] = c3.vertex_to_col[v];
                                    }
                                } else {
                                    for (int _i = 0; _i < touched_color_list.cur_pos; ++_i) {
                                        const int reset_col = touched_color_list[_i];
                                        if (c3.vertex_to_col[c3.lab[reset_col]] == reset_col) {
                                            for (int k = 0; k < c3.ptn[reset_col] + 1; ++k) {
                                                const int v = c3.lab[reset_col + k];
                                                colmap[v] = c3.vertex_to_col[v];
                                            }
                                        }
                                    }
                                }
                                reset_coloring_to_coloring_touched(g, &c1, &c3, quotient_component_start_pos_v,
                                                                   quotient_component_end_pos_v);
                                reset_coloring_to_coloring_touched(g, &c2, &c3, quotient_component_start_pos_v,
                                                                   quotient_component_end_pos_v);
                            } else {
                                // int f = 0;
                                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                    const int v = quotient_component_worklist_v[_i];

                                    // reset c1 / c2
                                    c1.vertex_to_col[v] = c3.vertex_to_col[v];
                                    c1.vertex_to_lab[v] = c3.vertex_to_lab[v];
                                    const int lab_pos = c3.vertex_to_lab[v];
                                    c1.lab[lab_pos] = v;
                                    c1.ptn[lab_pos] = c3.ptn[lab_pos];
                                    c1.cells = c3.cells;

                                    c2.vertex_to_col[v] = c3.vertex_to_col[v];
                                    c2.vertex_to_lab[v] = c3.vertex_to_lab[v];
                                    c2.lab[lab_pos] = v;
                                    c2.ptn[lab_pos] = c3.ptn[lab_pos];
                                    c2.cells = c3.cells;
                                }
                                // we did not find an automorphism
                                // c1/c2 are now 'done' for this component, i.e., we can not find automorphisms here
                                // so we just never touch it again
                                touched_current_component = false;
                            }

                            break;
                        }
                        reset_automorphism(_automorphism.get_array(), _automorphism_supp.cur_pos,
                                           _automorphism_supp.get_array());
                        _automorphism_supp.reset();

                        if (certify) {
                            if (c1.cells == g->v_size) {
                                for (int _i = quotient_component_start_pos_v; _i < quotient_component_end_pos_v; ++_i) {
                                    const int i = quotient_component_worklist_v[_i];
                                    assert(i >= 0);
                                    assert(i < g->v_size);
                                    colmap[i] = c1.vertex_to_col[i];
                                }
                            } else {
                                for (int j = 0; j < touched_color_list.cur_pos; ++j) {
                                    const int _c = touched_color_list[j];
                                    int f = 0;
                                    while (f < c1.ptn[_c] + 1) {
                                        assert(c1.vertex_to_col[c1.lab[_c + f]] == _c);
                                        colmap[c1.lab[_c + f]] = c1.vertex_to_col[c1.lab[_c +
                                                                                         f]]; // should be c1 or c2?
                                        ++f;
                                    }
                                }
                            }
                        }
                    }

                    if (touched_current_component) {
                        quotient_component_touched.push_back(component);
                        del_discrete_edges_inplace_component(g, &c1, quotient_component_start_pos);
                    }

                    quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;

                    ++component;

                    quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                    quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;

                    ++next_touched_component_i;
                    if (next_touched_component_i < (int) quotient_component_touched_swap.size()) {
                        next_touched_component = quotient_component_touched_swap[next_touched_component_i];
                    }

                    while (component != next_touched_component &&
                           quotient_component_start_pos < (int) quotient_component_worklist_col.size()) {
                        quotient_component_start_pos = quotient_component_worklist_boundary[component].first;
                        quotient_component_start_pos_v = quotient_component_worklist_boundary[component].second;

                        ++component;

                        quotient_component_end_pos = quotient_component_worklist_boundary[component].first;
                        quotient_component_end_pos_v = quotient_component_worklist_boundary[component].second;
                    }
                }
                global_automorphisms_found += automorphisms_found;
                if (automorphisms_found == 0)
                    break;
            }

            if (avg_support_sparse_ir > 0 && avg_support_sparse_ir_num > 0) {
                avg_support_sparse_ir = avg_support_sparse_ir / avg_support_sparse_ir_num;
            }
            if (avg_support_sparse_ir_num2 > 0) {
                avg_end_of_comp = (1.0 * avg_reached_end_of_component) / (avg_support_sparse_ir_num2 * 1.0);
            } else {
                avg_end_of_comp = 0;
            }

            //PRINT("(prep-red) sparse-ir-k completed: " << global_automorphisms_found);
            return global_automorphisms_found;
        }

        // deletes edges connected to discrete vertices, and marks discrete vertices for deletion later
        void del_discrete_edges_inplace(dejavu::sgraph *g, coloring *c) {
            int rem_edges = 0;
            int discrete_vert = 0;
            del.reset();
            for (int i = 0; i < c->lab_sz;) {
                const int col_sz = c->ptn[i];
                if (col_sz == 0) {
                    ++discrete_vert;
                    del.set(c->lab[i]);
                }
                i += col_sz + 1;
            }

            for (int v = 0; v < g->v_size; ++v) {
                if (del.get(v)) {
                    assert(c->ptn[c->vertex_to_col[v]] == 0);
                    rem_edges += g->d[v];
                    g->d[v] = 0;
                    continue;
                }

                for (int n = g->v[v]; n < g->v[v] + g->d[v];) {
                    const int neigh = g->e[n];
                    assert(neigh >= 0 && neigh < g->v_size);
                    if (del.get(neigh)) { // neigh == -1
                        const int swap_neigh = g->e[g->v[v] + g->d[v] - 1];
                        //g->e[g->v[v] + g->d[v] - 1] = neigh; // removed this operation because unnecessary?
                        g->e[n] = swap_neigh;
                        --g->d[v];
                        ++rem_edges;
                    } else {
                        ++n;
                    }
                }
            }
            assert(rem_edges % 2 == 0);
        }

        // deletes edges connected to discrete vertices, and marks discrete vertices for deletion later, component-wise
        void del_discrete_edges_inplace_component(dejavu::sgraph *g, coloring *c, int component_start_pos) {
            int rem_edges = 0;
            int discrete_vert = 0;
            for (size_t _i = component_start_pos; _i < quotient_component_worklist_col.size(); ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }

                for (int i = col; i < col + col_sz + 1;) {
                    if (c->vertex_to_col[c->lab[i]] != i)
                        continue; // not a color
                    if (c->ptn[i] == 0) {
                        ++discrete_vert;
                        del.set(c->lab[i]);
                    }
                    i += c->ptn[i] + 1;
                }
            }

            for (size_t _i = component_start_pos; _i < quotient_component_worklist_col.size(); ++_i) {
                const int col = quotient_component_worklist_col[_i];
                const int col_sz = quotient_component_worklist_col_sz[_i];

                if (col == -1) {// reached end of component
                    break;
                }

                for (int i = col; i < col + col_sz + 1; ++i) {
                    const int v = c->lab[i];
                    if (del.get(v)) {
                        rem_edges += g->d[v];
                        g->d[v] = 0;
                        continue;
                    }
                    // int write_pt_front = g->v[v];
                    // int write_pt_back = g->v[v] + g->d[v] - 1;
                    for (int n = g->v[v]; n < g->v[v] + g->d[v];) {
                        const int neigh = g->e[n];
                        if (del.get(neigh)) {
                            const int swap_neigh = g->e[g->v[v] + g->d[v] - 1];
                            g->e[g->v[v] + g->d[v] - 1] = neigh;
                            g->e[n] = swap_neigh;
                            --g->d[v];
                            ++rem_edges;
                        } else {
                            ++n;
                        }
                    }
                }
            }
        }

        // counts vertices of degree 0, 1, 2 in g
        void count_graph_deg(dejavu::sgraph *g, int *deg0, int *deg1, int *deg2) {
            *deg0 = 0;
            *deg1 = 0;
            *deg2 = 0;
            for (int i = 0; i < g->v_size; ++i) {
                switch (g->d[i]) {
                    case 0:
                        ++*deg0;
                        break;
                    case 1:
                        ++*deg1;
                        break;
                    case 2:
                        ++*deg2;
                        break;
                    default:
                        break;
                }
            }
        }

        void order_according_to_color(dejavu::sgraph *g, int* colmap) {
            bool in_order = true;
            for(int i = 0; i < g->v_size-1; ++i) {
                in_order = in_order && (colmap[i] <= colmap[i+1]);
                if(!in_order)
                    break;
            }
            if(in_order) {
                return;
            }

            g->initialize_coloring(&c, colmap);

            dejavu::work_list old_arr(g->v_size);

            std::memcpy(old_arr.get_array(), g->v, g->v_size*sizeof(int));
            for(int j = 0; j < g->v_size; ++j) {
                g->v[j] = old_arr[c.lab[j]];
            }

            std::memcpy(old_arr.get_array(), g->d, g->v_size*sizeof(int));
            for(int j = 0; j < g->v_size; ++j) {
                old_arr[j] = g->d[j];
            }
            for(int j = 0; j < g->v_size; ++j) {
                g->d[j] = old_arr[c.lab[j]];
            }


            for(int i = 0; i < g->v_size; ++i) {
                colmap[i] = c.vertex_to_col[c.lab[i]];
            }

            for(int i = 0; i < g->v_size; ++i) {
                const int map_to = c.lab[i];
                old_arr[map_to] = i; // iso^-1
            }

            for(int j = 0; j < g->e_size; ++j) {
                g->e[j] = old_arr[g->e[j]];
            }

            assert((int)backward_translation_layers[backward_translation_layers.size() - 1].size() == g->v_size);
            for(int i = 0; i < g->v_size; ++i) {
                old_arr[i] = backward_translation_layers[backward_translation_layers.size() - 1][i];
            }

            for(int i = 0; i < g->v_size; ++i) {
                backward_translation_layers[backward_translation_layers.size() - 1][i] = old_arr[c.lab[i]];
            }

            order_edgelist(g);
        }

    public:

        void reduce(dejavu::static_graph *g, dejavu_hook* hook, std::vector<preop> *schedule = nullptr) {
            reduce(g->get_sgraph(), g->get_coloring(), hook, schedule);
        }


        // main routine of the preprocessor, reduces (g, colmap) -- returns automorphisms through hook
        // optional parameter schedule defines the order of applied techniques
        void reduce(dejavu::sgraph *g, int *colmap, dejavu_hook* hook, const std::vector<preop> *schedule = nullptr) {
            const std::vector<preop> default_schedule =
                    {deg01, qcedgeflip, deg2ma, deg2ue, probe2qc, deg2ma, probeqc, deg2ma, redloop};

            if(schedule == nullptr) {
                schedule = &default_schedule;
            }

            std::chrono::high_resolution_clock::time_point timer = std::chrono::high_resolution_clock::now();

            PRINT("____________________________________________________");
            PRINT(std::setw(16) << std::left <<"T (ms)"                                  << std::setw(16) << "after_proc"  << std::setw(10) << "#N"        << std::setw(10)        << "#M");
            PRINT("____________________________________________________");
            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "start" << std::setw(10) << g->v_size << std::setw(10) << g->e_size );

            if(CONFIG_TRANSLATE_ONLY) {
                translate_layer_fwd.reserve(g->v_size);
                backward_translation_layers.emplace_back(std::vector<int>());
                const size_t back_ind = backward_translation_layers.size() - 1;
                translation_layers.emplace_back(std::vector<int>());
                // const int fwd_ind = translation_layers.size() - 1;
                backward_translation_layers[back_ind].reserve(g->v_size);
                for (int i = 0; i < g->v_size; ++i)
                    backward_translation_layers[back_ind].push_back(i);

                automorphism.allocate(g->v_size);
                for (int i = 0; i < g->v_size; ++i) {
                    automorphism.push_back(i);
                }
                automorphism_supp.allocate(g->v_size);
                _automorphism.allocate(g->v_size);
                _automorphism_supp.allocate(g->v_size);
                for (int i = 0; i < g->v_size; ++i)
                    _automorphism[i] = i;
                aux_automorphism.allocate(g->v_size);
                for (int i = 0; i < g->v_size; ++i) {
                    aux_automorphism.push_back(i);
                }
                aux_automorphism_supp.allocate(g->v_size);

                domain_size = g->v_size;

                recovery_strings.reserve(g->v_size);
                for (int i = 0; i < domain_size; ++i) {
                    recovery_strings.emplace_back(std::vector<int>());
                }
                saved_hook = hook;
                save_preprocessor = this;
                return;
            }

            domain_size = g->v_size;
            saved_hook = hook;
            save_preprocessor = this;
            if(g->v_size == 0)
                return;
            g->dense = !(g->e_size < g->v_size || g->e_size / g->v_size < g->v_size / (g->e_size / g->v_size));

            //PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "color_setup" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);

            const int test_d = g->d[0];
            int k;
            for (k = 0; k < g->v_size && g->d[k] == test_d; ++k);

            if(k == g->v_size) {
                // graph is regular
                skipped_preprocessing = true;
                PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "regular" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                return;
            }

            backward_translation_layers.emplace_back(std::vector<int>());
            const size_t back_ind = backward_translation_layers.size() - 1;
            translation_layers.emplace_back(std::vector<int>());
            // const int fwd_ind = translation_layers.size() - 1;
            backward_translation_layers[back_ind].reserve(g->v_size);
            for (int i = 0; i < g->v_size; ++i)
                backward_translation_layers[back_ind].push_back(i);

            edge_scratch.allocate(g->e_size);

            order_according_to_color(g, colmap);
            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "color_order" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);

            g->initialize_coloring(&c, colmap);

            const int pre_v_size = g->v_size;
            const int pre_e_size = g->e_size;
            const int pre_cells  = c.cells;

            dejavu::ir::refinement R_stack = dejavu::ir::refinement();
            R1 = &R_stack;
            R1->refine_coloring_first(g, &c, -1);

            const bool color_refinement_effective = pre_cells != c.cells;

            if (c.cells == g->v_size) {
                PRINT("(prep-red) graph is discrete");
                g->v_size = 0;
                g->e_size = 0;
                return;
            }

            int deg0, deg1, deg2;
            //add_edge_buff.allocate(domain_size);
            add_edge_buff.clear();
            add_edge_buff.reserve(domain_size);
            for (int i = 0; i < domain_size; ++i)
                add_edge_buff.emplace_back(); // do this smarter... i know how many edges end up here
            add_edge_buff_act.initialize(domain_size);

            translate_layer_fwd.reserve(g->v_size);

            automorphism.allocate(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                automorphism.push_back(i);
            }
            automorphism_supp.allocate(g->v_size);
            aux_automorphism.allocate(g->v_size);
            for (int i = 0; i < g->v_size; ++i) {
                aux_automorphism.push_back(i);
            }
            aux_automorphism_supp.allocate(g->v_size);
            _automorphism.allocate(g->v_size);
            _automorphism_supp.allocate(g->v_size);
            for (int i = 0; i < g->v_size; ++i)
                _automorphism[i] = i;

            before_move.allocate(domain_size);

            worklist_deg0.allocate(g->v_size);
            worklist_deg1.allocate(g->v_size);

            domain_size = g->v_size;

            // assumes colmap is array of length g->v_size
            del = dejavu::mark_set();
            del.initialize(g->v_size);

            recovery_strings.reserve(g->v_size);
            for (int j = 0; j < domain_size; ++j) recovery_strings.emplace_back();

            // eliminate degree 1 + 0 and discrete vertices
            del_discrete_edges_inplace(g, &c);
            bool has_deg_0 = false;
            bool has_deg_1 = false;
            bool has_deg_2 = false;
            bool has_discrete = false;
            bool graph_changed = false;

            for (int i = 0; i < c.ptn_sz;) {
                const int v = c.lab[i];
                switch (g->v[v]) {
                    case 0:
                        has_deg_0 = true;
                        break;
                    case 1:
                        has_deg_1 = true;
                        break;
                    case 2:
                        has_deg_2 = true;
                        break;
                    default:
                        break;
                }
                const int col_sz = c.ptn[i] + 1;
                has_discrete = has_discrete || col_sz == 1;
                i += col_sz;
            }
            copy_coloring_to_colmap(&c, colmap);
            PRINT(std::setw(16) << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "colorref" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);

            if(!has_deg_0 && !has_deg_1 && !has_deg_2 && !has_discrete) return;

            my_split_hook = self_split_hook();

            if (schedule != nullptr) {
                del_e = dejavu::mark_set();
                del_e.initialize(g->e_size);

                for (size_t pc = 0; pc < schedule->size(); ++pc) {
                    if (g->v_size <= 1) {
                        return;
                    }
                    preop next_op = (*schedule)[pc];
                    const int pre_v = g->v_size;
                    const int pre_e = g->e_size;
                    switch (next_op) {
                        case preop::deg01: {
                            if(!has_deg_0 && !has_deg_1 && !has_discrete && !graph_changed) break;
                            red_deg10_assume_cref(g, colmap, hook);
                            perform_del(g, colmap);
                            //PRINT("(prep-red) after 01 reduction (G, E) " << g->v_size << ", " << g->e_size);
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "deg01" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);

                            if(!(g->v_size == pre_v_size && g->e_size == pre_e_size && !color_refinement_effective)) {
                                order_according_to_color(g, colmap);
                                PRINT(std::setw(16) << std::left <<
                                                    (std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                            std::chrono::high_resolution_clock::now() -
                                                            timer).count()) / 1000000.0 << std::setw(16)
                                                    << "color_order" << std::setw(10) << g->v_size << std::setw(10)
                                                    << g->e_size);
                            }
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::deg2ma: {
                            if(!has_deg_2 && !graph_changed) break;
                            red_deg2_path_size_1(g, colmap);
                            perform_del_add_edge(g, colmap);
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "deg2ma" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::deg2ue: {
                            if(!has_deg_2 && !graph_changed) break;
                            red_deg2_unique_endpoint_new(g, colmap);
                            perform_del_add_edge(g, colmap);

                            red_deg2_trivial_connect(g, colmap);
                            perform_del_add_edge(g, colmap);

                            red_deg2_color_cycles(g, colmap);
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0 << std::setw(16) << "deg2ue" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::probe2qc: {
                            const int auto_found = sparse_ir_probe_sz2_quotient_components(g, colmap, hook, 8); //16
                            if (auto_found > 0) {
                                perform_del_discrete(g, colmap);
                            }
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "probe2qc" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::probeqc: {
                            const int auto_found = sparse_ir_probe_quotient_components(g, colmap, hook, 8, 1); // 16, 4
                            if (auto_found > 0) {
                                perform_del_discrete(g, colmap);
                            }
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "probeqc" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::probeflat: {
                            sparse_ir_probe(g, colmap, hook, SELECTOR_FIRST); // SELECTOR_LARGEST
                            perform_del_discrete(g, colmap);
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "probeflat" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            assert(_automorphism_supp.cur_pos == 0);
                            break;
                        }
                        case preop::redloop: {
                            int prev_size = g->v_size;
                            if (g->v_size > 1) {
                                count_graph_deg(g, &deg0, &deg1, &deg2);

                                int back_off = 1;
                                int add_on = 0;

                                int count_iteration = 0;

                                // continue reducing as long as we discover new degree 0 or degree 1 vertices
                                while ((deg0 > 0 || deg1 > 0) && !CONFIG_PREP_DEACT_DEG01) {
                                    // heuristics to activate / deactivate techniques
                                    // this could be more sophisticated...
                                    if (avg_end_of_comp > 0.5 &&
                                        (avg_support_sparse_ir * 1.0 / g->v_size * 1.0) > 0.1) {
                                        back_off =
                                                back_off * 16; // heuristic to stop using the more expensive techniques
                                    }
                                    if (avg_end_of_comp < 0.1 ||
                                        (avg_support_sparse_ir * 1.0 / g->v_size * 1.0) < 0.01) {
                                        add_on += 4; // heuristic to stop continue using some of the techniques
                                    }

                                    prev_size = g->v_size;

                                    red_deg10_assume_cref(g, colmap, hook);
                                    mark_discrete_for_deletion(g, colmap);
                                    perform_del(g, colmap);

                                    red_deg2_unique_endpoint_new(g, colmap);
                                    perform_del_add_edge(g, colmap);

                                    red_deg2_path_size_1(g, colmap);
                                    perform_del_add_edge(g, colmap);

                                    sparse_ir_probe_sz2_quotient_components(g, colmap, hook, 8);
                                    perform_del_discrete(g, colmap);

                                    sparse_ir_probe(g, colmap, hook, SELECTOR_LARGEST);
                                    perform_del_discrete(g, colmap);

                                    red_deg2_path_size_1(g, colmap);
                                    perform_del_add_edge(g, colmap);

                                    red_deg2_unique_endpoint_new(g, colmap);
                                    perform_del_add_edge(g, colmap);

                                    red_deg2_trivial_connect(g, colmap);
                                    perform_del_add_edge(g, colmap);

                                    red_deg2_color_cycles(g, colmap);
                                    count_graph_deg(g, &deg0, &deg1, &deg2);


                                    PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "loop" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                                    ++count_iteration;
                                    if(g->v_size <= 1)
                                        break;
                                    if (((g->v_size * 1.0) / prev_size) > 0.75) {
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case preop::qcedgeflip: {
                            red_quotient_edge_flip(g, colmap);
                            perform_del_edge(g);
                            PRINT(std::setw(16) << std::left << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - timer).count()) / 1000000.0  << std::setw(16) << "qcedgeflip" << std::setw(10) << g->v_size << std::setw(10) << g->e_size);
                            break;
                        }
                    }
                    graph_changed = graph_changed || pre_v != g->v_size || pre_e != g->e_size;
                }
            }


            //PRINT("(prep-red) after reduction (G, E) " << g->v_size << ", " << g->e_size);
            //PRINT("(prep-red) after reduction grp_sz " << base << "*10^" << exp);
            // g->sanity_check();
            // could do multiple calls for now obviously independent components -- could use "buffer consumer" to translate domains
        }

        void save_my_hook(dejavu_hook *hook) {
            saved_hook = hook;
        }

        // bliss usage specific:
#if defined(BLISS_VERSION_MAJOR) && defined(BLISS_VERSION_MINOR)
#if ( BLISS_VERSION_MAJOR >= 1 || BLISS_VERSION_MINOR >= 76 )
        void bliss_hook(unsigned int n, const unsigned int *aut) {
          auto p = preprocessor::save_preprocessor;
          p->pre_hook_buffered(n, (const int *) aut, -1, nullptr, p->saved_hook);
       }
#else
        static inline void bliss_hook(void *user_param, unsigned int n, const unsigned int *aut) {
                    auto p = (preprocessor *) user_param;
                    p->pre_hook_buffered(n, (const int *) aut, -1, nullptr, p->saved_hook);
                }
#endif
#else
        [[maybe_unused]] static inline void bliss_hook(void *user_param, unsigned int n, const unsigned int *aut) {
                auto p = (preprocessor *) user_param;
                p->pre_hook_buffered(n, (const int *) aut, -1, nullptr, p->saved_hook);
            }
#endif
        // Traces usage specific:
        [[maybe_unused]] static inline void traces_hook(int, int* aut, int n) {
            auto p = save_preprocessor;
            p->pre_hook_buffered(n, (const int *) aut, -1, nullptr, p->saved_hook);
        }

        [[maybe_unused]] void traces_save_my_preprocessor() {
            save_preprocessor = this;
        }

        // nauty usage specific:
        [[maybe_unused]] static inline void nauty_hook(int, int* aut, int*, int, int, int n) {
            auto p = save_preprocessor;
            p->pre_hook_buffered(n, (const int *) aut, -1, nullptr, p->saved_hook);
        }

        [[maybe_unused]] void nauty_save_my_preprocessor() {
            save_preprocessor = this;
        }

        // saucy usage specific:
        [[maybe_unused]] static inline int saucy_hook(int n, const int* aut, int nsupp, int* supp, void* user_param) {
            auto p = (preprocessor *) user_param;
            p->pre_hook_buffered(n, (const int *) aut, nsupp, supp, p->saved_hook);
            return true;
        }

        // dejavu usage specific: (TODO!)
        [[maybe_unused]] static inline void _dejavu_hook(int n, const int* aut, int nsupp, const int* supp) {
            auto p = save_preprocessor;
            if(p->skipped_preprocessing) {
                if(p->saved_hook != nullptr) {
                    (*p->saved_hook)(n, aut, nsupp, supp);
                }
                return;
            }
            p->pre_hook_buffered(n, (const int *) aut, nsupp, supp, p->saved_hook);
        }
    };
}
#endif //SASSY_H
