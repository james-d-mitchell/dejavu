// Copyright 2023 Markus Anders
// This file is part of dejavu 2.0.
// See LICENSE for extended copyright information.

#include "gtest/gtest.h"
#include "../dejavu.h"

int count_auto = 0;
void count_hook(int n, const int *p, int nsupp, const int *supp) {
    ++count_auto;
}

TEST(simple_graphs_test, trivial_graphs1) {
    auto test_hook = dejavu_hook(count_hook);

    dejavu::static_graph g1;
    g1.initialize_graph(0, 0);
    dejavu::solver d;
    d.automorphisms(&g1, &test_hook);

    EXPECT_EQ(count_auto, 0);
    EXPECT_EQ(d.get_automorphism_group_size().exponent, 0);
    EXPECT_NEAR(d.get_automorphism_group_size().mantissa, 1.0, 0.001);
}

TEST(simple_graphs_test, trivial_graphs2) {
    auto test_hook = dejavu_hook(count_hook);

    dejavu::static_graph g1;
    g1.initialize_graph(1, 0);
    g1.add_vertex(0,0);
    dejavu::solver d;
    d.automorphisms(&g1, &test_hook);

    EXPECT_EQ(count_auto, 0);
    EXPECT_EQ(d.get_automorphism_group_size().exponent, 0);
    EXPECT_NEAR(d.get_automorphism_group_size().mantissa, 1.0, 0.001);
}

TEST(simple_graphs_test, trivial_graphs3) {
    auto test_hook = dejavu_hook(count_hook);

    dejavu::static_graph g1;
    g1.initialize_graph(2, 1);
    g1.add_vertex(0,1);
    g1.add_vertex(1,1);
    g1.add_edge(0, 1);
    dejavu::solver d;
    d.automorphisms(&g1, &test_hook);

    EXPECT_EQ(count_auto, 0);
    EXPECT_EQ(d.get_automorphism_group_size().exponent, 0);
    EXPECT_NEAR(d.get_automorphism_group_size().mantissa, 1.0, 0.001);
}

TEST(simple_graphs_test, trivial_graphs4) {
    auto test_hook = dejavu_hook(count_hook);

    dejavu::static_graph g1;
    g1.initialize_graph(2, 1);
    g1.add_vertex(0,1);
    g1.add_vertex(0,1);
    g1.add_edge(0, 1);
    dejavu::solver d;
    d.automorphisms(&g1, &test_hook);
    EXPECT_EQ(d.get_automorphism_group_size().exponent, 0);
    EXPECT_NEAR(d.get_automorphism_group_size().mantissa, 2.0, 0.001);
}

TEST(simple_graphs_test, trivial_graphs5) {
    auto test_hook = dejavu_hook(count_hook);

    dejavu::static_graph g1;
    g1.initialize_graph(3, 2);
    g1.add_vertex(0,1);
    g1.add_vertex(0,2);
    g1.add_vertex(0,1);
    g1.add_edge(0, 1);
    g1.add_edge(1, 2);
    dejavu::solver d;
    d.automorphisms(&g1, &test_hook);
    EXPECT_EQ(d.get_automorphism_group_size().exponent, 0);
    EXPECT_NEAR(d.get_automorphism_group_size().mantissa, 2.0, 0.001);
}