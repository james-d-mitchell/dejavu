#ifndef SASSY_GRAPH_BUILDER_H
#define SASSY_GRAPH_BUILDER_H

#include <fstream>

namespace dejavu {
    /**
     * \brief Internal graph data structure
     *
     * Graph data strcture as used internally by the dejavu solver. See \
     */
    class sgraph {
        struct vertexComparator {
            vertexComparator(const sgraph &g) : g(g) {}

            const sgraph &g;

            bool operator()(const int &v1, const int &v2) const {
                return g.d[v1] < g.d[v2];
            }
        };

        struct vertexComparatorColor {
            vertexComparatorColor(const sgraph &g, const int *vertex_to_col) : g(g), vertex_to_col(vertex_to_col) {}

            const sgraph &g;
            const int *vertex_to_col;

            bool operator()(const int &v1, const int &v2) const {
                //return (g.d[v1] < g.d[v2]) || ((g.d[v1] == g.d[v2]) && (vertex_to_col[v1] < vertex_to_col[v2]));
                return (vertex_to_col[v1] < vertex_to_col[v2]);
            }
        };

    public:
        bool initialized = false;
        int *v = nullptr;
        int *d = nullptr;
        int *e = nullptr;

        int v_size = 0;
        int e_size = 0;

        bool dense = false;

        void initialize(int nv, int ne) {
            initialized = true;
            v = new int[nv];
            d = new int[nv];
            e = new int[ne];
        }

        // initialize a coloring of this sgraph, partitioning degrees of vertices
        void initialize_coloring(ds::coloring *c, int *vertex_to_col) {
            c->initialize(this->v_size);
            std::memset(c->ptn, 1, sizeof(int) * v_size);

            if (this->v_size < c->domain_size) {
                c->domain_size = this->v_size;
            }

            if (v_size == 0)
                return;

            int cells = 0;
            int last_new_cell = 0;

            if (vertex_to_col == nullptr) {
                for (int i = 0; i < v_size; i++) {
                    c->lab[i] = i;
                }
                std::sort(c->lab, c->lab + c->domain_size, vertexComparator(*this));
                for (int i = 0; i < c->domain_size; i++) {
                    c->vertex_to_col[c->lab[i]] = last_new_cell;
                    c->vertex_to_lab[c->lab[i]] = i;
                    if (i + 1 == c->domain_size) {
                        cells += 1;
                        c->ptn[last_new_cell] = i - last_new_cell;
                        c->ptn[i] = 0;
                        break;
                    }
                    assert(this->d[c->lab[i]] <= this->d[c->lab[i + 1]]);
                    if (this->d[c->lab[i]] != this->d[c->lab[i + 1]]) {
                        c->ptn[i] = 0;
                        cells += 1;
                        c->ptn[last_new_cell] = i - last_new_cell;
                        last_new_cell = i + 1;
                        continue;
                    }
                }
            } else {
                int col = 0;

                int min_col = INT32_MAX;
                int max_col = INT32_MIN;
                for (int i = 0; i < v_size; i++) {
                    if (vertex_to_col[i] < min_col)
                        min_col = vertex_to_col[i];
                    if (vertex_to_col[i] > max_col)
                        max_col = vertex_to_col[i];
                }

                std::vector<int> colsize;
                colsize.reserve(std::min(this->v_size, (max_col - min_col) + 1));

                if (min_col < 0 || max_col > 4 * this->v_size) {
                    std::unordered_map<int, int> colors; // TODO: should not use unordered_map!
                    colors.reserve(this->v_size);
                    for (int i = 0; i < v_size; i++) {
                        auto it = colors.find(vertex_to_col[i]);
                        if (it == colors.end()) {
                            colors.insert(std::pair<int, int>(vertex_to_col[i], col));
                            colsize.push_back(1);
                            assert(col < this->v_size);
                            vertex_to_col[i] = col;
                            ++col;
                        } else {
                            const int found_col = it->second;
                            assert(found_col < this->v_size);
                            vertex_to_col[i] = found_col;
                            ++colsize[found_col];
                        }
                    }
                } else {
                    std::vector<int> colors;
                    colors.reserve(max_col + 1);
                    for (int i = 0; i < max_col + 1; ++i)
                        colors.push_back(-1);
                    for (int i = 0; i < v_size; i++) {
                        if (colors[vertex_to_col[i]] == -1) {
                            colors[vertex_to_col[i]] = col;
                            colsize.push_back(1);
                            assert(col < this->v_size);
                            vertex_to_col[i] = col;
                            ++col;
                        } else {
                            const int found_col = colors[vertex_to_col[i]];
                            assert(found_col < this->v_size);
                            vertex_to_col[i] = found_col;
                            ++colsize[found_col];
                        }
                    }
                }

                int increment = 0;
                for (int &i: colsize) {
                    const int col_sz = i;
                    i += increment;
                    increment += col_sz;
                }
                assert(increment == v_size);

                for (int i = 0; i < v_size; i++) {
                    const int v_col = vertex_to_col[i];
                    --colsize[v_col];
                    const int v_lab_pos = colsize[v_col];
                    c->lab[v_lab_pos] = i;
                }

                /*for(int i = 0; i < v_size; i++) {
                    c->lab[i] = i;
                }
                std::sort(c->lab, c->lab + c->lab_sz, vertexComparatorColor(*this, vertex_to_col));*/
                for (int i = 0; i < c->domain_size; i++) {
                    c->vertex_to_col[c->lab[i]] = last_new_cell;
                    c->vertex_to_lab[c->lab[i]] = i;
                    if (i + 1 == c->domain_size) {
                        cells += 1;
                        c->ptn[last_new_cell] = i - last_new_cell;
                        c->ptn[i] = 0;
                        break;
                    }
                    //assert(this->d[c->lab[i]] <= this->d[c->lab[i + 1]]);
                    //if(this->d[c->lab[i]] < this->d[c->lab[i + 1]]  || (this->d[c->lab[i]] == this->d[c->lab[i + 1]]
                    //&& (vertex_to_col[c->lab[i]] < vertex_to_col[c->lab[i + 1]]))) {
                    if (vertex_to_col[c->lab[i]] != vertex_to_col[c->lab[i + 1]]) {
                        c->ptn[i] = 0;
                        cells += 1;
                        c->ptn[last_new_cell] = i - last_new_cell;
                        last_new_cell = i + 1;
                        continue;
                    }
                }
            }

            c->cells = cells;
        }

