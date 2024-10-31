/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/linked_node.hpp"

#include <iostream>
#include <catch2/catch_all.hpp>
#include <fmt/base.h>

namespace {

template<class T>
std::vector<T> list_of_node_values(rav::linked_node<T>& nodes) {
    std::vector<T> values;
    for (const auto& node : nodes) {
        values.push_back(node.value());
    }
    return values;
}

template<class T>
std::vector<T*> list_of_node_pointers(T& nodes) {
    std::vector<T*> pointers;
    for (auto& node : nodes) {
        pointers.push_back(&node);
    }
    return pointers;
}

}  // namespace

TEST_CASE("linked_node | Build a list") {
    rav::linked_node n1(1);
    rav::linked_node n2(2);
    rav::linked_node n3(3);

    SECTION("Single node") {
        REQUIRE(n1.value() == 1);
        REQUIRE(n1.is_front() == false);
        REQUIRE(n1.is_back() == false);
        REQUIRE(n1.is_linked() == false);

        REQUIRE(n2.value() == 2);
        REQUIRE(n2.is_front() == false);
        REQUIRE(n2.is_back() == false);
        REQUIRE(n2.is_linked() == false);

        REQUIRE(n3.value() == 3);
        REQUIRE(n3.is_front() == false);
        REQUIRE(n3.is_back() == false);
        REQUIRE(n3.is_linked() == false);

        REQUIRE(list_of_node_values(n1) == std::vector {1});
    }

    n1.push_back(n2);

    SECTION("Two nodes") {
        REQUIRE(n1.value() == 1);
        REQUIRE(n1.is_front() == true);
        REQUIRE(n1.is_back() == false);
        REQUIRE(n1.is_linked() == true);

        REQUIRE(n2.value() == 2);
        REQUIRE(n2.is_front() == false);
        REQUIRE(n2.is_back() == true);
        REQUIRE(n2.is_linked() == true);

        REQUIRE(n3.value() == 3);
        REQUIRE(n3.is_front() == false);
        REQUIRE(n3.is_back() == false);
        REQUIRE(n3.is_linked() == false);

        REQUIRE(list_of_node_values(n1) == std::vector {1, 2});
    }

    n1.push_back(n3);

    SECTION("Three nodes") {
        REQUIRE(n1.value() == 1);
        REQUIRE(n1.is_front() == true);
        REQUIRE(n1.is_back() == false);
        REQUIRE(n1.is_linked() == true);

        REQUIRE(n2.value() == 2);
        REQUIRE(n2.is_front() == false);
        REQUIRE(n2.is_back() == false);
        REQUIRE(n2.is_linked() == true);

        REQUIRE(n3.value() == 3);
        REQUIRE(n3.is_front() == false);
        REQUIRE(n3.is_back() == true);
        REQUIRE(n3.is_linked() == true);

        REQUIRE(list_of_node_values(n1) == std::vector {1, 2, 3});
    }

    n2.unlink();

    SECTION("Two nodes again") {
        REQUIRE(n1.value() == 1);
        REQUIRE(n1.is_front() == true);
        REQUIRE(n1.is_back() == false);
        REQUIRE(n1.is_linked() == true);

        REQUIRE(n2.value() == 2);
        REQUIRE(n2.is_front() == false);
        REQUIRE(n2.is_back() == false);
        REQUIRE(n2.is_linked() == false);

        REQUIRE(n3.value() == 3);
        REQUIRE(n3.is_front() == false);
        REQUIRE(n3.is_back() == true);
        REQUIRE(n3.is_linked() == true);

        REQUIRE(list_of_node_values(n1) == std::vector {1, 3});
    }

    n1.unlink();

    SECTION("One node again") {
        REQUIRE(n1.value() == 1);
        REQUIRE(n1.is_front() == false);
        REQUIRE(n1.is_back() == false);
        REQUIRE(n1.is_linked() == false);

        REQUIRE(n2.value() == 2);
        REQUIRE(n2.is_front() == false);
        REQUIRE(n2.is_back() == false);
        REQUIRE(n2.is_linked() == false);

        REQUIRE(n3.value() == 3);
        REQUIRE(n3.is_front() == false);
        REQUIRE(n3.is_back() == false);
        REQUIRE(n3.is_linked() == false);

        REQUIRE(list_of_node_values(n1) == std::vector {1});
        REQUIRE(list_of_node_values(n2) == std::vector {2});
        REQUIRE(list_of_node_values(n3) == std::vector {3});
    }
}

