#ifndef BRUTUS_INVARIANT_H
#define BRUTUS_INVARIANT_H


#include <stack>
#include <vector>

class invariant {
    std::vector<std::vector<int>> vec_invariant;
public:
    void push_level();
    void pop_level();
    std::vector<int> top_level();
    void top_is_geq(std::vector<int> *other);
    void write_top(int i);
    void print();
};


#endif //BRUTUS_INVARIANT_H