        [[maybe_unused]] void sanity_check() {
#ifndef NDEBUG
            for(int i = 0; i < v_size; ++i) {
                assert(d[i]>0?v[i] < e_size:true);
                assert(d[i]>0?v[i] >= 0:true);
                assert(d[i] >= 0);
                assert(d[i] < v_size);
            }
            for(int i = 0; i < e_size; ++i) {
                assert(e[i] < v_size);
                assert(e[i] >= 0);
            }

            // multiedge test
            dejavu::ds::markset multiedge_test;
            multiedge_test.initialize(v_size);
            for(int i = 0; i < v_size; ++i) {
                multiedge_test.reset();
                for(int j = 0; j < d[i]; ++j) {
                    const int neigh = e[v[i] + j];
                    assert(!multiedge_test.get(neigh));
                    multiedge_test.set(neigh);
                }
            }

            // fwd - bwd test
            multiedge_test.initialize(v_size);
            for(int i = 0; i < v_size; ++i) {
                multiedge_test.reset();
                for(int j = 0; j < d[i]; ++j) {
                    const int neigh = e[v[i] + j];
                    assert(neigh >= 0);
                    assert(neigh < v_size);
                    bool found = false;
                    for(int k = 0; k < d[neigh]; ++k) {
                        const int neigh_neigh = e[v[neigh] + k];
                        if(neigh_neigh == i) {
                            found = true;
                            break;
                        }
                    }
                    assert(found);
                }
            }
#endif
        }

        void copy_graph(sgraph *g) {
            if (initialized) {
                delete[] v;
                delete[] d;
                delete[] e;
            }
            initialize(g->v_size, g->e_size);

            memcpy(v, g->v, g->v_size * sizeof(int));
            memcpy(d, g->d, g->v_size * sizeof(int));
            memcpy(e, g->e, g->e_size * sizeof(int));
            v_size = g->v_size;
            e_size = g->e_size;
        }

        [[maybe_unused]] void sort_edgelist() const {
            for (int i = 0; i < v_size; ++i) {
                const int estart = v[i];
                const int eend = estart + d[i];
                std::sort(e + estart, e + eend);
            }
        }

        ~sgraph() {
            if (initialized) {
                delete[] v;
                delete[] d;
                delete[] e;
            }
        }
    };

    /**
     * \brief Graph with static number of vertices and edges
     *
     * Graph format based on the internal format of dejavu (`sgraph`), but adding sanity checks and easy access to the
     * construction. Essentially, this class provides a more convenient interface to construct `sgraph`s.
     *
     * The graph must first be initialized (either using the respective constructor or using initialize_graph). For the
     * initialization, the final number of vertices or edges must be given. The number of vertices or edges can not be
     * changed. Then, using add_vertex and add_edge, the precise number of defined vertices and edges must be added.
     * The `add_vertex(color, deg)` function requires a color and a degree. Both can not be changed later.
     *
     * The `add_edge(v1, v2)` function adds an undirected edge from `v1` to `v2`. It is always required that `v1 < v2`
     * holds, to prevent the accidental addition of hyper-edges.
     *
     * After the graph was built, the internal sassy graph (sgraph) can be accessed either by the user, or the provided
     * functions. Once the graph construction is finished, the internal sgraph can be changed arbitrarily.
     */
    class static_graph {
    private:
        sgraph   g;
        int*     c        = nullptr;
        int*     edge_cnt = nullptr;
        unsigned int num_vertices_defined  = 0;
        unsigned int num_edges_defined     = 0;
        unsigned int num_deg_edges_defined = 0;
        bool initialized;
        bool finalized = false;

