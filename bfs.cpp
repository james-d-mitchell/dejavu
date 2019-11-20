//
// Created by markus on 18/10/2019.
//

#include "bfs.h"
#include "dejavu.h"

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
    BW.bfs_level_todo              = new moodycamel::ConcurrentQueue<std::tuple<bfs_element*, int, int>>[base_size + 2];
    BW.bfs_level_finished_elements = new moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>[base_size + 2];

    BW.domain_size = domain_size;
    BW.base_size    = base_size;
    BW.current_level = 1;
    BW.target_level  = -1;
    BW.level_states  = new bfs_element**[base_size + 2];
    BW.level_sizes   = new int[base_size + 2];
    BW.level_reserved_sizes = new int[base_size + 2];
    BW.level_maxweight = new double[base_size + 2];
    BW.level_minweight = new double[base_size + 2];
    BW.level_abort_map_done  = new int[base_size + 2];
    BW.level_abort_map_mutex = new std::mutex*[base_size + 2];
    BW.level_abort_map = new std::unordered_map<int, int>[base_size + 2];

    BW.abort_map_prune.store(0);

    BW.level_expecting_finished = new int[base_size + 2];
    for(int i = 0; i < base_size + 2; ++i) {
        BW.bfs_level_todo[i]              = moodycamel::ConcurrentQueue<std::tuple<bfs_element*, int, int>>(BW.chunk_size, 0, config.CONFIG_THREADS_REFINEMENT_WORKERS);
        BW.bfs_level_finished_elements[i] = moodycamel::ConcurrentQueue<std::pair<bfs_element*, int>>(BW.chunk_size, 0, config.CONFIG_THREADS_REFINEMENT_WORKERS);
        BW.level_expecting_finished[i] = 0;
        BW.level_maxweight[i] = 1;
        BW.level_minweight[i] = INT32_MAX;
        BW.level_abort_map[i] = std::unordered_map<int, int>();
        BW.level_abort_map_done[i] = -1;
        BW.level_abort_map_mutex[i] = new std::mutex();
    }

    BW.level_states[0]    = new bfs_element*[1];
    BW.level_states[0][0] = root_elem;
    root_elem->weight = 1;
    root_elem->target_color = init_c;

    int sz = 0;
    for(int i = init_c; i < init_c + root_elem->c->ptn[init_c] + 1; ++i) {
        int next_v = root_elem->c->lab[i];
        sz += 1;
        BW.bfs_level_todo[BW.current_level].enqueue(std::tuple<bfs_element*, int, int>(root_elem, next_v, -1));
    }

    std::cout << "Abort map expecting: " <<  root_elem->c->ptn[init_c] + 1 << std::endl;
    BW.level_abort_map_done[BW.current_level + 1] = root_elem->c->ptn[init_c] + 1;

    BW.level_expecting_finished[0] = 0;
    BW.level_sizes[0] = 1;
    BW.level_reserved_sizes[0] = 1;

    BW.level_expecting_finished[1] = sz;
    BW.level_states[1] = new bfs_element*[sz];
    BW.level_reserved_sizes[1] = sz;
    BW.level_sizes[1] = 0;

    BW.finished_elems = new std::pair<bfs_element*, int>[BW.chunk_size * config.CONFIG_THREADS_REFINEMENT_WORKERS];
    BW.finished_elems_sz = BW.chunk_size * config.CONFIG_THREADS_REFINEMENT_WORKERS;
    std::cout << "[B] BFS structure initialized, expecting " << sz << " on first level" << std::endl;
    //std::cout << "[B] BFS structure initialized" << std::endl;
    //std::cout << "[B] ToDo for level " << BW.current_level << " is " << BW.level_expecting_finished[BW.current_level] << std::endl;
}

