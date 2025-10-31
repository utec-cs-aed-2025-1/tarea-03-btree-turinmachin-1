#ifndef NODE_H
#define NODE_H

#include <cstddef>
#include <utility>

template<typename TK>
struct Node {
    TK* keys;
    Node** children;
    std::size_t count = 0;
    bool leaf = true;

    Node() = delete;

    Node(const Node&) = delete;

    Node(Node&& other) noexcept {
        std::swap(keys, other.keys);
        std::swap(children, other.children);
        std::swap(count, other.count);
        std::swap(leaf, other.leaf);
    }

    Node& operator=(const Node&) = delete;

    Node& operator=(Node&& other) noexcept {
        std::swap(keys, other.keys);
        std::swap(children, other.children);
        std::swap(count, other.count);
        std::swap(leaf, other.leaf);
    }

    explicit Node(const std::size_t M)
        : keys(new TK[M - 1]),
          children(new Node*[M]) {}

    // Este destructor ejecuta cuando se elimina un nodo. Por conveniencia, solo limpia sus propios
    // recursos y no a sus children
    ~Node() {
        delete[] keys;
        delete[] children;
    }

    // Este método es como el destructor pero también limpia a los children recursivamente
    void killSelf() {
        for (std::size_t i = 0; i < count + 1; ++i)
            children[i]->killSelf();

        delete this;
    }
};

#endif