    private:
        void finalize() {
            if(!finalized) {
                if (!initialized)
                    throw std::logic_error("uninitialized graph");
                if (num_vertices_defined != (unsigned int) g.v_size)
                    throw std::logic_error("did not add the number of vertices requested by constructor");
                if (num_edges_defined != (unsigned int) g.e_size) {
                    std::cout << num_edges_defined << " vs. " << g.e_size << std::endl;
                    throw std::logic_error("did not add the number of edges requested by constructor");
                }
                sanity_check();
                finalized = true;
            }
        }
    public:
        [[maybe_unused]] static_graph(const int nv, const int ne) {
            if(nv <= 0) throw std::out_of_range("number of vertices must be positive");
            if(ne <= 0) throw std::out_of_range("number of edges must be positive");
            g.initialize(nv, 2*ne);
            g.v_size = nv;
            g.e_size = 2*ne;
            c = new int[nv];
            edge_cnt = new int[nv];
            for(int i = 0; i < nv; ++i) edge_cnt[i] = 0;
            initialized = true;
        };

        static_graph() {
            initialized = false;
        }

        ~static_graph() {
            if(initialized && c != nullptr)
                delete[] c;
            if(initialized && edge_cnt != nullptr)
                delete[] edge_cnt;
        }

        [[maybe_unused]] void initialize_graph(const unsigned int nv, const unsigned int ne) {
            if(initialized || finalized)
                throw std::logic_error("can not initialize a graph that is already initialized");
            initialized = true;
            g.initialize((int) nv, (int) (2*ne));
            g.v_size = (int) nv;
            g.e_size = (int) (2*ne);
            c = new int[nv];
            edge_cnt = new int[nv];
            for(unsigned int i = 0; i < nv; ++i)
                edge_cnt[i] = 0;
        };

        [[maybe_unused]] unsigned int add_vertex(const int color, const int deg) {
            if(!initialized)
                throw std::logic_error("uninitialized graph");
            if(finalized)
                throw std::logic_error("can not change finalized graph");
            const unsigned int vertex = num_vertices_defined;
            ++num_vertices_defined;
            if(num_vertices_defined > (unsigned int) g.v_size)
                throw std::out_of_range("vertices out-of-range, define more vertices initially");
            c[vertex]   = color;
            g.d[vertex] = deg;
            g.v[vertex] = (int) num_deg_edges_defined;
            num_deg_edges_defined += deg;
            return vertex;
        };

        [[maybe_unused]] void add_edge(const unsigned int v1, const unsigned int v2) {
            if(!initialized)
                throw std::logic_error("uninitialized graph");
            if(finalized)
                throw std::logic_error("can not change finalized graph");
            if(v1 > v2 || v1 == v2)
                throw std::invalid_argument("invalid edge: v1 < v2 must hold");
            if(v1 >= num_vertices_defined)
                throw std::out_of_range("v1 is not a defined vertex, use add_vertex to add vertices");
            if(v2 >= num_vertices_defined)
                throw std::out_of_range("v2 is not a defined vertex, use add_vertex to add vertices");
            if(static_cast<int>(num_edges_defined + 2) > g.e_size)
                throw std::out_of_range("too many edges");
            if(v1 > INT32_MAX)
                throw std::out_of_range("v1 too large, must be < INT32_MAX");
            if(v2 > INT32_MAX)
                throw std::out_of_range("v2 too large, must be < INT32_MAX");
            ++edge_cnt[v1];
            const int edg_cnt1 = edge_cnt[v1];
            if(edg_cnt1 > g.d[v1])
                throw std::out_of_range("too many edges incident to v1");
            g.e[g.v[v1] + edg_cnt1 - 1] = static_cast<int>(v2);

            ++edge_cnt[v2];
            const int edg_cnt2 = edge_cnt[v2];
            if(edg_cnt2 > g.d[v2])
                throw std::out_of_range("too many edges incident to v2");
            g.e[g.v[v2] + edg_cnt2 - 1] = static_cast<int>(v1);

            num_edges_defined += 2;
        };

        void sanity_check() {
            g.sanity_check();
        }

        [[maybe_unused]] void dump_dimacs(const std::string& filename) {
            finalize();
            std::ofstream dumpfile;
            dumpfile.open (filename, std::ios::out);

            dumpfile << "p edge " << g.v_size << " " << g.e_size/2 << std::endl;

            for(int i = 0; i < g.v_size; ++i) {
                dumpfile << "n " << i+1 << " " << c[i] << std::endl;
            }

            for(int i = 0; i < g.v_size; ++i) {
                for(int j = g.v[i]; j < g.v[i]+g.d[i]; ++j) {
                    const int neighbour = g.e[j];
                    if(neighbour < i) {
                        dumpfile << "e " << neighbour+1 << " " << i+1 << std::endl;
                    }
                }
            }
        }

        dejavu::sgraph* get_sgraph() {
            finalize();
            return &g;
        };

        int* get_coloring() {
            finalize();
            return c;
        };
    };
}

#endif //SASSY_GRAPH_BUILDER_H
