// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#ifndef NODELIST_H
#define NODELIST_H

#include "common.h"

// Simple collection type for a linked list where the list overhead is
// embedded into the held type itself, meaning zero heap overhead and
// O(1) time complexity for both insert and remove by reference.

template<class T> class Node;
template<class T> class NodeList;

template<class T>
class Node
{
    friend class NodeList<T>;
    Node<T>* _prev = nullptr;
    Node<T>* _next = nullptr;
    NodeList<T>* _list = nullptr;
public:
    T* prev() { return static_cast<T*>(_prev); }
    T* next() { return static_cast<T*>(_next); }
    NodeList<T>* list() { return _list; }
public:
    Node() {}
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    ~Node() { remove(); }
    // Move the node to before another node.
    inline void insertBefore(Node<T>* node);
    // Move the node to after another node.
    inline void insertAfter(Node<T>* node);
    // Move the node to first in a list.
    inline void insertFirst(NodeList<T>* list);
    // Move the node to last in a list.
    inline void insertLast(NodeList<T>* list);
    // Remove the node from any list.
    inline void remove();
};

template<class T>
class NodeList
{
    friend class Node<T>;
    Node<T>* _first = nullptr;
    Node<T>* _last = nullptr;
    int _count = 0;
public:
    T* first() { return static_cast<T*>(_first); }
    T* last() { return static_cast<T*>(_last); }
    int count() { return _count; }
public:
    NodeList() {}
    NodeList(const NodeList&) = delete;
    NodeList(NodeList&&) = delete;
    ~NodeList() { clear(); }
    // Unlink all nodes in the list.
    inline void clear();
    // Check if a given node is inside this list.
    inline bool contains(const Node<T>* node) const;
};

template<class T>
inline void Node<T>::insertBefore(Node<T>* node)
{
    remove();
    if ((_prev = std::exchange(node->_prev, this)))
        _prev->_next = this;
    else if (node->_list)
        node->_list->_first = this;
    _next = node;
    if ((_list = node->_list))
        _list->_count++;
}
template<class T>
inline void Node<T>::insertAfter(Node<T>* node)
{
    remove();
    if ((_next = std::exchange(node->_next, this)))
        _next->_prev = this;
    else if (node->_list)
        node->_list->_last = this;
    _prev = node;
    if ((_list = node->_list))
        _list->_count++;
}
template<class T>
inline void Node<T>::insertFirst(NodeList<T>* list)
{
    remove();
    if ((_next = std::exchange(list->_first, this)))
        _next->_prev = this;
    else
        list->_last = this;
    _list = list;
    _list->_count++;
}
template<class T>
inline void Node<T>::insertLast(NodeList<T>* list)
{
    remove();
    if ((_prev = std::exchange(list->_last, this)))
        _prev->_next = this;
    else
        list->_first = this;
    _list = list;
    _list->_count++;
}
template<class T>
inline void Node<T>::remove()
{
    if (_prev)
        _prev->_next = _next;
    else if (_list)
        _list->_first = _next;
    if (_next)
        _next->_prev = _prev;
    else if (_list)
        _list->_last = _prev;
    if (_list)
        --_list->_count;
    _prev = nullptr;
    _next = nullptr;
    _list = nullptr;
}

template<class T>
inline void NodeList<T>::clear()
{
    while (_first)
    {
        _first->remove();
    }
}
template<class T>
inline bool NodeList<T>::contains(const Node<T>* node) const
{
    return node->_list == this;
}


#endif
