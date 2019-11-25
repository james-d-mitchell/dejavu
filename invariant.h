#ifndef DEJAVU_INVARIANT_H
#define DEJAVU_INVARIANT_H


#include <stack>
#include <vector>
#include "assert.h"

class alignas(64) invariant {
public:
    std::vector<int>* vec_invariant = nullptr;
    invariant*        compareI;
    std::vector<int>* compare_vec;
    bool has_compare = false;
    bool no_write = false;
    bool comp = true;
    bool never_fail = false;
    int comp_fail_pos = -2;
    int comp_fail_val = -1;
    int comp_fail_acc = -1;
    int cur_pos = -1;
    int acc = 0;

    // currently a bit convoluted, really should be split into 2 functions...
    inline bool write_top_and_compare(int i) {
        acc += i * (35235237 - i * 5);
        if(no_write) {
            if(comp_fail_pos == -2)
                comp = ((*compare_vec)[++cur_pos] == i);
            else
                comp = false;
            if(!comp) {
                if(comp_fail_pos == -2) {
                    comp_fail_pos = cur_pos;
                    comp_fail_val = i;
                    comp_fail_acc = i;
                } else {
                    comp_fail_acc += i * (35235235 - i * 3); // could just use acc instead
                }
            }
            return (comp || never_fail);
        } else {
            vec_invariant->push_back(i);
            cur_pos += 1;
           // assert(cur_pos == vec_invariant.size() - 1);
            if (has_compare) {
                if ((compareI->vec_invariant)->size() < vec_invariant->size())
                    return false;
                return (*vec_invariant)[cur_pos] == (*compareI->vec_invariant)[cur_pos];
            } else {
                return true;
            }
        }
    }

    void reset_deviation() {
        comp_fail_pos = -2;
        comp_fail_val = -1;
        comp_fail_acc = -1;
    }

    void set_compare_invariant(invariant *I);

    void create_vector() {
        vec_invariant = new std::vector<int>();
    }

    /*~invariant() {
        if(vec_invariant != nullptr)
            delete vec_invariant;
    }*/
};


#endif //DEJAVU_INVARIANT_H
