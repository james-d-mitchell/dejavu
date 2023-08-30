// Copyright 2023 Markus Anders
// This file is part of dejavu 2.0.
// See LICENSE for extended copyright information.

#ifndef DEJAVU_DEJAVU_H
#define DEJAVU_DEJAVU_H

#define DEJAVU_VERSION_MAJOR 2
#define DEJAVU_VERSION_MINOR 0

#include "dfs.h"
#include "bfs.h"
#include "rand.h"
#include "preprocess.h"
#include "inprocess.h"
#include "components.h"

// structures for testing
extern dejavu::ir::refinement test_r;
extern dejavu::sgraph dej_test_graph;
extern int*           dej_test_col;


namespace dejavu {
    static void test_hook([[maybe_unused]] int n, [[maybe_unused]] const int *p, [[maybe_unused]] int nsupp,
                   [[maybe_unused]] const int *supp) {
        assert(test_r.certify_automorphism_sparse(&dej_test_graph, p, nsupp, supp));
        //assert(test_r.certify_automorphism(&dej_test_graph, dej_test_col, p));
        //assert(test_r.certify_automorphism_sparse(&dej_test_graph, dej_test_col, p, nsupp, supp));
    }

    /**
     * \brief The dejavu solver.
     *
     * Contains the high-level strategy of the dejavu solver, controlling the interactions between different modules
     * of the solver.
     */
    class dejavu2 {
    private:
        // auxiliary
        worklist  colmap_substitute;

    public:
        // settings
        int h_error_bound       = 10; /**< error probability is below `1/2^h_error_bound`, default value of 10 thus
                                       *   misses a generator with probabiltiy at most 1/2^10 < 0.098% */
        //int h_limit_fail        = 0; /**< limit for the amount of backtracking allowed */
        int h_base_max_diff     = 5; /**< only allow a base that is at most `h_base_max_diff` times larger than the
                                       *  previous base */
        bool h_decompose = true; /**< use nonuniform component decomposition */
        //bool h_cert_original = true; /**< certify all automorphisms on the original graph, and skip non-certified */
        //int  s_cert_skip = 0; /**< how many non-certified automorphisms were skipped, please report bug if not 0  */

        // std::function<selector_hook>* h_user_selector  = nullptr; /**< user-provided cell selector */
        // std::function<selector_hook>* h_user_invariant = nullptr; /**< user-provided invariant to be applied during
        //                                                             inprocessing */

        bool h_silent = false; /**< don't print solver progress */
        int h_bfs_memory_limit = 0x20000000;

        // statistics
        bool s_deterministic_termination = true; /**< did the last run terminate deterministically? */
        big_number s_grp_sz; /**< size of the automorphism group computed in last run */

        void automorphisms(static_graph* g, dejavu_hook* hook = nullptr) {
            automorphisms(g->get_sgraph(), g->get_coloring(), hook);
        }

