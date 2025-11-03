#ifndef BTree_H
#define BTree_H

#include <cmath>
#include <cstddef>
#include <cstdint>
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
            const TK& key = node->keys[i];

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

    static void merge_children(BNode* const node, const std::size_t i) {
        BNode* const left = node->children[i];
        BNode* const right = node->children[i + 1];

        left->keys[left->count] = std::move(node->keys[i]);

        for (std::size_t k = 0; k < right->count; ++k) {
            left->keys[left->count + 1 + k] = std::move(right->keys[k]);
            left->children[left->count + 1 + k] = std::exchange(right->children[k], nullptr);
        }

        left->children[left->count + 1 + right->count] =
            std::exchange(right->children[right->count], nullptr);

        left->count += 1 + right->count;

        delete std::exchange(node->children[i + 1], node->children[i]);

        for (std::size_t k = i; k < node->count - 1; ++k) {
            node->keys[k] = std::move(node->keys[k + 1]);
            node->children[k] = std::exchange(node->children[k + 1], nullptr);
        }

        node->children[node->count - 1] = std::exchange(node->children[node->count], nullptr);

        --node->count;
    }

    enum class DeleteResult : std::uint8_t {
        NotDeleted,
        JustDeleted,
        Deleted,
    };

    DeleteResult remove(BNode* node, const TK& key) {
        if (node == nullptr)
            return DeleteResult::NotDeleted;

        std::size_t i = 0;
        while (i < node->count && node->keys[i] < key)
            ++i;

        const bool found_key = i < node->count && node->keys[i] == key;

        if (found_key && node->leaf) {
            // Found the key!
            for (std::size_t k = i; k < node->count - 1; ++k)
                node->keys[k] = std::move(node->keys[k + 1]);

            --node->count;
            return DeleteResult::JustDeleted;
        }

        DeleteResult result{};

        if (found_key) {
            const TK& succ = minKey(node->children[i + 1]);
            node->keys[i] = succ;
            result = remove(node->children[i + 1], succ);
        } else {
            result = remove(node->children[i], key);
        }

        if (result != DeleteResult::JustDeleted)
            return result;

        // Only here we might need to fix properties.
        // (This is the actually hard part)

        const std::size_t MIN_KEYS = std::ceil(M / 2.0) - 1;

        BNode* const mid = node->children[i];

        if (mid->count >= MIN_KEYS)
            return result;

        // We gotta fix node->children[i]

        if (i > 0 && node->children[i - 1]->count > MIN_KEYS) {
            // Borrow from left
            BNode* const left = node->children[i - 1];

            mid->children[mid->count + 1] = std::exchange(mid->children[mid->count], nullptr);

            for (std::size_t k = mid->count; k > 0; ++k) {
                mid->keys[k] = std::move(mid->keys[k - 1]);
                mid->children[k] = std::exchange(mid->children[k - 1], nullptr);
            }

            mid->keys[0] = std::move(node->keys[i - 1]);
            mid->children[0] = std::exchange(left->children[left->count], nullptr);
            node->keys[i - 1] = std::move(left->keys[left->count - 1]);

            ++mid->count;
            --left->count;

            return DeleteResult::Deleted;  // We're done.
        }

        if (i < node->count && node->children[i + 1]->count > MIN_KEYS) {
            // Borrow from right
            BNode* const right = node->children[i + 1];

            mid->keys[mid->count] = std::move(node->keys[i]);
            mid->children[mid->count + 1] = std::exchange(right->children[0], nullptr);
            node->keys[i] = std::move(right->keys[0]);

            for (std::size_t k = 0; k < right->count - 1; ++k) {
                right->keys[k] = std::move(right->keys[k + 1]);
                right->children[k] = std::exchange(right->children[k + 1], nullptr);
            }

            right->children[right->count - 1] =
                std::exchange(right->children[right->count], nullptr);

            ++mid->count;
            --right->count;

            return DeleteResult::Deleted;  // We're done.
        }

        // No borrow available! We have to merge.
        if (i < node->count)
            merge_children(node, i);  // Merge with right
        else
            merge_children(node, i - 1);  // Merge with left

        return DeleteResult::JustDeleted;
    }

public:
    explicit BTree(const std::size_t M)
        : M(M) {}

    BTree(const BTree& other) = delete;

    BTree(BTree&& other) noexcept
        : M(other.M) {
        swap(other);
    }

    BTree& operator=(const BTree& other) = delete;

    BTree& operator=(BTree&& other) noexcept {
        swap(other);
        return *this;
    }

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
        const DeleteResult result = remove(root, key);

        if (result == DeleteResult::JustDeleted && root->count == 0)
            delete std::exchange(root, root->children[0]);

        const bool was_deleted =
            result == DeleteResult::Deleted || result == DeleteResult::JustDeleted;

        if (was_deleted)
            --n;
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

    static BTree* build_from_ordered_vector(const std::vector<TK>& elements, std::size_t M) {
        if (M < 3)
            throw std::invalid_argument("order must be greater than 2");

        auto* tree = new BTree(M);
        if (elements.empty())
            return tree;

        auto* root = new BNode(M);
        tree->root = root;

        std::vector<BNode*> path{root};
        for (TK elem : elements) {
            while (!path.back()->leaf) {
                BNode* cur = path.back();
                BNode*& child = cur->children[cur->count];
                if (child == nullptr)
                    child = new BNode(M);
                path.push_back(child);
            }

            BNode* leaf = path.back();
            leaf->keys[leaf->count++] = elem;
            ++tree->n;

            for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
                BNode* node = path[i];
                if (node->count < M - 1)
                    break;

                std::size_t mid = (M - 1) / 2;
                TK promoted = node->keys[mid];

                auto* right = new BNode(M);
                right->leaf = node->leaf;
                right->count = node->count - mid - 1;

                std::copy(node->keys + mid + 1, node->keys + node->count, right->keys);
                if (!node->leaf)
                    std::copy(node->children + mid + 1, node->children + node->count + 1,
                              right->children);

                node->count = mid;

                if (i == 0) {
                    auto* newRoot = new BNode(M);
                    newRoot->leaf = false;
                    newRoot->count = 1;
                    newRoot->keys[0] = promoted;
                    newRoot->children[0] = node;
                    newRoot->children[1] = right;
                    tree->root = newRoot;
                    path = {newRoot};
                    break;
                }

                BNode* parent = path[i - 1];
                std::size_t pos = 0;
                while (pos <= parent->count && parent->children[pos] != node)
                    ++pos;

                for (std::size_t i = parent->count - 1; i >= pos; --i) {
                    parent->keys[i + 1] = std::move(parent->keys[i]);
                }
                for (std::size_t i = parent->count; i > pos; --i) {
                    parent->children[i + 1] = std::move(parent->children[i]);
                }

                parent->keys[pos] = promoted;
                parent->children[pos + 1] = right;
                ++parent->count;
            }

            path.resize(1);
        }

        return tree;
    }

    [[nodiscard]] bool check_properties() const {
        return check_properties(root);
    }
};

#endif
