//
// Created by markus on 19.09.19.
//

#include "refinement.h"
#include <list>
#include <set>
#include <iostream>
#include <assert.h>
#include <algorithm>

void remove_duplicates(std::list<std::pair<int, int>> *l) {
    std::set<std::pair<int, int>> found;
    for(auto x = l->begin(); x != l->end();) {
        if (!found.insert(*x).second) {
            x = l->erase(x);
        }
        else {
            ++x;
        }
    }
}

bool compare_pair(std::pair<int, int> p1, std::pair<int, int> p2)
{
    if(p1.first < p2.first) {
        return true;
    } else if(p1.first == p2.first) {
        return (p1.second < p2.second);
    }
    return false;
}


void refinement::refine_coloring(sgraph *g, coloring *c, std::set<std::pair<int, int>> *changes, invariant* I) {
    std::cout << "Refining..." << std::endl;
    counting_array.initialize(g->v.size(), c);
    std::queue<std::pair<int, int>> worklist_color_classes;

    // initialize queue with all classes
    for(int i = 0; i < c->ptn.size(); ) {
        worklist_color_classes.push(std::pair<int, int>(i, c->ptn[i] + 1));
        i += c->ptn[i] + 1;
    }

    while(!worklist_color_classes.empty()) {
        std::list<std::pair<int, int>> color_class_splits;
        std::pair<int, int> next_color_class = worklist_color_classes.front();
        worklist_color_classes.pop();

       //std::cout << "Refining color class " << next_color_class.first << ", size: " << next_color_class.second << std::endl;
        // write color class and size to invariant
        I->write_top(-1);
        I->write_top(next_color_class.first);
        I->write_top(next_color_class.second);

        refine_color_class(g, c, next_color_class.first, next_color_class.second, &color_class_splits, I);

        // find first, largest color class
        int largest_color_class    = -1;
        int largest_color_class_sz = -1;
        for(auto cc = color_class_splits.begin(); cc != color_class_splits.end(); ++cc) {
            int new_class    = cc->second;
            int new_class_sz = c->ptn[new_class] + 1;
            if(largest_color_class_sz < new_class_sz && new_class < largest_color_class) {
                largest_color_class = new_class;
                largest_color_class_sz = new_class_sz;
            }
        }

        // add all new classes except for the last one
        //std::cout << "Adding color classes to queue..." << std::endl;

        //std::sort(color_class_splits.begin(), color_class_splits.end(), compare_pair);
        for(auto cc = color_class_splits.begin(); cc != color_class_splits.end(); ++cc) {
            changes->insert(*cc);
            int new_class    = cc->second;
            int new_class_sz = c->ptn[new_class] + 1;
            I->write_top(new_class);
            I->write_top(new_class_sz);
            if(largest_color_class != new_class) {
                //std::cout << "(" << new_class << ", " << new_class_sz << ")" << std::endl;
                worklist_color_classes.push(std::pair<int, int>(new_class, new_class_sz));
            }
        }
    }
}

void refinement::refine_color_class(sgraph *g, coloring *c, int color_class, int class_size, std::list<std::pair<int, int>> *color_class_split_worklist, invariant* I) {
    // for all vertices of the color class...
    std::set<std::pair<int, int>> color_set_worklist;
    std::set<int> vertex_worklist;
    std::set<int> new_colors;

    int cc = color_class; // iterate over color class
    while(cc < color_class + class_size) { // increment value of neighbours of vc by 1
        int vc = c->lab[cc];
        int pe = g->v[vc];
        for(int i = pe; i < pe + g->d[vc]; i++) {
            // v is a neighbour of vc
            int v = g->e[i];
            vertex_worklist.insert(v);
            counting_array.increment(v);
        }
        cc += 1;
    }

    //counting_array.write_color_degrees(I);

    // split color classes according to count in counting array
    for(auto v = vertex_worklist.begin(); v != vertex_worklist.end(); ++v) {
        int v_class_size = c->ptn[c->vertex_to_col[*v]] + 1;
        int v_old_color  = c->vertex_to_col[*v];
        int v_new_color  = v_old_color + v_class_size - counting_array.get_size(*v);

        if(v_new_color != v_old_color) {
            color_set_worklist.insert(std::pair<int, int>(*v, v_new_color));
            color_class_split_worklist->push_front(std::pair<int, int>(v_old_color, v_old_color));
            color_class_split_worklist->push_front(std::pair<int, int>(v_old_color, v_new_color));
        }
    }

    remove_duplicates(color_class_split_worklist);

    for(auto p = color_class_split_worklist->begin(); p != color_class_split_worklist->end(); ++p) {
        int v_old_color = p->first;
        int v_new_color = p->second;
        //std::cout << "Color split (" << v_old_color << ", " << v_new_color << ")" << std::endl;
        if(v_old_color != v_new_color) {
            new_colors.insert(v_new_color);
            c->ptn[v_new_color] = -1;
        }
    }

    for(auto p = color_set_worklist.begin(); p != color_set_worklist.end(); ++p) {
        int vertex = p->first;
        int color = p->second;
        int old_color = c->vertex_to_col[vertex];

        //std::cout << "set " << vertex << " to " << color << std::endl;

        int vertex_old_pos = c->vertex_to_lab[vertex];
        int vertex_at_pos = c->lab[color + c->ptn[color] + 1];
        c->lab[vertex_old_pos] = vertex_at_pos;
        c->vertex_to_lab[vertex_at_pos] = vertex_old_pos;

        c->lab[color + c->ptn[color] + 1] = vertex;
        c->vertex_to_col[vertex] = color;
        c->vertex_to_lab[vertex] = color + c->ptn[color] + 1;
        c->ptn[color] += 1;

        if (old_color != color) {
            assert(color > old_color);
            c->ptn[old_color] -= 1;
        }
    }

    for(auto new_color = new_colors.begin(); new_color != new_colors.end(); new_color++) {
        if(*new_color != 0) {
            c->ptn[*new_color - 1] = 0;
        }
    }

    counting_array.reset();
}

