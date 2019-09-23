//
// Created by markus on 23.09.19.
//

#include "auto_blaster.h"
#include <stack>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <random>
#include <chrono>
#include "ir_tools.h"
#include "trail.h"
#include "refinement.h"
#include "selector.h"
#include "invariant.h"
#include "group.h"
#include "concurrentqueue.h"
#include <pthread.h>

void auto_blaster::find_automorphism(sgraph *g, bool compare, invariant* canon_I, bijection* canon_leaf, bijection* automorphism, std::default_random_engine* re) {
    bool backtrack = false;
    std::set<std::pair<int, int>> changes;
    refinement R;
    selector S;

    // initialize a search state
    coloring c = start_c;
    trail T(g->v.size()); // a trail for backtracking and undoing refinements
    invariant I = start_I; // invariant, hopefully becomes complete in leafs such that automorphisms can be found
    //
    T.push_op_r(&changes);
    while(T.last_op() != OP_END) {
        int s;
        if(!backtrack) {
            s = S.select_color(g, &c);
            if (s == -1) {
                if(compare) {
                    std::cout << "Discrete coloring found." << std::endl;
                    I.push_level();
                    R.complete_colorclass_invariant(g, &c, &I);
                    std::cout << "Compare:" << I.top_is_eq(canon_I->get_level(I.current_level())) << std::endl;
                    // we can derive an automorphism!
                    bijection leaf;
                    leaf.read_from_coloring(&c);
                    *automorphism = leaf;
                    automorphism->inverse();
                    automorphism->compose(*canon_leaf);
                    assert(g->certify_automorphism(*automorphism));
                    return;
                } else {
                    std::cout << "Discrete coloring found." << std::endl;
                    I.push_level();
                    R.complete_colorclass_invariant(g, &c, &I);
                    //I.print();
                    canon_leaf->read_from_coloring(&c);
                    *canon_I = I;
                    return;
                }
            }
        }

        if(T.last_op() == OP_I && !backtrack) { // add new operations to trail...
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //                                                REFINEMENT
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            std::cout << "R";
            changes.clear();
            I.push_level();
            R.refine_coloring(g, &c, &changes, &I);
            //R.complete_colorclass_invariant(g, &c, &I);
            T.push_op_r(&changes);
            if(compare) {
                // compare invariant
                if(I.top_is_eq(canon_I->get_level(I.current_level()))) {
                    continue;
                } else {
                    std::cout << "Backtracking" << std::endl;
                    backtrack = true;
                    continue;
                }
            }
        } else if(T.last_op() == OP_R  && !backtrack) {
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //                                             INDIVIDUALIZATION
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // collect all elements of color s
            std::cout << "I";
            std::deque<int> color_s;
            int i = s;
            while((i == s) || (i == 0) || c.ptn[i - 1] != 0) {
                color_s.push_front(c.lab[i]);
                i += 1;
            }
            std::shuffle(color_s.begin(), color_s.end(), *re);
            int v = color_s.front();
            color_s.pop_front();
            assert(color_s.size() > 0);
            // individualize random vertex of class, save the rest of the class in trail
            R.individualize_vertex(g, &c, v);
            T.push_op_i(&color_s, v);
        } else if(T.last_op() == OP_I && backtrack) { // backtrack trail, undo operations...
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //                                       BACKTRACK INDIVIDUALIZATION
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // backtrack until we can do a new operation
            int v = T.top_op_i_v();
            R.undo_individualize_vertex(g, &c, v);
            T.pop_op_i_v();
            // undo individualization
            if(T.top_op_i_class().empty()) {
                // we tested the entire color class, need to backtrack further
                T.pop_op_i_class();
                continue;
            } else {
                // there is another vertex we have to try, so we are done backtracking
                int v = T.top_op_i_class().front();
                T.top_op_i_class().pop_front();
                T.push_op_i_v(v);
                R.individualize_vertex(g, &c, v);
                backtrack = false;
            }
        }  else if(T.last_op() == OP_R && backtrack) {
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //                                             UNDO REFINEMENT
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // we are backtracking, so we have to undo refinements
            I.pop_level();
            R.undo_refine_color_class(g, &c, &T.top_op_r());
            T.pop_op_r();
        }
    }
    assert(false);
}

void auto_blaster::sample(sgraph *g, bool master, bool* done) {
    // find comparison leaf
    invariant canon_I;
    std::thread work_thread;
    bijection canon_leaf;
    bijection trash;
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine re = std::default_random_engine(seed);

    refinement R;
    std::set<std::pair<int, int>> changes;
    if(master) {
        start_I.push_level();
        g->initialize_coloring(&start_c);
        R.refine_coloring(g, &start_c, &changes, &start_I);
    }
    find_automorphism(g, false, &canon_I, &canon_leaf, &trash, &re);
    std::cout << "Found canonical leaf." << std::endl;

    int abort_counter = 0;
    int sampled_paths = 0;
    // sample for automorphisms
    if(master) {
        // initialize automorphism group
        group G(g->v.size());

        // start threads
        //std::cout << "Launching worker..." << std::endl;
        //work_thread = std::thread(&auto_blaster::sample, this, g, false, done);

        // run algorithm
        while (abort_counter <= 10) {
            std::cout << "Sampled paths: " << sampled_paths << std::endl;
            sampled_paths += 1;
            // sample myself
            bijection automorphism;
            //std::cout << "Launching ..." << std::endl;
            find_automorphism(g, true, &canon_I, &canon_leaf, &automorphism, &re);
            bool added = G.add_permutation(&automorphism);
            if (added) {
                abort_counter = 0;
            } else {
                abort_counter += 1;
            }

            // add samples of other threads
            bool d;
            do {
                d = Q.try_dequeue(automorphism);
                if (d) {
                    //std::cout << "Considering concurrent perm..." << std::endl;
                    added = G.add_permutation(&automorphism);
                    if (added) {
                        abort_counter = 0;
                    } else {
                        abort_counter += 1;
                    }
                }
            } while(d);
        }
        *done = true;
        std::cout << "Sampled paths: " << sampled_paths << std::endl;
        std::cout << "Group size: ";
        G.print_group_size();
        //work_thread.join();
    } else {
        while(!(*done)) {
            bijection automorphism;
            find_automorphism(g, true, &canon_I, &canon_leaf, &automorphism, &re);
            Q.enqueue(automorphism);
        }
        return;
    }
}