TEST_CASE("linked_node | Removing nodes from front and back") {
    rav::linked_node n1(1);
    rav::linked_node n2(2);
    rav::linked_node n3(3);

    n1.push_back(n2);
    n1.push_back(n3);

    REQUIRE(list_of_node_values(n1) == std::vector {1, 2, 3});

    SECTION("From back") {
        n3.unlink();
        REQUIRE(list_of_node_values(n1) == std::vector {1, 2});
        REQUIRE(list_of_node_values(n3) == std::vector {3});

        n2.unlink();
        REQUIRE(list_of_node_values(n1) == std::vector {1});
        REQUIRE(list_of_node_values(n2) == std::vector {2});
        REQUIRE(list_of_node_values(n3) == std::vector {3});
    }

    SECTION("From front") {
        n1.unlink();
        REQUIRE(list_of_node_values(n1) == std::vector {1});
        REQUIRE(list_of_node_values(n2) == std::vector {2, 3});

        n2.unlink();
        REQUIRE(list_of_node_values(n1) == std::vector {1});
        REQUIRE(list_of_node_values(n2) == std::vector {2});
        REQUIRE(list_of_node_values(n3) == std::vector {3});
    }
}

TEST_CASE("linked_node | try to break it") {
    rav::linked_node n1(1);
    rav::linked_node n2(2);
    rav::linked_node n3(3);

    std::vector<int> nodes;

    n1.push_back(n2);
    n1.push_back(n3);

    SECTION("Adding a node twice should keep integrity") {
        n1.push_back(n2);

        for (const auto& node : n1) {
            nodes.push_back(node.value());
        }

        REQUIRE(nodes == std::vector {1, 3, 2});
    }

    SECTION("When a node goes out of scope it should remove itself") {
        {
            rav::linked_node n4(4);
            n1.push_back(n4);

            for (const auto& node : n1) {
                nodes.push_back(node.value());
            }
            REQUIRE(nodes == std::vector {1, 2, 3, 4});
        }

        nodes.clear();
        for (const auto& node : n1) {
            nodes.push_back(node.value());
        }
        REQUIRE(nodes == std::vector {1, 2, 3});
    }
}

TEST_CASE("linked_node | Assign new value") {
    rav::linked_node n1(1);
    n1 = 4;
    REQUIRE(n1.value() == 4);
}

