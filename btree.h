#ifndef BTree_H
#define BTree_H

#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "node.h"

template<typename TK>
class BTree {
    using BNode = Node<TK>;

    std::size_t M;
    BNode* root = nullptr;
    std::size_t n = 0;

    static std::ptrdiff_t height(const BNode* const node) {
        std::ptrdiff_t height = -1;
        const BNode* cur = node;

        while (cur != nullptr) {
            ++height;
            cur = cur->children[0];
        }

        return height;
    }

    bool check_properties(const BNode* const node) const {
        if (node == nullptr)
            return true;

        const std::size_t min_keys = node == root ? 1 : std::ceil(M / 2.0) - 1;
        const std::size_t max_keys = M - 1;

        // Check key count
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

            // Check that subtrees go inside the correct keys
            for (std::size_t i = 0; i < node->count; ++i) {
                const TK& min = maxKey(node->children[i]);
                const TK& max = minKey(node->children[i + 1]);

                if (node->keys[i] < min || node->keys[i] >= max)
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

        for (std::size_t i = 0; i < node->count; ++i) {
            if (!node->leaf)
                result += toString(node->children[i], sep) + sep;

            result += std::to_string(node->keys[i]);

            if (i < node->count - 1)
                result += sep;
        }

        if (!node->leaf)
            result += sep + toString(node->children[node->count], sep);

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

    // Returns:
    // - bool if the result didn't split. Indicates "inserted".
    // - (TK, Node*) if there was a split (implies inserted = true)
    std::variant<bool, std::pair<TK, BNode*>> insert(BNode* const node, TK key) {
        // Los comentarios en esta función son prueba de mi travesía intentando insertar en un
        // BTree...
        //
        // (programar pensando en inglés es peak)

        if (node == nullptr)
            return std::pair{key, nullptr};  // We went past a leaf. Gotta insert back up...

        std::size_t i = 0;
        while (i < node->count && node->keys[i] < key)
            ++i;

        if (i < node->count && node->keys[i] == key)
            return false;  // Key already exists

        // Insert between the correct keys
        auto result = insert(node->children[i], std::move(key));

        if (std::holds_alternative<bool>(result))
            return result;  // Nothing to propagate, we're done here.

        // Well, we got a split.
        auto& [new_key, new_child] = std::get<std::pair<TK, BNode*>>(result);

        // Got space left?
        if (node->count < M - 1) {
            // Got space left.
            node->children[node->count + 1] = node->children[node->count];

            for (std::size_t j = node->count; j > i; --j) {
                node->keys[j] = std::move(node->keys[j - 1]);
                node->children[j] = node->children[j - 1];
            }

            node->keys[i] = std::move(new_key);
            node->children[i] = new_child;
            ++node->count;

            return true;
        }

        // Don't got space left.

        // Move keys to temporary "overflowed" list
        TK keys_tmp[M];
        BNode* children_tmp[M + 1];
        std::size_t e = 0;
        bool new_key_pending = true;

        for (std::size_t j = 0; j < M; ++j) {
            if (new_key_pending && (e >= node->count || new_key < node->keys[e])) {
                keys_tmp[j] = std::move(new_key);
                children_tmp[j] = new_child;
                new_key_pending = false;
            } else {
                keys_tmp[j] = std::move(node->keys[e]);
                children_tmp[j] = std::exchange(node->children[e], nullptr);
                ++e;
            }
        }

        children_tmp[M] = std::exchange(node->children[M - 1], nullptr);

        const std::size_t mid = (M - 1) / 2;
        const TK lifted = std::move(keys_tmp[mid]);

        auto* const lsplit = new BNode(M);
        lsplit->leaf = node->leaf;

        // Split sizes (if split is uneven, left is 1 key smaller)
        lsplit->count = mid;
        node->count = M - 1 - mid;

        for (std::size_t k = 0; k < lsplit->count; ++k) {
            lsplit->keys[k] = std::move(keys_tmp[k]);
            lsplit->children[k] = children_tmp[k];
        }

        lsplit->children[lsplit->count] = children_tmp[mid];

        for (std::size_t k = 0; k < node->count; ++k) {
            node->keys[k] = std::move(keys_tmp[mid + 1 + k]);
            node->children[k] = children_tmp[mid + 1 + k];
        }

        node->children[node->count] = children_tmp[M];

        return std::pair{lifted, lsplit};
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
        if (root != nullptr) {
            root->killSelf();
            root = nullptr;
        }

        n = 0;
    }

    void insert(TK key) {
        const auto result = insert(root, key);

        if (const bool* const inserted = std::get_if<bool>(&result)) {
            if (*inserted)
                ++n;
            return;
        }

        auto [new_key, new_child] = std::get<std::pair<TK, BNode*>>(result);

        auto* const new_root = new BNode(M);
        new_root->leaf = new_child == nullptr;

        new_root->keys[0] = std::move(new_key);
        new_root->count = 1;

        new_root->children[0] = new_child;
        new_root->children[1] = root;

        root = new_root;
        ++n;
    }

    void remove(const TK& key) {
        // TODO: implement
    }

    [[nodiscard]] std::ptrdiff_t height() const {
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
        auto* result = new BTree(M);

        for (const auto& el : elements)
            result->insert(el);

        return result;
    }

    [[nodiscard]] bool check_properties() const {
        return check_properties(root);
    }
};

#endif
