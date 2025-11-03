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
#include <stack>
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

    void remove(const TK& key, BNode* node) {
        
        
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

        //Temporal
        std::cout << this->toString(", ") << std::endl;

        BNode* cur = root;
        std::stack<BNode*> st;

        while (cur != nullptr) {
            std::size_t idx = 0;
            while (idx < cur->count && key > cur->keys[idx]) {
                idx++;
            }
            
            if (idx < cur->count && key == cur->keys[idx]) {
                // En este punto ya encontró el elemento a eliminar
                // cur = el nodo en el que se encuentra el key, idx = la posición en el nodo

                if (!cur->leaf) {
                    // Primero manejamos el caso "no bonito", en el que el key se encuentra en un nodo interno
                    // Aqui tenemos que tomar "prestado" del sucesor o el anterior, que siempre es una hoja.
                    // La elección es arbitraria, pero eligiremos el anterior siempre, para reducir los desplazamientos a la hora de eliminar.
                    
                    // Aqui haremos el algoritmo para intercambiar con el predecesor de un nodo INTERNO.
                    BNode* temp = cur;
                    cur = cur->children[idx];
                    while(!cur->leaf) {
                        st.push(cur);
                        cur = cur->children[cur->count];
                    }
                    std::swap(temp->keys[idx], cur->keys[cur->count - 1]);
                    idx = cur->count - 1;
                }

                // Para este punto cur necesariamente es hoja, por lo que eliminamos directamente
                while (idx < cur->count - 1) {
                    cur->keys[idx] = std::move(cur->keys[idx + 1]);
                    ++idx;
                }
                --cur->count;
                --n;
                
                // En este punto ya eliminamos, solo queda restaurar el arbol B.
                const int minKeys = (M - 1) / 2;

                while (!st.empty() && cur->count < minKeys) {
                    // Esto significa que le faltan elementos a la hoja. Vamos a manejar caso por caso.

                    BNode* parent = st.top();
                    st.pop();

                    std::size_t pos = 0; // En este caso pos representa el indice del nodo donde se elimino respecto al padre.
                    while (pos <= parent->count && parent->children[pos] != cur) { ++pos; }
                    
                    // Se guardan los hermanos de cur.
                    BNode* left = (pos > 0) ? parent->children[pos - 1] : nullptr;
                    BNode* right = (pos < parent->count) ? parent->children[pos + 1] : nullptr;

                    if (left && left->count > minKeys) {
                        // CASO 1.1: Prestar de vecino izquierda y realizar rotacion
                        std::cout << "CASO 1.1" << std::endl;
                        // Primero tomar del padre (nuevo minimo de current) y desplazar todo a la derecha
                        cur->keys[0] = std::move(parent->keys[pos - 1]);
                        for (int i = cur->count; i > 0; --i) {
                            cur->keys[i] = std::move(cur->keys[i - 1]);
                        }

                        if (!cur->leaf) {
                            cur->children[0] = std::move(left->children[left->count]);
                            for (int i = cur->count + 1; i > 0; --i) {
                                cur->children[i] = std::move(cur->children[i - 1]);
                            }
                        }
                        cur->count++;

                        // Actualizar padre y tomar prestado del hijo izquierda
                        parent->keys[pos - 1] = std::move(left->keys[left->count - 1]);
                        
                        // Hijo izquierdo presto, por lo que pierde un elemento.
                        left->count--;
                        break;
                    } else if (right && right->count > minKeys) {
                        // CASO 1.2: Prestar de vecino derecho y realizar rotacion
                        std::cout << "CASO 1.2" << std::endl;
                        // Primero tomar del padre (nuevo maximo de current) y colocar al final.
                        cur->keys[cur->count] = parent->keys[pos];
                        if (!cur->leaf) {
                            cur->children[cur->count + 1] = std::move(right->children[0]);
                        }
                        cur->count++;
                        
                        // Padre toma prestado del hijo derecho
                        parent->keys[pos] = std::move(right->keys[0]);

                        // Actualizar hijo derecho, el cual presto y pierde un elemento. Desplazar todo a la izquierda.
                        for (std::size_t i = 0; i < right->count - 1; ++i) {
                            right->keys[i] = std::move(right->keys[i + 1]);
                        }
                        if (!right->leaf) {
                            for (std::size_t i = 0; i < right->count; ++i) {
                                right->children[i] = std::move(right->children[i + 1]);
                            }
                        }
                        right->count--;

                        break;
                    } else if (left) {
                        // Caso 2.1: Merge con left
                        std::cout << "CASO 2.1" << std::endl;
                        // Primero bajamos el parent a left.
                        left->keys[left->count] = parent->keys[pos - 1];
                        left->count++;
                        
                        // Copiamos todo current a left
                        for (std::size_t i = 0; i < cur->count; ++i) {
                            left->keys[left->count + i] = cur->keys[i];
                        }
                        if (!cur->leaf) {
                            for (std::size_t i = 0; i <= cur->count; ++i) {
                                left->children[left->count + i] = cur->children[i];
                            }
                        }
                        left->count += cur->count;
                        
                        for (std::size_t i = pos - 1; i < parent->count - 1; ++i) {
                            parent->keys[i] = parent->keys[i + 1];
                        }
                        // Revisar el <=, puede que esté mal
                        for (std::size_t i = pos; i <= parent->count; ++i) {
                            parent->children[i] = parent->children[i + 1];
                        }
                        parent->count--;

                        delete cur;
                        cur = parent;
                    } else if (right) {
                        // Caso 2.2: Merge con right
                        std::cout << "CASO 2.2" << std::endl;
                        // Primero traemos el padre al current.
                        cur->keys[cur->count] = parent->keys[pos];
                        cur->count++;

                        // Luego copiamos todo right al current.
                        for (std::size_t i = 0; i < right->count; ++i) {
                            cur->keys[cur->count + i] = right->keys[i];
                        }
                        if (!cur->leaf) {
                            for (std::size_t i = 0; i <= right->count; ++i) {
                                cur->children[cur->count + i] = right->children[i];
                            }
                        }
                        cur->count += right->count;

                        // Finalmente se actualiza el padre realizando el desplazamiento correspondiente
                        // Aqui falta revisar bien estos dos for loops
                        for (std::size_t i = pos; i < parent->count - 1; ++i) {
                            parent->keys[i] = parent->keys[i + 1];
                        }
                        for (std::size_t i = pos + 1; i <= parent->count; ++i) {
                            parent->children[i] = parent->children[i + 1];
                        }
                        parent->count--;

                        delete right;
                        cur = parent;
                    }
                }

                return;
            }
            
            st.push(cur);
            cur = cur->children[idx];
        }

        // Si llega a este punto es que el nodo a eliminar no se encuentra en el arbol
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
