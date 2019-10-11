#include <algorithm>
#include <assert.h>
#include <iostream>
#include <set>
#include <cstring>
#include "sgraph.h"


// initialize a coloring of this sgraph, partitioning degrees of vertices
void sgraph::initialize_coloring(coloring *c) {
    c->lab = new int[this->v_size];
    c->ptn = new int[this->v_size];
    c->lab_sz = this->v_size;
    c->ptn_sz = this->v_size;
    c->init = true;
    c->vertex_to_col.reserve(this->v_size);
   // c->vertex_to_lab.reserve(this->v_size);
    for(int i = 0; i < v_size; i++) {
        c->vertex_to_col.push_back(-1);
        //c->vertex_to_lab.push_back(-1);
        c->lab[i] = i;
        c->ptn[i] = 1;
    }

    std::sort(c->lab, c->lab + c->lab_sz, vertexComparator(*this));

    int cells = 0;
    int last_new_cell   = 0;
    for(int i = 0; i < c->lab_sz; i++) {
        c->vertex_to_col[c->lab[i]] = last_new_cell;
        //c->vertex_to_lab[c->lab[i]] = i;
        if(i + 1 == c->lab_sz) {
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

    //std::cout << "Cells: " << cells << std::endl;
}

void sgraph::initialize_coloring_bucket(coloring_bucket *c) {
    int n = this->v_size;
    std::cout << "n:" << n << std::endl;
    c->lab = new int[n];
    c->ptn = new int[n];
    c->lab_sz = n;
    c->ptn_sz = n;

    for(int i = 0; i < n; i++) {
        c->lab[i] = i;
        c->ptn[i] = n + 1;
    }
    c->ptn[n - 1] = 0;
    c->init = true;
}

// certify that a permutation is an automorphism of the sgraph
bool sgraph::certify_automorphism(bijection p) {
    assert(p.map.size() == v_size);

    for(int i = 0; i < v_size; ++i) {
        int image_i = p.map_vertex(i);
        if(d[i] != d[image_i]) // degrees must be equal
            return false;

        // automorphism must preserve neighbours
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

sgraph sgraph::permute_graph(bijection p) { // ToDo: broken
    sgraph ng;
    ng.v = v;
    ng.e = e;
    ng.d = d;

    int epos = 0;
    for(int i = 0; i < v_size; ++i) {
        int mapped_v = p.map_vertex(i);
        ng.d[i] = d[mapped_v];
        ng.v[i] = epos;
        for(int j = 0; j < d[mapped_v]; j++) {
            ng.e[epos + j] = p.map_vertex(e[v[mapped_v] + j]);
        }
        epos += ng.d[i];
    }
    assert(ng.v_size == v_size);
    assert(ng.e_size == e_size);
    assert(ng.d_size == d_size);
    assert(epos == ng.e_size);

    return ng;
}

void sgraph::copy_graph(sgraph* g) {
    v = new int[g->v_size];
    d = new int[g->d_size];
    e = new int[g->e_size];

    memcpy(v, g->v, g->v_size*sizeof(int));
    memcpy(d, g->d, g->d_size*sizeof(int));
    memcpy(e, g->e, g->e_size*sizeof(int));
    v_size = g->v_size;
    d_size = g->d_size;
    e_size = g->e_size;
}