void refinement::individualize_vertex(sgraph *g, coloring *c, int v) {
    int color = c->vertex_to_col[v];
    int pos   = c->vertex_to_lab[v];
    int color_class_size = c->ptn[color];
    assert(color_class_size > 0);
    int new_color = color + color_class_size;

    int vertex_at_pos = c->lab[color + color_class_size];
    c->lab[pos] = vertex_at_pos;
    c->vertex_to_lab[vertex_at_pos] = pos;

    c->lab[color + color_class_size] = v;
    c->vertex_to_lab[v] = color + color_class_size;
    c->vertex_to_col[v] = color + color_class_size;

    c->ptn[color] -= 1;
    c->ptn[color + color_class_size - 1] = 0;
}

void refinement::undo_individualize_vertex(sgraph *g, coloring *c, int v) {
    int color = c->vertex_to_col[v];
    int pos   = c->vertex_to_lab[v];
    assert(color == pos);
    assert(pos > 0);
    assert(c->ptn[pos] == 0);
    int new_color = c->vertex_to_col[c->lab[pos - 1]];

    c->ptn[new_color] += 1;
    c->vertex_to_col[v] = new_color;
    if(c->ptn[pos - 1] == 0) {
        c->ptn[pos - 1] = 1;
    }
}

void refinement::undo_refine_color_class(sgraph *g, coloring *c, std::set<std::pair<int, int>> *changes) {
    std::queue<int> reset_ends;
    for(auto p = changes->begin(); p != changes->end(); ++p) {
        int old_color = c->vertex_to_col[c->lab[p->first]];
        int new_color = p->second;
        //std::cout << new_color << " back to " << old_color << std::endl;
        if(old_color != new_color && c->vertex_to_col[c->lab[new_color]] == new_color) {
            reset_ends.push(new_color - 1);
            for (int j = new_color; j <= new_color + c->ptn[new_color]; ++j) {
                c->vertex_to_col[c->lab[j]] = old_color;
                c->ptn[old_color] += 1;
            }
        }
    }

    while(!reset_ends.empty()) {
        int i = reset_ends.front();
        reset_ends.pop();
        if(i >= 0 && c->ptn[i] == 0) {
            c->ptn[i] = 1;
        }
    }
}

void cumulative_counting::write_color_degrees(invariant* I) {
    /*for(int i = 0; i < col_list_short.size(); ++i) {
        I->write_top(col_list_short[i]);
        I->write_top(sizes[i][1]);
    }*/
}

void cumulative_counting::initialize(int size, coloring *c) {
    this->c = c;
    for(int i = 0; i < size; ++i) {
        this->col_list.push_back(0);
        this->count.push_back(0);
    }
    reset();
}

void cumulative_counting::reset() {
    while(!reset_queue.empty()) {
        int index = reset_queue.front();
        reset_queue.pop();
        count[index] = 0;
    }
    this->sizes.clear();
    col_list_short.clear();
    for(int i = 0; i < c->ptn.size();) {
        assert(i==0?true:c->ptn[i - 1] == 0);
        col_list[i] = this->sizes.size();
        col_list_short.push_back(i);
        this->sizes.emplace_back(std::vector<int>());
        this->sizes[col_list[i]].push_back(c->ptn[i] + 1);
        i += c->ptn[i] + 1;
    }
}

void cumulative_counting::increment(int index) {
    if(count[index] == 0) {
        reset_queue.push(index);
    }
    count[index] += 1;
    int b_col_index = col_list[c->vertex_to_col[index]];
    if(sizes[b_col_index].size() <= count[index]) {
        sizes[b_col_index].push_back(1);
    } else {
        sizes[b_col_index][count[index]] += 1;
    }
}

const int cumulative_counting::operator[](const size_t index) {
    return count[index];
 }

int cumulative_counting::get_size(int index) {
    int b_col_index = col_list[c->vertex_to_col[index]];
    return sizes[b_col_index][count[index]];
}
