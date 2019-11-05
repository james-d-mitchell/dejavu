//
// Created by markus on 18/10/2019.
//

#include "bfs.h"
#include "auto_blaster.h"

bfs_element::~bfs_element() {
    if(init_c)
        delete c;
    if(init_I)
        delete I;
    if(init_base)
        delete base;
}

bfs::bfs() {

}

void bfs::initialize(bfs_element* root_elem, int init_c, int domain_size, int base_size) {
    BW.bfs_level_todo              = new moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>[base_size + 2];
    BW.bfs_level_finished_elements = new moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>[base_size + 2];

    BW.domain_size = domain_size;
    BW.base_size    = base_size;
    BW.current_level = 1;
    BW.target_level  = -1;
    BW.level_states  = new bfs_element**[base_size + 2];
    BW.level_sizes   = new int[base_size + 2];
    BW.level_expecting_finished = new int[base_size + 2];
    for(int i = 0; i < base_size + 2; ++i) {
        BW.bfs_level_todo[i]              = moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>(BW.chunk_size, 0, config.CONFIG_THREADS_REFINEMENT_WORKERS);
        BW.bfs_level_finished_elements[i] = moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>(BW.chunk_size, 0, config.CONFIG_THREADS_REFINEMENT_WORKERS);
        BW.level_expecting_finished[i] = 0;
    }

    BW.level_states[0]    = new bfs_element*[1];
    BW.level_states[0][0] = root_elem;

    int sz = 0;
    for(int i = init_c; i < init_c + root_elem->c->ptn[init_c] + 1; ++i) {
        int next_v = root_elem->c->lab[i];
        sz += 1;
        BW.bfs_level_todo[BW.current_level].enqueue(std::pair<bfs_element*, int>(root_elem, next_v));
    }

    BW.level_expecting_finished[0] = 0;
    BW.level_sizes[0] = 1;
    BW.level_expecting_finished[1] = sz;
    BW.level_states[1] = new bfs_element*[sz];
    BW.level_sizes[1] = 0;

    BW.finished_elems = new std::pair<bfs_element*, int>[BW.chunk_size * config.CONFIG_THREADS_REFINEMENT_WORKERS];
    BW.finished_elems_sz = BW.chunk_size * config.CONFIG_THREADS_REFINEMENT_WORKERS;
    std::cout << "[B] BFS structure initialized, expecting " << sz << " on first level" << std::endl;
    //std::cout << "[B] BFS structure initialized" << std::endl;
    //std::cout << "[B] ToDo for level " << BW.current_level << " is " << BW.level_expecting_finished[BW.current_level] << std::endl;
}

void bfs::work_queues() {
    // no work left!
    if(BW.current_level == BW.target_level) {
        if(!BW.done) {
            BW.done = true;
            std::cout << "[B] Finished BFS at " << BW.current_level - 1 << " with " << BW.level_sizes[BW.current_level - 1] << " elements." << std::endl;
        }
        return;
    }

    // dequeue and process on current level only
    size_t num = BW.bfs_level_finished_elements[BW.current_level].try_dequeue_bulk(BW.finished_elems, BW.finished_elems_sz);

    //if(num > 0) std::cout << "Chunk " << num << std::endl;
    for(int i = 0; i < num; ++i) {
        bfs_element *elem = BW.finished_elems[i].first;
        int todo = BW.finished_elems[i].second;
        int lvl = BW.current_level;
        // std::cout << "[B] Received level " << lvl << " elem " << elem << " todo " << todo << std::endl;

        if (elem == nullptr) {
            BW.level_expecting_finished[lvl] -= todo;
        } else {
            BW.level_states[lvl][BW.level_sizes[lvl]] = elem;
            elem->id = BW.level_sizes[lvl];
            BW.level_sizes[lvl] += 1;
            BW.level_expecting_finished[lvl] -= 1;
            BW.level_expecting_finished[lvl + 1] += todo;
        }
    }

    // advance level if possible
    if (BW.level_expecting_finished[BW.current_level] == 0) {
        int expected_size = BW.level_expecting_finished[BW.current_level +1];

        std::cout << "[B] BFS advancing to level " << BW.current_level + 1 << " expecting " << expected_size << std::endl;

        if(BW.current_level == BW.target_level - 1 && BW.target_level <= BW.base_size) {
            //if(expected_size < std::max(BW.domain_size / 100, 1)) {
            if(BW.level_sizes[BW.current_level] == 1 && BW.level_expecting_finished[BW.current_level + 1] < BW.chunk_size) { // ToDo: this should be very efficient! make it efficient! (ToDos and back-and-forth between threads are probably the culprit)
                                                        // ToDo: once levels can be made cheaply high, prefer base points in canon such that no base points have to be fixed
                                                        // ToDo: or save skipperm for BW level
                 //std::cout << "[B] Increasing target level (expected_size small), setting target level to " << BW.current_level + 1 << std::endl;
                 //BW.target_level += 1;
            }
        }

        if(expected_size < 1000 * BW.domain_size) {
            BW.level_states[BW.current_level + 1] = new bfs_element * [expected_size];
            BW.level_sizes[BW.current_level + 1] = 0;
            BW.current_level += 1;
        } else {
            std::cout << "[B] Refusing to advance level (expected_size too large), setting target level to " << BW.current_level + 1 << std::endl;
            BW.target_level   = BW.current_level + 1;
            BW.current_level += 1;
        }
    }
}