bool bfs::work_queues(int tolerance) {
    // no work left!
    if(BW.current_level == BW.target_level) {
        if(!BW.done) {
            BW.done = true;
            std::cout << "[B] Finished BFS at " << BW.current_level - 1 << " with " << BW.level_sizes[BW.current_level - 1] << " elements, maxweight " << BW.level_maxweight[BW.current_level - 1] << "" << std::endl;
        }
        return false;
    } else {
        BW.done = false;
    }

    // dequeue and process on current level only
    size_t num = BW.bfs_level_finished_elements[BW.current_level].try_dequeue_bulk(BW.finished_elems, BW.finished_elems_sz);

    bool test = false;
    if(num == 0) {
        test = BW.bfs_level_finished_elements[BW.current_level].try_dequeue(BW.finished_elems[0]);
        if(test)
            num = 1;
    }

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
            if(elem->weight > BW.level_maxweight[lvl])
                BW.level_maxweight[lvl] = elem->weight;
            if(elem->weight < BW.level_minweight[lvl] && elem->weight >= 1)
                BW.level_minweight[lvl] = elem->weight;

            for(int j = 0; j < elem->base_sz; ++j)
                assert(elem->base[j] >= 0 && elem->base[j] < BW.domain_size);

            elem->level = lvl;
            assert(BW.level_sizes[lvl] < BW.level_reserved_sizes[lvl]);
            BW.level_sizes[lvl] += 1;
            BW.level_expecting_finished[lvl] -= 1;
            BW.level_expecting_finished[lvl + 1] += todo;
        }
    }

    bool need_queue_fill = false;

    // advance level if possible
    if (BW.level_expecting_finished[BW.current_level] == 0) {
        int expected_size = BW.level_expecting_finished[BW.current_level + 1];

        std::cout << "[B] BFS advancing to level " << BW.current_level + 1 << " expecting " << BW.level_sizes[BW.current_level] << " -> " << expected_size << ", maxweight " << BW.level_maxweight[BW.current_level] << ", minweight " << BW.level_minweight[BW.current_level] << std::endl;

        if(BW.current_level == BW.target_level - 1 && BW.target_level <= BW.base_size) {
            //if(expected_size < std::max(BW.domain_size / 100, 1)) {
            if(BW.level_sizes[BW.current_level] == 1 && BW.level_expecting_finished[BW.current_level + 1] < BW.chunk_size) { // ToDo: this should be very efficient! make it efficient! (ToDos and back-and-forth between threads are probably the culprit)
                                                        // ToDo: once levels can be made cheaply high, prefer base points in canon such that no base points have to be fixed
                                                        // ToDo: or save skipperm for BW level
                 //std::cout << "[B] Increasing target level (expected_size small), setting target level to " << BW.current_level + 1 << std::endl;
                 //BW.target_level += 1;
            }
        }

        if(expected_size < config.CONFIG_IR_SIZE_FACTOR * BW.domain_size * tolerance || config.CONFIG_IR_FULLBFS) {
            BW.level_reserved_sizes[BW.current_level + 1] = expected_size;
            BW.level_states[BW.current_level + 1] = new bfs_element * [expected_size];
            BW.level_sizes[BW.current_level + 1] = 0;

            // ToDo: insert identity first...
            /*int check_expected = 0;
            int c, c_size;
            for (int j = 0; j < BW.level_sizes[BW.current_level]; ++j) {
                bfs_element *elem = BW.level_states[BW.current_level][j];
                if (elem->weight > 0) {
                    c = elem->target_color;
                    c_size = elem->c->ptn[c] + 1;
                    for (int i = c; i < c + c_size; ++i) {
                        BW.bfs_level_todo[BW.current_level + 1].enqueue(std::tuple<bfs_element *, int, int>(elem, elem->c->lab[i], -1));
                        check_expected += 1;
                    }
                }
                if(elem->is_identity) {
                    std::cout << "Abort map expecting: " << c_size << std::endl;
                    BW.level_abort_map_done[BW.current_level + 1] = c_size;
                }
            }*/

            assert(expected_size > 0);
            //assert(check_expected > 0);
            /*if(expected_size != check_expected) {
                std::cout << "expected_size != actual_todo" << std::endl;
                assert(false);
            }*/

            need_queue_fill = true;
            BW.current_level += 1;
        } else {
            std::cout << "[B] Refusing to advance level (expected_size too large), setting target level to " << BW.current_level + 1 << std::endl;

            BW.level_reserved_sizes[BW.current_level + 1] = expected_size;
            BW.level_states[BW.current_level + 1] = new bfs_element * [expected_size]; // maybe do this only if tolerance is increased?
            BW.level_sizes[BW.current_level + 1] = 0;

            BW.target_level   = BW.current_level + 1;
            BW.reached_initial_target = false;
            BW.current_level += 1;
        }
    } else {
        //if(BW.bfs_level_todo[BW.current_level].size_approx() == 0)
        //    pthread_yield();
    }
    return need_queue_fill;
}

void bfs::reset_initial_target() {
    BW.reached_initial_target = true;
}

void bfs::write_abort_map(int level, int pos, int val) {
    BW.level_abort_map_mutex[level]->lock();
    BW.level_abort_map[level].insert(std::pair<int, int>(pos, val));
    BW.level_abort_map_done[level]--;
    BW.level_abort_map_mutex[level]->unlock();
}

bool bfs::read_abort_map(int level, int pos, int val) {
    //std::cout << BW.level_abort_map_done[level] << std::endl;
    if(BW.level_abort_map_done[level] != 0) {
       // if(BW.level_abort_map_done[level] < 0)
       //     std::cout << "bad" << BW.level_abort_map_done[level] << std::endl;
        return true;
    }
    auto check = BW.level_abort_map[level].find(pos);
    if(check == BW.level_abort_map[level].end())
        return false;

    //if(check->second == val) {
    //    std::cout << "insignificant " << val << std::endl;
   // }

    return(check->second == val);
}