TEST_CASE("linked_node | Move semantics") {
    rav::linked_node<std::string> n1("n1");
    rav::linked_node<std::string> n2("n2");
    rav::linked_node<std::string> n3("n3");
    n1.push_back(n2);
    n1.push_back(n3);

    REQUIRE(list_of_node_values(n1) == std::vector<std::string> {"n1", "n2", "n3"});

    SECTION("Move assignment") {
        rav::linked_node<std::string> l1("l1");
        rav::linked_node<std::string> l2("l2");
        rav::linked_node<std::string> l3("l3");
        l1.push_back(l2);
        l1.push_back(l3);

        REQUIRE(list_of_node_values(l1) == std::vector<std::string> {"l1", "l2", "l3"});

        l2 = std::move(n2);

        REQUIRE(n2.is_linked() == false);

        REQUIRE(list_of_node_values(n1) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(n2) == std::vector<std::string> {{}});
        REQUIRE(list_of_node_values(n3) == std::vector<std::string> {"n1", "n2", "n3"});

        REQUIRE(list_of_node_values(l1) == std::vector<std::string> {"l1", "l3"});
        REQUIRE(list_of_node_values(l2) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(l3) == std::vector<std::string> {"l1", "l3"});

        REQUIRE(list_of_node_pointers(n1) == std::vector<rav::linked_node<std::string>*> {&n1, &l2, &n3});
        REQUIRE(list_of_node_pointers(n2) == std::vector<rav::linked_node<std::string>*> {&n2});
        REQUIRE(list_of_node_pointers(n3) == std::vector<rav::linked_node<std::string>*> {&n1, &l2, &n3});

        REQUIRE(list_of_node_pointers(l1) == std::vector<rav::linked_node<std::string>*> {&l1, &l3});
        REQUIRE(list_of_node_pointers(l2) == std::vector<rav::linked_node<std::string>*> {&n1, &l2, &n3});
        REQUIRE(list_of_node_pointers(l3) == std::vector<rav::linked_node<std::string>*> {&l1, &l3});
    }

    SECTION("Move construction") {
        // The next operation should replace n2 with l2
        rav::linked_node new_node(std::move(n2));

        // Now new_node is linked to n1 and n3 and n2 is not linked to anything
        REQUIRE(n2.is_linked() == false);

        REQUIRE(list_of_node_values(n1) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(n2) == std::vector<std::string> {{}});
        REQUIRE(list_of_node_values(n3) == std::vector<std::string> {"n1", "n2", "n3"});

        REQUIRE(list_of_node_pointers(n1) == std::vector<rav::linked_node<std::string>*> {&n1, &new_node, &n3});
        REQUIRE(list_of_node_pointers(n2) == std::vector<rav::linked_node<std::string>*> {&n2});
        REQUIRE(list_of_node_pointers(n3) == std::vector<rav::linked_node<std::string>*> {&n1, &new_node, &n3});

        REQUIRE(list_of_node_values(new_node) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_pointers(new_node) == std::vector<rav::linked_node<std::string>*> {&n1, &new_node, &n3});
    }

    SECTION("Swap") {
        rav::linked_node<std::string> l1("l1");
        rav::linked_node<std::string> l2("l2");
        rav::linked_node<std::string> l3("l3");
        l1.push_back(l2);
        l1.push_back(l3);

        std::swap(n2, l2);

        REQUIRE(n2.value() == "l2");
        REQUIRE(l2.value() == "n2");

        REQUIRE(list_of_node_values(n1) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(l1) == std::vector<std::string> {"l1", "l2", "l3"});

        REQUIRE(list_of_node_pointers(n1) == std::vector<rav::linked_node<std::string>*> {&n1, &l2, &n3});
        REQUIRE(list_of_node_pointers(l1) == std::vector<rav::linked_node<std::string>*> {&l1, &n2, &l3});
    }
}

TEST_CASE("linked_node | Inside a container") {
    SECTION("Survive reallocation") {
        rav::linked_node<std::string> l2("n2");
        rav::linked_node<std::string> l3("n3");

        std::vector<rav::linked_node<std::string>> nodes;
        auto& it = nodes.emplace_back("n1");
        it.push_back(l2);
        it.push_back(l3);

        REQUIRE(list_of_node_values(it) == std::vector<std::string> {"n1", "n2", "n3"});

        // Now resize the vector to force re-allocation
        nodes.resize(nodes.capacity() + 1);

        REQUIRE(list_of_node_values(nodes.front()) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_pointers(nodes.front()) == std::vector<rav::linked_node<std::string>*> {&nodes.front(), &l2, &l3});
    }

    SECTION("Survive move construction") {
        std::vector<rav::linked_node<std::string>> nodes;
        nodes.emplace_back("n1");
        nodes.emplace_back("n2");
        nodes.emplace_back("n3");
        nodes.front().push_back(nodes.at(1));
        nodes.front().push_back(nodes.at(2));

        REQUIRE(list_of_node_values(nodes.at(0)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(nodes.at(1)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(nodes.at(2)) == std::vector<std::string> {"n1", "n2", "n3"});

        std::vector new_nodes(std::move(nodes));
        REQUIRE(list_of_node_values(new_nodes.at(0)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(new_nodes.at(1)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(new_nodes.at(2)) == std::vector<std::string> {"n1", "n2", "n3"});
    }

    SECTION("Survive move assignment") {
        std::vector<rav::linked_node<std::string>> nodes;
        nodes.emplace_back("n1");
        nodes.emplace_back("n2");
        nodes.emplace_back("n3");
        nodes.front().push_back(nodes.at(1));
        nodes.front().push_back(nodes.at(2));

        REQUIRE(list_of_node_values(nodes.at(0)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(nodes.at(1)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(nodes.at(2)) == std::vector<std::string> {"n1", "n2", "n3"});

        std::vector<rav::linked_node<std::string>> new_nodes;
        new_nodes = std::move(nodes);

        REQUIRE(list_of_node_values(new_nodes.at(0)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(new_nodes.at(1)) == std::vector<std::string> {"n1", "n2", "n3"});
        REQUIRE(list_of_node_values(new_nodes.at(2)) == std::vector<std::string> {"n1", "n2", "n3"});
    }
}