        /**
         * Compute the automorphisms of the graph \p g colored with vertex colors \p colmap. Automorphisms are returned
         * using the function pointer \p hook.
         *
         * @param g The graph.
         * @param colmap The vertex coloring of \p g. A null pointer is admissible as the trivial coloring.
         * @param hook The hook used for returning automorphisms. A null pointer is admissible if this is not needed.
         *
         * \sa A description of the graph format can be found in sgraph.
         */
        void automorphisms(sgraph* g, int* colmap = nullptr, dejavu_hook* hook = nullptr) {
            enum termination_strategy {t_prep, t_inproc, t_dfs, t_bfs, t_det_schreier, t_rand_schreier};
            termination_strategy s_term = t_prep;

            // want to print progress with a timer, initialize module
            timed_print m_printer;

            // no colmap provided? let's substitute a trivial vertex coloring
            if(colmap == nullptr) {
                colmap_substitute.resize(g->v_size);
                colmap = colmap_substitute.get_array();
                for(int i = 0; i < g->v_size; ++i) colmap[i] = 0;
            }

            // first, we try to preprocess
            //ir::refinement      m_refinement;          /*< workspace for color refinement and other utilities */
            sassy::preprocessor m_prep; /*< initializes the preprocessor */


            // preprocess the graph using sassy
            m_printer.print("preprocessing");
            m_prep.reduce(g, colmap, hook); /*< reduces the graph */
            s_grp_sz.multiply(m_prep.grp_sz); /*< group size needed if the
                                               *  early out below is used */

            // early-out if preprocessor finished solving the graph
            if(g->v_size <= 1) return;

            // if the preprocessor changed the vertex set of the graph, need to use reverse translation
            dejavu_hook dhook = sassy::preprocessor::_dejavu_hook;
            hook = &dhook; /*< change hook to sassy hook */

            // attempt to split into multiple quotient components than can be handled individually
            ir::graph_decomposer m_decompose;
            int s_num_components = 1;
            if(h_decompose) {
                // place to store the result of component computation
                worklist vertex_to_component(g->v_size);
                // compute the quotient components
                s_num_components = ir::quotient_components(g, colmap, &vertex_to_component);
                // make the decomposition according to the quotient components
                m_decompose.decompose(g, colmap, vertex_to_component, s_num_components);
            }

            // run the solver for each of the components separately (tends to be just one component, though)
            for(int i = 0; i < s_num_components; ++i) {
                // if we have multiple components, we need to lift the symmetry back to the original graph
                // we do so using the lifting routine of the preprocessor
                if(s_num_components > 1) {
                    g      = m_decompose.get_component(i);     // graph of current component
                    colmap = m_decompose.get_colmap(i);        // vertex coloring of current component
                    m_prep.inject_decomposer(&m_decompose, i); // set translation to current component
                }

                // print that we are solving now...
                m_printer.h_silent = h_silent || (g->v_size <= 128 && i != 0);
                if(!m_printer.h_silent)
                    PRINT("\r\nsolving_component " << i << "/" << s_num_components-1 << " (n=" << g->v_size << ")")
                m_printer.print_header();
                m_printer.timer_split();

                // flag to denote which color refinement version is used
                g->dense = !(g->e_size < g->v_size || g->e_size / g->v_size < g->v_size / (g->e_size / g->v_size));

                // settings of heuristics
                int h_budget          = 1;  /*< budget of current restart iteration    */
                int h_budget_inc_fact = 5;  /*< factor used when increasing the budget */

                // statistics used to steer heuristics
                int  s_restarts = -1;       /*< number of restarts performed */
                int  s_inproc_success = 0;  /*< how many times inprocessing succeeded  */
                int  s_cost = 0;            /*< cost induced so far by current restart iteration */
                bool s_long_base  = false;  /*< flag for bases that are considered very long  */
                bool s_short_base = false;  /*< flag for bases that are considered very short */
                bool s_prunable   = false;  /*< does this graph seem to exhibit exploitable structure? */
                double s_random_score_bias = 1.0; /*< a bias for random search, whether we've gathered that the
                                                   *  technique does not seem to work on this graph */


                bool s_few_cells  = false;
                [[maybe_unused]] bool s_many_cells = false;
                bool h_used_shallow_inprocess = false;
                bool s_inprocessed = false; /*< flag which says that we've inprocessed the graph since last restart */
                int s_consecutive_discard = 0; /*< count how many bases have been discarded in a row -- prevents edge
                                                * case where all searchable bases became much larger due to BFS or other
                                                * changes */
                bool s_last_bfs_pruned = false;/*< keep track of whether last BFS calculation pruned some nodes */
                bool s_any_bfs_pruned  = false;
                big_number s_last_tree_sz;


                // some data we want to keep track of
                std::vector<int> base_vertex;       /*< current base_vertex of vertices       */
                std::vector<int> base_sizes; /*< current size of colors on base_vertex */

                // local modules and workspace, to be used by other modules
                ir::cell_selector_factory     m_selectors;  /*< cell selector creation */
                ir::refinement        m_refinement;          /*< workspace for color refinement and other utilities */
                ds::domain_compressor m_compress;   /*< can compress a workspace of vertices to a subset of vertices */
                groups::automorphism_workspace automorphism(g->v_size); /*< workspace to keep an automorphism */
                groups::schreier_workspace     schreierw(g->v_size);    /*< workspace for Schreier-Sims */

                // shared, global modules
                ir::shared_tree sh_tree(g->v_size);    /*< BFS levels, shared leaves, ...           */
                groups::compressed_schreier sh_schreier; /*< Schreier structure to sift automorphisms */
                sh_schreier.set_error_bound(h_error_bound);

                // initialize modules for high-level search strategies
                search_strategy::dfs_ir      m_dfs;       /*< depth-first search */
                search_strategy::bfs_ir      m_bfs;       /*< breadth-first search */
                search_strategy::random_ir   m_rand;      /*< randomized search */
                search_strategy::inprocessor m_inprocess; /*< inprocessing */

                // link high-level, local strategy modules to local workspace modules
                m_dfs.link_to_workspace(&automorphism);
                m_bfs.link_to_workspace(&automorphism, &schreierw);
                m_rand.link_to_workspace(&schreierw, &automorphism);

                // initialize a coloring using colors of preprocessed graph
                coloring local_coloring;
                coloring local_coloring_left;
                g->initialize_coloring(&local_coloring, colmap);
                const bool s_regular = local_coloring.cells == 1; /*< is this graph regular? */

                // set up a local state for IR computations
                ir::controller local_state(&m_refinement,      &local_coloring); /*< controls movement in IR tree*/
                ir::controller local_state_left(&m_refinement, &local_coloring_left);

                // set deviation counter relative to graph size
                local_state.set_increase_deviation(std::min(static_cast<int>(floor(3 * sqrt(g->v_size))), 128));
                local_state.reserve(); // reserve some space

                // save root state for random and BFS search, as well as restarts
                ir::limited_save root_save;
                local_state.save_reduced_state(root_save); /*< root of the IR tree */
                int s_last_base_size = g->v_size + 1;      /*< v_size + 1 is larger than any actual base_vertex*/
                int dfs_level = -1; /*< level up to which depth-first search was performed */

                // now that we are set up, let's start solving the graph
                // loop for restarts
                while (true) {
                    // "Dry land is not a myth, I've seen it!"
                    const bool s_hard = h_budget > 256; /* graph is "hard" (was 10000)*/
                    const bool s_easy = s_restarts == -1; /* graph is "easy" */

                    ++s_restarts; /*< increase the restart counter */
                    if (s_restarts > 0) {
                        // now, we manage the restart....
                        local_state.load_reduced_state(root_save); /*< start over from root */
                        progress_print_split();
                        const int s_inc_fac = (s_restarts >= 3)? h_budget_inc_fact : 2;/*< factor of budget increase */
                        if (s_inproc_success >= 3) h_budget_inc_fact = 2;
                        if (s_inprocessed) h_budget = 1; /*< if we inprocessed, we hope that the graph is easy again */
                        h_budget *= s_inc_fac; /*< increase the budget */
                        s_cost    = 0;         /*< reset the cost */
                    }

                    // keep vertices which are save to individualize for in-processing
                    m_inprocess.inproc_can_individualize.clear();
                    m_inprocess.inproc_maybe_individualize.clear();

                    // find a selector, moves local_state to a leaf of the IR tree
                    m_selectors.make_cell_selector(g, &local_state, &local_state_left, s_restarts, h_budget);
                    auto selector = m_selectors.get_selector_hook();
                    m_printer.timer_print("sel", local_state.s_base_pos, local_state.T->get_position());
                    int base_size = local_state.s_base_pos;

                    // determine whether base_vertex is "large", or "short"
                    s_long_base  = base_size >  sqrt(g->v_size); /*< a "long"  base_vertex */
                    s_short_base = base_size <= 2;               /*< a "short" base_vertex */
                    s_few_cells  = root_save.get_coloring()->cells <= 2;               /*< "few"  cells in initial */
                    s_many_cells = root_save.get_coloring()->cells >  sqrt(g->v_size); /*< "many" cells in initial */

                    big_number s_tree_estimate = m_selectors.get_ir_size_estimate();

                    // heuristics to determine whether we want to skip this base, i.e., is this obviously worse than
                    // what we've seen before, and is there no reason to look at it?
                    const bool s_too_long = base_size > h_base_max_diff * s_last_base_size && s_inproc_success <= 1;
                    const bool s_too_big  = (s_inproc_success <= (s_regular + !s_prunable)) &&
                                            (s_last_tree_sz < s_tree_estimate) &&
                                            (s_restarts >= 2); // TODO was 3 at some point

                    // immediately discard this base if deemed too unfavourable by the heuristics above, unless we are
                    // discarding too often
                    if ((s_too_big || s_too_long) && s_consecutive_discard < 3) {
                        // TODO was s_inproc_success < 1
                        m_printer.timer_print("skip", local_state.s_base_pos, s_last_base_size);
                        ++s_consecutive_discard;
                        continue;
                    }
                    s_consecutive_discard = 0;    /*< reset counter since we are not discarding this one   */
                    s_last_base_size = base_size; /*< also reset `s_last_base_size` to current `base_size` */
                    s_last_tree_sz   = m_selectors.get_ir_size_estimate(); /*< ...same with the IR tree size estimate */
                    const bool s_last_base_eq = (base_vertex == local_state.base_vertex); /*< is this just the same
                                                                                           *  base again? */

                    // make snapshot of trace and leaf, used by following search strategies
                    local_state.compare_to_this();
                    base_vertex = local_state.base_vertex;   // we want to keep this for later
                    //base_sizes  = local_state.base_color_sz; // used for upper bounds in Schreier structure
                    base_sizes.clear();
                    base_sizes.reserve(local_state.base.size());
                    for(auto& bi : local_state.base) base_sizes.push_back(bi.color_sz); // TODO clean up

                    const int s_trace_full_cost = local_state.T->get_position(); /*< total trace cost of this base */


                    // we first perform a depth-first search, starting from the computed leaf in local_state
                    m_dfs.h_recent_cost_snapshot_limit  = s_long_base ? 0.33 : 0.25; // set up DFS heuristic
                    //m_dfs.h_recent_cost_snapshot_limit = 1.0;
                    dfs_level = s_last_base_eq?dfs_level: m_dfs.do_paired_dfs(hook, g, local_state_left, local_state,
                                                                          m_inprocess.inproc_maybe_individualize);
                    m_printer.timer_print("dfs", std::to_string(base_size) + "-" + std::to_string(dfs_level),
                                   "~" + std::to_string((int) m_dfs.s_grp_sz.mantissa) + "*10^" +
                                   std::to_string(m_dfs.s_grp_sz.exponent));
                    s_prunable = s_prunable || (dfs_level < base_size - 5);
                    if (dfs_level == 0) {
                        // dfs finished the graph -- we are done!
                        s_term = t_dfs;
                        break;
                    }
                    const bool s_dfs_backtrack =
                            m_dfs.s_termination == search_strategy::dfs_ir::termination_reason::r_fail;
                    /*< did dfs terminate because it needed to backtrack? */

                    // next, we go into the random path + BFS algorithm, so we set up a Schreier structure and IR tree
                    // for the given base_vertex

                    // first, the Schreier structure
                    const bool can_keep_previous = (s_restarts >= 3) && !s_inprocessed; /*< do we want to try to keep
                                                                                     *  previous automorphisms? */
                    // maybe we want to compress the Schreier structure
                    sh_schreier.h_min_compression_ratio = g->v_size > 1000000 ? 0.7 : 0.4; /*<if graph is large,
                                                                                             * compress aggressively */
                    m_compress.determine_compression_map(*root_save.get_coloring(), base_vertex, dfs_level);

                    // set up Schreier structure
                    const bool reset_prob = sh_schreier.reset(&m_compress, g->v_size, schreierw, base_vertex, base_sizes,
                                                               dfs_level,
                                                               can_keep_previous, m_inprocess.inproc_fixed_points);
                    if (reset_prob) sh_schreier.reset_probabilistic_criterion(); /*< need to reset probabilistic abort
                                                                                   *  criterion after inprocessing */

                    // reset metrics of random search
                    m_rand.reset_statistics();

                    // here, we set up the shared IR tree, which contains computed BFS levels, stored leaves, as well as
                    // further gathered data for heuristics
                    sh_tree.reset(base_vertex, &root_save, can_keep_previous);
                    if (s_inprocessed) sh_tree.clear_leaves();

                    s_inprocessed = false; /*< tracks whether we changed the root of IR tree since we've initialized BFS
                                            *  and Schreier last time */

                    // we add the leaf of the base_vertex to the IR tree
                    search_strategy::random_ir::specific_walk(g, sh_tree, local_state, base_vertex);

                    s_last_bfs_pruned = false;
                    s_any_bfs_pruned  = false;
                    int s_bfs_next_level_nodes = search_strategy::bfs_ir::next_level_estimate(sh_tree, selector);
                    int h_rand_fail_lim_total  = 0;
                    int h_rand_fail_lim_now    = 0;
                    double s_path_fail1_avg  = 0;

                    // in the following, we will always decide between 3 different strategies: random paths, BFS, or
                    // inprocessing followed by a restart
                    enum decision {random_ir, bfs_ir, restart}; /*< this enum tracks the decision */
                    decision h_next_routine = random_ir;        /*< here, we store the decision */
                    decision h_last_routine = random_ir;        /*< here, we store the last decision */

                    // we perform these strategies in a loop, with the following abort criteria
                    bool do_a_restart        = false; /*< we want to do a full restart */
                    bool finished_symmetries = false; /*< we found all the symmetries  */

                    h_rand_fail_lim_now = 4; /*< how many failures are allowed during random leaf search */
                    h_last_routine = restart;

                    while (!do_a_restart && !finished_symmetries) {
                        // What do we do next? Random search, BFS, or a restart?
                        // here are our decision heuristics (AKA dark magic)

                        // first, we update our estimations / statistics
                        s_bfs_next_level_nodes = search_strategy::bfs_ir::next_level_estimate(sh_tree, selector);

                        const bool s_have_rand_estimate = (m_rand.s_paths >= 4);  /*< for some estimations, we need a
                                                                                   *  few random paths */
                        double s_trace_cost1_avg = s_trace_full_cost;

                        int s_random_path_trace_cost = s_trace_full_cost - sh_tree.get_current_level_tracepos();
                        if (s_have_rand_estimate) {
                            s_path_fail1_avg  = (double) m_rand.s_paths_fail1 / (double) m_rand.s_paths;
                            s_trace_cost1_avg = (double) m_rand.s_trace_cost1 / (double) m_rand.s_paths;
                        }

                        // using this data, we now make a decision
                        h_next_routine = restart; /*< undecided? do a restart */
                        double score_rand, score_bfs;

                        if (!s_have_rand_estimate) { /*< don't have an estimate, so let's poke a bit with random! */
                            h_next_routine = random_ir; /*< need to gather more information */
                        } else {
                            // now that we have some data, we attempt to model how effective and costly random search
                            // and BFS is, to then make an informed decision of what to do next
                            const double reset_cost_rand = g->v_size;
                            const double reset_cost_bfs = std::min(s_trace_cost1_avg, (double) g->v_size);
                            double s_bfs_estimate  = (s_trace_cost1_avg + reset_cost_bfs) * (s_bfs_next_level_nodes);
                            double s_rand_estimate = (s_random_path_trace_cost + reset_cost_rand) * h_rand_fail_lim_now;

                            // we do so by negatively scoring each method: higher score, worse technique
                            score_rand = s_rand_estimate * (1 - m_rand.s_rolling_success) * s_random_score_bias;
                            score_bfs  = s_bfs_estimate  * (0.1 + 1 - s_path_fail1_avg);

                            // we make some adjustments to try to model effect of techniques better
                            // increase BFS score if BFS does not prune nodes on the next level -- we want to be
                            // somewhat more reluctant to perform BFS in this case
                            if (s_path_fail1_avg < 0.01) score_bfs *= 2;

                            // we decrease the BFS score if we are beyond the first level, in the hope that this models
                            // the effect of trace deviation maps
                            if (sh_tree.get_finished_up_to() >= 1) score_bfs *= (1 - s_path_fail1_avg);

                            // we make a decision...
                            h_next_routine = (score_rand < score_bfs) ? random_ir : bfs_ir;

                            // if we do random_ir next, increase its budget
                            h_rand_fail_lim_now =
                                    h_next_routine == random_ir ? 2 * h_rand_fail_lim_now : h_rand_fail_lim_now;
                        }

                        // we override the above decisions in specific cases...
                        // first, we consider our limits: budget, memory limits, ...
                        if (h_next_routine == bfs_ir && s_bfs_next_level_nodes * (1 - s_path_fail1_avg) > 2*h_budget)
                            h_next_routine = restart; /* best decision would be BFS, but it would exceed the budget a
                                                       * lot! */

                        // let's stick to the memory limits...
                        long s_bfs_est_mem_usage = round(s_bfs_next_level_nodes * (1 - s_path_fail1_avg) * g->v_size);
                        if (h_next_routine == bfs_ir && (s_bfs_est_mem_usage >h_bfs_memory_limit)) {
                            h_next_routine = random_ir;
                        }

                        // let's stick to the budget...
                        if (s_cost > h_budget) h_next_routine = restart; /*< we exceeded our budget, restart */

                        // ...unless...
                        if (s_dfs_backtrack && s_regular && s_few_cells && s_restarts == 0 &&
                            s_path_fail1_avg > 0.01 && sh_tree.get_finished_up_to() == 0)
                            h_next_routine = bfs_ir; /*< surely BFS will help in this case, so let's fast-track */
                        // silly case in which the base is so long, that an unnecessary restart has fairly high
                        // cost attached -- so if BFS can be successful, let's do that first...
                        if (h_next_routine == restart && 2*base_size > s_bfs_next_level_nodes
                            && s_trace_cost1_avg < base_size && s_path_fail1_avg > 0.01) h_next_routine = bfs_ir;

                        // ... and if we are "almost done" with random search, we stretch the budget a bit
                        // here are some definitions for "almost done", because I can not come up with a single one
                        if (search_strategy::random_ir::h_almost_done(sh_schreier)) h_next_routine = random_ir;
                        if (m_rand.s_rolling_success > 0.1  && s_cost <= h_budget * 4)
                            h_next_routine = random_ir;
                        if (s_hard && m_rand.s_succeed >= 1 && s_cost <= m_rand.s_succeed * h_budget * 10 &&
                            h_next_routine == restart)
                            h_next_routine = random_ir;
                        if (dfs_level == sh_tree.get_finished_up_to() && s_any_bfs_pruned && s_cost <= h_budget * 20)
                            h_next_routine = random_ir;

                        // immediately inprocess if bfs was successful in pruning the first level -- we somewhat don't
                        // want to do too much bfs, since we have only very limited automorphism pruning capability
                        if (sh_tree.get_finished_up_to() == 1 && dfs_level > 1 && s_last_bfs_pruned)
                            h_next_routine = restart;
                        if (sh_tree.get_finished_up_to() > 1 && sh_tree.get_current_level_size() < base_sizes[0])
                            h_next_routine = restart;

                        // now we've finally made up our mind of what to do next

                        // some printing...
                        if(h_last_routine == random_ir && h_next_routine != random_ir) {
                            m_printer.timer_print("random", sh_tree.stat_leaves(), m_rand.s_rolling_success);
                            if (sh_schreier.s_densegen() + sh_schreier.s_sparsegen() > 0) {
                                m_printer.timer_print("schreier", "s" + std::to_string(sh_schreier.s_sparsegen()) +
                                                                 "/d" + std::to_string(sh_schreier.s_densegen()), "_");
                            }
                        }

                        // let's perform the next routine...
                        switch (h_next_routine) {
                            case random_ir: { // random leaf search
                                // look close means we do not abort random walks that deviate from trace on the first
                                // level
                                const bool h_look_close = ((m_rand.s_rolling_first_level_success > 0.5) &&
                                                          !s_short_base) || (s_have_rand_estimate &&
                                                           sh_tree.get_finished_up_to() == base_size - 1);
                                h_rand_fail_lim_total += h_rand_fail_lim_now;
                                m_rand.use_look_close(h_look_close);
                                m_rand.h_sift_random     = !s_easy;
                                m_rand.h_randomize_up_to = dfs_level;
                                if (sh_tree.get_finished_up_to() == 0 || (s_long_base && !s_any_bfs_pruned)) {
                                    // random automorphisms, sampled from root of IR tree
                                    m_rand.random_walks(g, hook, selector, sh_tree, sh_schreier, local_state,
                                                        local_state_left, h_rand_fail_lim_total);
                                } else {
                                    // random automorphisms, sampled from current bfs level of ir tree
                                    m_rand.random_walks_from_tree(g, hook, selector, sh_tree, sh_schreier,
                                                                  local_state, local_state_left,
                                                                  h_rand_fail_lim_total);
                                }
                                finished_symmetries = sh_schreier.any_abort_criterion();
                                s_term = sh_schreier.deterministic_abort_criterion()? t_det_schreier : t_rand_schreier;
                                s_cost += h_rand_fail_lim_now;

                                // TODO this code is duplicated, let's think about this again...
                                if(finished_symmetries) {
                                    m_printer.timer_print("random", sh_tree.stat_leaves(), m_rand.s_rolling_success);
                                    if (sh_schreier.s_densegen() + sh_schreier.s_sparsegen() > 0) {
                                        m_printer.timer_print("schreier",
                                                              "s"  + std::to_string(sh_schreier.s_sparsegen()) +
                                                              "/d" + std::to_string(sh_schreier.s_densegen()), "_");
                                    }
                                }
                            }
                                break;
                            case bfs_ir: { // one level of breadth-first search
                                m_bfs.h_use_deviation_pruning = !((s_inproc_success >= 2) && s_path_fail1_avg > 0.1);
                                //m_bfs.h_use_deviation_pruning = false;
                                m_bfs.do_a_level(g, hook, sh_tree, &sh_schreier, local_state, selector);
                                m_printer.timer_print("bfs",
                                                      "0-" + std::to_string(sh_tree.get_finished_up_to()) + "(" +
                                                      std::to_string(s_bfs_next_level_nodes) + ")",
                                               std::to_string(sh_tree.get_level_size(sh_tree.get_finished_up_to())));

                                // A bit of a mess here! Manage correct group size whenever BFS finishes the graph. Bit
                                // complicated because of the special code for `base_size == 2`, which can perform
                                // automorphism pruning. (The removed automorphisms must be accounted for when
                                // calculating the group size.)
                                if (sh_tree.get_finished_up_to() == base_size) {
                                    finished_symmetries = true;
                                    s_term = t_bfs;
                                    if (base_size == 2) { /*< case for the special code */
                                        m_inprocess.s_grp_sz.multiply(
                                                (double) sh_tree.h_bfs_top_level_orbit.orbit_size(base_vertex[0]) *
                                                sh_tree.h_bfs_automorphism_pw, 0);
                                    } else if (base_size == 1) {
                                        m_inprocess.s_grp_sz.multiply(
                                                (double) sh_tree.h_bfs_top_level_orbit.orbit_size(base_vertex[0]), 0);
                                    } else { /*< base_vertex case which just multiplies the remaining elements of the level */
                                        m_inprocess.s_grp_sz.multiply(
                                                (double) sh_tree.get_level_size(sh_tree.get_finished_up_to()), 0);
                                    }
                                }

                                // if there are less remaining nodes than we expected, we pruned some nodes
                                s_last_bfs_pruned = sh_tree.get_current_level_size() < s_bfs_next_level_nodes;
                                s_any_bfs_pruned  = s_any_bfs_pruned || s_last_bfs_pruned;
                                m_rand.reset_statistics();
                                s_cost += sh_tree.get_current_level_size();
                            }
                                break;
                            case restart: // do a restart
                                do_a_restart = true;
                                break;
                        }

                        h_last_routine = h_next_routine;
                    }

                    // Are we done or just restarting?
                    if (finished_symmetries) {
                        sh_schreier.compute_group_size(); // need to compute the group size now
                        break; // we are done
                    }

                    // we are restarting -- so we try to inprocess using the gathered data
                    const bool h_use_shallow_inprocess = !h_used_shallow_inprocess && s_inproc_success == 0 &&
                                                          s_path_fail1_avg > 0.1;
                    h_used_shallow_inprocess = h_used_shallow_inprocess || h_use_shallow_inprocess;
                    s_inprocessed = m_inprocess.inprocess(g, sh_tree, sh_schreier, local_state, root_save, h_budget,
                                                          s_any_bfs_pruned, h_use_shallow_inprocess);
                    s_inproc_success += s_inprocessed;

                    if(s_inprocessed) {
                        m_printer.timer_print("inprocess", local_state.c->cells, s_inproc_success);
                    }

                    // edge case where inprocessing might finish the graph
                    if (root_save.get_coloring()->cells == g->v_size) {
                        s_term = t_inproc;
                        finished_symmetries = true;
                        break;
                    }
                }

                s_deterministic_termination = (s_term != t_rand_schreier) && s_deterministic_termination;
                big_number sh_grp_sz = sh_schreier.get_group_size();

                // We are done! Let's add up the total group size from all the different modules.
                s_grp_sz.multiply(m_inprocess.s_grp_sz);
                s_grp_sz.multiply(m_dfs.s_grp_sz);

                // if we finished with BFS, group size in Schreier is redundant since we also found them with BFS
                if(s_term != t_bfs) s_grp_sz.multiply(sh_schreier.get_group_size());
            }
            m_printer.h_silent = h_silent;
            m_printer.timer_print("done", s_deterministic_termination, s_term);
        }
    };
}

#endif //DEJAVU_DEJAVU_H
