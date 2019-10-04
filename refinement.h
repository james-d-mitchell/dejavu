//
// Created by markus on 19.09.19.
//

#ifndef BRUTUS_REFINEMENT_H
#define BRUTUS_REFINEMENT_H


#include <queue>
#include <set>
#include "coloring.h"
#include "sgraph.h"
#include "invariant_acc.h"
#include <list>

class cumulative_counting {
public:
    void initialize(int size, coloring *c);
    void reset();
    void increment(int index);
    int get_size(int index);
    int get_count(int index);
    void set_coloring(coloring *c);
private:
    coloring* c;
    std::vector<int> count;
    std::vector<std::vector<int>> sizes;
    std::queue<int>  reset_queue;
    std::queue<int>  reset_queue_sizes;
};

class work_set {
public:
    void initialize(int size);
    void set(int index);
    bool get(int index);
    void reset();
private:
    std::queue<int>  reset_queue;
    std::vector<bool> s;
};

class work_list {
public:
    void initialize(int size);
    void push_back(int index);
    int pop_back();
    bool empty();
    void reset();
    ~work_list();
private:
    int* arr;
    int arr_sz = -1;
    int cur_pos;
};


class refinement {
public:
    bool refine_coloring(sgraph* g, coloring* c, std::list<std::pair<int, int>> *changes, invariant* I, std::list<int>* init_color_class, bool track_changes);
    void individualize_vertex(sgraph* g, coloring* c, int v);
    void undo_individualize_vertex(sgraph *g, coloring *c, int v);
    bool refine_color_class(sgraph *g, coloring *c, int color_class, int class_size, std::list<std::pair<int, int>> *color_class_split_worklist, invariant* I, int* largest_color_class_index);
    void undo_refine_color_class(sgraph *g, coloring *c, std::list<std::pair<int, int>> *changes);
    void complete_colorclass_invariant(sgraph *g, coloring *c, invariant_acc *I);
    bool assert_is_equitable(sgraph *g, coloring *c);
    ~refinement();
private:
    bool initialized = false;
    cumulative_counting counting_array;
    work_set vertex_workset;
    work_set color_worklset;
    work_list color_worklist_vertex;
    work_list color_worklist_color;
    work_list vertex_worklist;
    int* largest_color_class_index;
};


#endif //BRUTUS_REFINEMENT_H
