// Copyright 2023 Markus Anders
// This file is part of dejavu 2.0.
// See LICENSE for extended copyright information.

#include "gtest/gtest.h"
#include "../dejavu.h"

using dejavu::groups::random_schreier;
using dejavu::hooks::schreier_hook;
using dejavu::groups::orbit;

TEST(schreier_test, schreier_construction_test) {
    random_schreier s(16);
    ASSERT_EQ(s.base_size(), 0);
    dejavu::big_number grp_sz = s.group_size();
    ASSERT_EQ(grp_sz.exponent, 0);
    ASSERT_NEAR(grp_sz.mantissa, 1.0, 0.01);

    std::vector<int> test_base;
    test_base.push_back(4);
    test_base.push_back(3);
    test_base.push_back(2);
    test_base.push_back(1);

    s.set_base(test_base);
    ASSERT_EQ(s.base_size(), 4);
    ASSERT_EQ(s.get_fixed_point(0), 4);
    ASSERT_EQ(s.get_fixed_point(1), 3);
    ASSERT_EQ(s.get_fixed_point(2), 2);
    ASSERT_EQ(s.get_fixed_point(3), 1);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 1);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 1);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 1);
    ASSERT_EQ(s.get_fixed_orbit_size(3), 1);

    ASSERT_EQ(s.get_fixed_orbit(0)[0], 4);
    ASSERT_EQ(s.get_fixed_orbit(1)[0], 3);
    ASSERT_EQ(s.get_fixed_orbit(2)[0], 2);
    ASSERT_EQ(s.get_fixed_orbit(3)[0], 1);
}

TEST(schreier_test, schreier_basic_group) {
    random_schreier s(3);
    schreier_hook hook(s);

    std::vector<int> test_base;
    test_base.push_back(0);
    test_base.push_back(1);
    test_base.push_back(2);
    s.set_base(test_base);

    dejavu::static_graph g1;
    g1.initialize_graph(3, 3);
    g1.add_vertex(0,2);
    g1.add_vertex(0,2);
    g1.add_vertex(0,2);
    g1.add_edge(0, 1);
    g1.add_edge(1, 2);
    g1.add_edge(0, 2);

    dejavu::solver d1;
    d1.automorphisms(&g1, hook.get_hook());

    dejavu::big_number grp_sz = s.group_size();
    ASSERT_EQ(grp_sz.exponent, 0);
    ASSERT_NEAR(grp_sz.mantissa, 6.0, 0.01);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 2);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 1);
}

TEST(schreier_test, schreier_orbit_at_level1) {
    random_schreier s(5);
    schreier_hook hook(s);

    std::vector<int> test_base;
    test_base.push_back(0);
    test_base.push_back(1);
    test_base.push_back(2);
    test_base.push_back(3);
    test_base.push_back(4);
    s.set_base(test_base);

    dejavu::static_graph g1;
    g1.initialize_graph(5, 0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);

    dejavu::solver d1;
    d1.automorphisms(&g1, hook.get_hook());

    dejavu::big_number grp_sz = s.group_size();
    ASSERT_EQ(grp_sz.exponent, 2);
    ASSERT_NEAR(grp_sz.mantissa, 1.2, 0.01);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 5);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 4);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(3), 2);
    ASSERT_EQ(s.get_fixed_orbit_size(4), 1);

    std::vector<int> test_orbit = s.get_fixed_orbit(2);
    for(int i = 0; i < test_orbit.size(); ++i) {
        const int v = test_orbit[i];
        ASSERT_TRUE(v == 2 || v == 3 || v ==4);
        ASSERT_FALSE(v == 0);
        ASSERT_FALSE(v == 1);
    }

    orbit o(5);
    s.get_stabilizer_orbit(2, o);
    ASSERT_TRUE(o.are_in_same_orbit(2, 3));
    ASSERT_TRUE(o.are_in_same_orbit(3, 4));
    ASSERT_FALSE(o.are_in_same_orbit(0, 2));
    ASSERT_FALSE(o.are_in_same_orbit(0, 1));
}

TEST(schreier_test, schreier_orbit_at_level2) {
    random_schreier s(8);
    schreier_hook hook(s);

    std::vector<int> test_base;
    test_base.push_back(0);
    test_base.push_back(1);
    test_base.push_back(2);
    test_base.push_back(3);
    test_base.push_back(4);
    s.set_base(test_base);

    dejavu::static_graph g1;
    g1.initialize_graph(8, 0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(1,0);
    g1.add_vertex(1,0);
    g1.add_vertex(1,0);

    dejavu::solver d1;
    d1.automorphisms(&g1, hook.get_hook());

    dejavu::big_number grp_sz = s.group_size();
    ASSERT_EQ(grp_sz.exponent, 2);
    ASSERT_NEAR(grp_sz.mantissa, 1.2, 0.01);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 5);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 4);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(3), 2);
    ASSERT_EQ(s.get_fixed_orbit_size(4), 1);

    std::vector<int> test_orbit = s.get_fixed_orbit(2);
    for(int i = 0; i < test_orbit.size(); ++i) {
        const int v = test_orbit[i];
        ASSERT_TRUE(v == 2 || v == 3 || v ==4);
        ASSERT_FALSE(v == 0);
        ASSERT_FALSE(v == 1);
        ASSERT_FALSE(v > 4);
    }

    orbit o(8);
    s.get_stabilizer_orbit(2, o);
    ASSERT_TRUE(o.are_in_same_orbit(2, 3));
    ASSERT_TRUE(o.are_in_same_orbit(3, 4));
    ASSERT_FALSE(o.are_in_same_orbit(0, 2));
    ASSERT_FALSE(o.are_in_same_orbit(0, 1));
    ASSERT_FALSE(o.are_in_same_orbit(2, 5));
    ASSERT_TRUE(o.are_in_same_orbit(5, 6));
    ASSERT_TRUE(o.are_in_same_orbit(5, 7));
}

TEST(schreier_test, schreier_base_change) {
    random_schreier s(8);
    schreier_hook hook(s);

    std::vector<int> test_base;
    test_base.push_back(0);
    test_base.push_back(1);
    test_base.push_back(2);
    s.set_base(test_base);

    dejavu::static_graph g1;
    g1.initialize_graph(8, 3);
    g1.add_vertex(0,2);
    g1.add_vertex(0,2);
    g1.add_vertex(0,2);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_vertex(0,0);
    g1.add_edge(0, 1);
    g1.add_edge(1, 2);
    g1.add_edge(0, 2);

    dejavu::solver d1;
    d1.automorphisms(&g1, hook.get_hook());

    dejavu::big_number grp_sz = s.group_size();
    ASSERT_EQ(grp_sz.exponent, 0);
    ASSERT_NEAR(grp_sz.mantissa, 6.0, 0.01);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 2);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 1);

    std::vector<int> test_base2;
    test_base2.push_back(0);
    test_base2.push_back(1);
    test_base2.push_back(2);
    test_base2.push_back(3);
    test_base2.push_back(4);
    test_base2.push_back(5);
    test_base2.push_back(6);
    s.set_base(test_base2);

    ASSERT_EQ(s.get_fixed_orbit_size(0), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(1), 2);
    ASSERT_EQ(s.get_fixed_orbit_size(2), 1);

    ASSERT_EQ(s.get_fixed_orbit_size(3), 5);
    ASSERT_EQ(s.get_fixed_orbit_size(4), 4);
    ASSERT_EQ(s.get_fixed_orbit_size(5), 3);
    ASSERT_EQ(s.get_fixed_orbit_size(6), 2);
}