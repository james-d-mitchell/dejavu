#include <algorithm>
#include <assert.h>
#include <iostream>
#include <set>
#include "graph.h"


// initialize a coloring of this graph, partitioning degrees of vertices
void graph::initialize_coloring(coloring *c) {
    c->lab.reserve(this->v.size());
    c->ptn.reserve(this->v.size());
    c->vertex_to_col.reserve(this->v.size());
    c->vertex_to_lab.reserve(this->v.size());
    for(int i = 0; i < v.size(); i++) {
        c->vertex_to_col.push_back(-1);
        c->vertex_to_lab.push_back(-1);
        c->lab.push_back(i);
        c->ptn.push_back(1);
    }

    std::sort(c->lab.begin(), c->lab.end(), vertexComparator(*this));

    int cells = 0;
    int last_new_cell   = 0;
    for(int i = 0; i < c->lab.size(); i++) {
        c->vertex_to_col[c->lab[i]] = last_new_cell;
        c->vertex_to_lab[c->lab[i]] = i;
        if(i + 1 == c->lab.size()) {
            cells += 1;
            c->ptn[last_new_cell] = i - last_new_cell;
            c->ptn[i] = 0;
            break;
        }
        assert(this->d[c->lab[i]] <= this->d[c->lab[i + 1]]);
        if(this->d[c->lab[i]] < this->d[c->lab[i + 1]]) {
            c->ptn[i] = 0;
            cells += 1;
            c->ptn[last_new_cell] = i - last_new_cell;
            last_new_cell = i + 1;
            continue;
        }
    }

    std::cout << "Cells: " << cells << std::endl;
}

// certify that a permutation is an isomorphism of the graph
bool graph::certify_isomorphism(bijection p) {
    assert(p.map.size() == v.size());

    for(int i = 0; i < v.size(); ++i) {
        int image_i = p.map_vertex(i);
        if(d[i] != d[image_i]) // degrees must be equal
            return false;

        // isomorphism must preserve neighbours
        std::set<int> image_neighbours_of_i;
        for(int j = v[i]; j < v[i] + d[i]; ++j) {
            int vertex_j = e[j];
            int image_j  = p.map_vertex(vertex_j);
            image_neighbours_of_i.insert(image_j);
        }
        for(int j = v[image_i]; j < v[image_i] + d[image_i]; ++j) {
            int vertex_j = e[j];
            if(image_neighbours_of_i.find(vertex_j) == image_neighbours_of_i.end()) {
                return false;
            }
            image_neighbours_of_i.erase(image_neighbours_of_i.find(vertex_j));
        }
        if(!image_neighbours_of_i.empty()) {
            return false;
        }
    }

    return true;
}
