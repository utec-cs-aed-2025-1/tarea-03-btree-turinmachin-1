#ifndef BTree_H
#define BTree_H

#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "node.h"

template<typename TK>
class BTree {
    using BNode = Node<TK>;

    std::size_t M;
    BNode* root = nullptr;
    std::size_t n = 0;

    static std::size_t height(const BNode* const node) {
        std::size_t height = 0;
        const BNode* cur = node;

        while (cur != nullptr) {
            cur = cur->children[0];
            ++height;
        }

        return height;
    }

    bool check_properties(const BNode* const node) const {
        const std::size_t min_keys = node == root ? 1 : std::ceil(M / 2.0) - 1;
        const std::size_t max_keys = M - 1;

        // Check entry count
        if (node->count < min_keys || node->count > max_keys)
            return false;

        // Keys must be ordered
        for (std::size_t i = 0; i < node->count - 1; ++i) {
            if (node->keys[i] >= node->keys[i + 1])
                return false;
        }

        if (!node->leaf) {
            // Non-leaf nodes must have count + 1 children
            for (std::size_t i = 0; i < node->count + 1; ++i) {
                if (node->children[i] == nullptr)
                    return false;
            }

            // Check properties recursively
            for (std::size_t i = 0; i < node->count + 1; ++i) {
                if (!check_properties(node->children[i]))
                    return false;
            }

            const std::size_t expected_height = height(node->children[0]);

            // Subtree heights should be equal
            for (std::size_t i = 1; i < node->count + 1; ++i) {
                // NOTE: esta llamada es costosa. Podría aplicarse un caché o algo parecido, pero no
                // estamos considerando mucho el costo de check_properties.
                if (height(node->children[i]) != expected_height)
                    return false;
            }

            // Leftmost subtree should be less than first key
            if (maxKey(node->children[0]) >= node->keys[0])
                return false;

            // Rightmost subtree should be greater than last key
            if (minKey(node->children[node->count]) >= node->keys[node->count - 1])
                return false;

            // Check that subtrees go inside the correct keys
            for (std::size_t i = 1; i < node->count; ++i) {
                const TK& min = minKey(node->children[i]);
                const TK& max = maxKey(node->children[i]);

                if (node->keys[i - 1] >= min || max >= node->keys[i])
                    return false;
            }

        } else {
            // Leaf nodes should have no children
            for (std::size_t i = 0; i < node->count + 1; ++i) {
                if (node->children[i] != nullptr)
                    return false;
            }
        }

        return true;
    }

    static std::string toString(BNode* node, const std::string& sep = ",") {
        if (node == nullptr)
            return "";

        std::string result;

        for (int i = 0; i < node->count; i++) {
            result += toString(node->children[i], sep);
            result += std::to_string(node->keys[i]) + sep;
        }

        result += toString(node->children[node->count], sep);
        return result;
    }

    static void rangeSearch(BNode* const node,
                            std::vector<const TK&>& out,
                            const TK& begin,
                            const TK& end) {
        if (node == nullptr)
            return;

        for (std::size_t i = 0; i < node->count; ++i) {
            auto& key = node->keys[i];

            if (begin < key)
                rangeSearch(node->children[i], out, begin, end);

            if (key >= begin && end >= key)
                out.push(key);
        }

        rangeSearch(node->children[node->count], out, begin, end);
    }

    static const TK& minKey(const BNode* const node) {
        const BNode* cur = node;

        while (!cur->leaf)
            cur = cur->children[0];

        return cur->keys[0];
    }

    static const TK& maxKey(const BNode* const node) {
        const BNode* cur = node;

        while (!cur->leaf)
            cur = cur->children[cur->count];

        return cur->keys[cur->count - 1];
    }

    void swap(BTree& other) noexcept {
        std::swap(root, other.root);
        std::swap(n, other.n);
        std::swap(M, other.M);
    }

public:
    explicit BTree(const std::size_t M)
        : M(M) {}

    [[nodiscard]] bool search(TK key) const {
        const BNode* cur = root;

        while (cur != nullptr) {
            std::size_t idx = 0;
            while (idx < cur->count && key > cur->keys[idx])
                idx++;

            if (idx < cur->count && key == cur->keys[idx])
                return true;

            cur = cur->children[idx];
        }
        return false;
    }

    ~BTree() {
        root->killSelf();
        root = nullptr;
        n = 0;
    }

    void insert(TK key) {
        // TODO: implement
    }

    void remove(const TK& key) {
        // TODO: implement
    }

    [[nodiscard]] std::size_t height() const {
        return height(root);
    }

    [[nodiscard]] std::string toString(const std::string& sep) const {
        return toString(root, sep);
    }

    std::vector<const TK&> rangeSearch(const TK& begin, const TK& end) {
        std::vector<const TK&> out;
        rangeSearch(root, out, begin, end);
        return out;
    }

    [[nodiscard]] const TK& minKey() const {
        if (root == nullptr)
            throw std::runtime_error("BTree is empty");

        return minKey(root);
    }

    [[nodiscard]] const TK& maxKey() const {
        if (root == nullptr)
            throw std::runtime_error("BTree is empty");

        return maxKey(root);
    }

    void clear() {
        BTree(M).swap(*this);
    }

    [[nodiscard]] std::size_t size() const {
        return n;
    }

    static BTree* build_from_ordered_vector(const std::vector<TK>& elements, const std::size_t M) {
        auto* const result = new BTree(M);
        // TODO: implement
        return result;
    }

    [[nodiscard]] bool check_properties() const {
        if (root == nullptr)
            return true;

        return check_properties(root);
    }
};

#endif
