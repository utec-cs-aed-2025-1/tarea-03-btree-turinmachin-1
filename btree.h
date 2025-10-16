#ifndef BTree_H
#define BTree_H
#include "node.h"
#include <iostream>

using namespace std;

template <typename TK> class BTree {
private:
  Node<TK> *root;
  int M; // grado u orden del arbol
  int n; // total de elementos en el arbol

public:
  BTree(int _M) : root(nullptr), M(_M) {}

  bool search(TK key) {
    Node<TK> *current = root;
    while (current != nullptr) {
      int idx = 0;
      while (idx < current->count && key > current->keys[idx]) {
        idx++;
      }
      if (idx < current->count && key == current->keys[idx]) {
        return true;
      }
      current = current->children[idx];
    }
    return false;
  }
  void insert(TK key); // inserta un elemento
  void remove(TK key); // elimina un elemento
  int height();        // altura del arbol. Considerar altura 0 para arbol vacio
  string toString(const string &sep) { return toString(root, sep); }
  vector<TK> rangeSearch(TK begin, TK end);

  TK minKey();  // minimo valor de la llave en el arbol
  TK maxKey();  // maximo valor de la llave en el arbol
  void clear(); // eliminar todos lo elementos del arbol
  int size();   // retorna el total de elementos insertados

  // Construya un árbol B a partir de un vector de elementos ordenados
  static BTree *build_from_ordered_vector(vector<TK> elements);
  // Verifique las propiedades de un árbol B
  bool check_properties();

  ~BTree(); // liberar memoria
private:
  string toString(Node<TK> *node, string sep = ",") {
    string result = "";
    if (node == nullptr) {
      return "";
    }
    for (int i = 0; i < node->count; i++) {
      result += toString(node->children[i], sep);
      result += to_string(node->keys[i]) + sep;
    }
    result += toString(node->children[node->count], sep);
    return result;
  }
};

#endif
