/**
 * implement a container like std::linked_hashmap
 */
#ifndef SJTU_LINKEDHASHMAP_HPP
#define SJTU_LINKEDHASHMAP_HPP

// only for std::equal_to<T> and std::hash<T>
#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {
    /**
     * In linked_hashmap, iteration ordering is differ from map,
     * which is the order in which keys were inserted into the map.
     * You should maintain a doubly-linked list running through all
     * of its entries to keep the correct iteration order.
     *
     * Note that insertion order is not affected if a key is re-inserted
     * into the map.
     */
    
template<
    class Key,
    class T,
    class Hash = std::hash<Key>, 
    class Equal = std::equal_to<Key>
> class linked_hashmap {
public:
    typedef pair<const Key, T> value_type;

private:
    // Base node: only list links (used for sentinel head/tail)
    struct NodeBase {
        NodeBase* prev;
        NodeBase* next;
        NodeBase() : prev(nullptr), next(nullptr) {}
    };

    // Full node: list links + data + bucket chain
    struct Node : public NodeBase {
        value_type data;
        Node* bucket_next;  // singly-linked list within bucket

        Node(const value_type& v) : data(v), bucket_next(nullptr) {}
    };

    // Hash table: array of Node* (heads of bucket chains)
    Node** buckets;
    size_t bucket_count;
    size_t _size;

    // Doubly-linked list sentinels (heap allocated to avoid
    // requiring Key/T default constructors)
    NodeBase* list_head;  // sentinel before first element
    NodeBase* list_tail;  // sentinel after last element (== end())

    Hash hasher;
    Equal equaler;

    static const size_t INITIAL_CAPACITY = 16;
    static const size_t MAX_LOAD_NUMERATOR = 3;
    static const size_t MAX_LOAD_DENOMINATOR = 4;  // load factor = 0.75

    size_t bucket_idx(const Key& key) const {
        return hasher(key) % bucket_count;
    }

    Node* find_node(const Key& key) const {
        size_t idx = bucket_idx(key);
        Node* cur = buckets[idx];
        while (cur) {
            if (equaler(cur->data.first, key)) return cur;
            cur = cur->bucket_next;
        }
        return nullptr;
    }

    void rehash(size_t new_cap) {
        Node** new_buckets = new Node*[new_cap]();
        // Re-insert all nodes into new buckets
        NodeBase* cur = list_head->next;
        while (cur != list_tail) {
            Node* n = static_cast<Node*>(cur);
            size_t idx = hasher(n->data.first) % new_cap;
            n->bucket_next = new_buckets[idx];
            new_buckets[idx] = n;
            cur = cur->next;
        }
        delete[] buckets;
        buckets = new_buckets;
        bucket_count = new_cap;
    }

    void init_empty(size_t cap = INITIAL_CAPACITY) {
        bucket_count = cap;
        buckets = new Node*[bucket_count]();
        _size = 0;
        list_head = new NodeBase();
        list_tail = new NodeBase();
        list_head->next = list_tail;
        list_tail->prev = list_head;
    }

    void destroy_all() {
        // Delete all data nodes (not sentinels)
        NodeBase* cur = list_head->next;
        while (cur != list_tail) {
            NodeBase* nxt = cur->next;
            delete static_cast<Node*>(cur);
            cur = nxt;
        }
        delete[] buckets;
        buckets = nullptr;
        _size = 0;
    }

    void copy_from(const linked_hashmap& other) {
        // Copy in insertion order
        NodeBase* cur = other.list_head->next;
        while (cur != other.list_tail) {
            Node* n = static_cast<Node*>(cur);
            insert_no_check(n->data);
            cur = cur->next;
        }
    }

    // Insert without checking for existing key (used in copy)
    void insert_no_check(const value_type& value) {
        if (_size >= bucket_count * MAX_LOAD_NUMERATOR / MAX_LOAD_DENOMINATOR) {
            rehash(bucket_count * 2);
        }
        Node* n = new Node(value);
        size_t idx = bucket_idx(value.first);
        n->bucket_next = buckets[idx];
        buckets[idx] = n;
        // Append to end of list
        n->prev = list_tail->prev;
        n->next = list_tail;
        list_tail->prev->next = n;
        list_tail->prev = n;
        _size++;
    }

public:
    class const_iterator;

    class iterator {
    private:
        NodeBase* node;
        const linked_hashmap* map;

        friend class linked_hashmap;
        friend class const_iterator;

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename linked_hashmap::value_type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::output_iterator_tag;

        iterator() : node(nullptr), map(nullptr) {}
        iterator(NodeBase* n, const linked_hashmap* m) : node(n), map(m) {}
        iterator(const iterator& other) : node(other.node), map(other.map) {}

        // post-increment
        iterator operator++(int) {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            iterator tmp = *this;
            node = node->next;
            return tmp;
        }
        // pre-increment
        iterator& operator++() {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            node = node->next;
            return *this;
        }
        // post-decrement
        iterator operator--(int) {
            if (node == nullptr || node->prev == nullptr || node->prev == map->list_head)
                throw invalid_iterator();
            iterator tmp = *this;
            node = node->prev;
            return tmp;
        }
        // pre-decrement
        iterator& operator--() {
            if (node == nullptr || node->prev == nullptr || node->prev == map->list_head)
                throw invalid_iterator();
            node = node->prev;
            return *this;
        }

        value_type& operator*() const {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            return static_cast<Node*>(node)->data;
        }

        value_type* operator->() const noexcept {
            return &(static_cast<Node*>(node)->data);
        }

        bool operator==(const iterator& rhs) const { return node == rhs.node; }
        bool operator==(const const_iterator& rhs) const { return node == rhs.node; }
        bool operator!=(const iterator& rhs) const { return node != rhs.node; }
        bool operator!=(const const_iterator& rhs) const { return node != rhs.node; }
    };

    class const_iterator {
    private:
        const NodeBase* node;
        const linked_hashmap* map;

        friend class linked_hashmap;
        friend class iterator;

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename linked_hashmap::value_type;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::output_iterator_tag;

        const_iterator() : node(nullptr), map(nullptr) {}
        const_iterator(const NodeBase* n, const linked_hashmap* m) : node(n), map(m) {}
        const_iterator(const const_iterator& other) : node(other.node), map(other.map) {}
        const_iterator(const iterator& other) : node(other.node), map(other.map) {}

        const_iterator operator++(int) {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            const_iterator tmp = *this;
            node = node->next;
            return tmp;
        }
        const_iterator& operator++() {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            node = node->next;
            return *this;
        }
        const_iterator operator--(int) {
            if (node == nullptr || node->prev == nullptr || node->prev == map->list_head)
                throw invalid_iterator();
            const_iterator tmp = *this;
            node = node->prev;
            return tmp;
        }
        const_iterator& operator--() {
            if (node == nullptr || node->prev == nullptr || node->prev == map->list_head)
                throw invalid_iterator();
            node = node->prev;
            return *this;
        }

        const value_type& operator*() const {
            if (node == nullptr || node == map->list_tail)
                throw invalid_iterator();
            return static_cast<const Node*>(node)->data;
        }

        const value_type* operator->() const noexcept {
            return &(static_cast<const Node*>(node)->data);
        }

        bool operator==(const const_iterator& rhs) const { return node == rhs.node; }
        bool operator==(const iterator& rhs) const { return node == rhs.node; }
        bool operator!=(const const_iterator& rhs) const { return node != rhs.node; }
        bool operator!=(const iterator& rhs) const { return node != rhs.node; }
    };

    // Constructors
    linked_hashmap() {
        init_empty();
    }

    linked_hashmap(const linked_hashmap& other) {
        init_empty();
        copy_from(other);
    }

    linked_hashmap& operator=(const linked_hashmap& other) {
        if (this == &other) return *this;
        // Clear current contents
        NodeBase* cur = list_head->next;
        while (cur != list_tail) {
            NodeBase* nxt = cur->next;
            delete static_cast<Node*>(cur);
            cur = nxt;
        }
        // Reset list and buckets
        list_head->next = list_tail;
        list_tail->prev = list_head;
        _size = 0;
        // Reset buckets
        for (size_t i = 0; i < bucket_count; i++) buckets[i] = nullptr;
        // Copy from other
        copy_from(other);
        return *this;
    }

    ~linked_hashmap() {
        destroy_all();
        delete list_head;
        delete list_tail;
    }

    T& at(const Key& key) {
        Node* n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    const T& at(const Key& key) const {
        Node* n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    T& operator[](const Key& key) {
        Node* n = find_node(key);
        if (n) return n->data.second;
        // Insert with default value
        insert_no_check(value_type(key, T()));
        return find_node(key)->data.second;
    }

    const T& operator[](const Key& key) const {
        Node* n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    iterator begin() {
        return iterator(list_head->next, this);
    }
    const_iterator cbegin() const {
        return const_iterator(list_head->next, this);
    }
    iterator end() {
        return iterator(list_tail, this);
    }
    const_iterator cend() const {
        return const_iterator(list_tail, this);
    }

    bool empty() const { return _size == 0; }
    size_t size() const { return _size; }

    void clear() {
        NodeBase* cur = list_head->next;
        while (cur != list_tail) {
            NodeBase* nxt = cur->next;
            delete static_cast<Node*>(cur);
            cur = nxt;
        }
        list_head->next = list_tail;
        list_tail->prev = list_head;
        for (size_t i = 0; i < bucket_count; i++) buckets[i] = nullptr;
        _size = 0;
    }

    pair<iterator, bool> insert(const value_type& value) {
        Node* existing = find_node(value.first);
        if (existing) {
            return pair<iterator, bool>(iterator(existing, this), false);
        }
        insert_no_check(value);
        Node* n = find_node(value.first);
        return pair<iterator, bool>(iterator(n, this), true);
    }

    void erase(iterator pos) {
        if (pos.node == nullptr || pos.node == list_tail || pos.map != this)
            throw invalid_iterator();

        Node* n = static_cast<Node*>(pos.node);

        // Remove from bucket chain
        size_t idx = bucket_idx(n->data.first);
        Node* cur = buckets[idx];
        Node* prev_b = nullptr;
        while (cur) {
            if (cur == n) {
                if (prev_b) prev_b->bucket_next = cur->bucket_next;
                else buckets[idx] = cur->bucket_next;
                break;
            }
            prev_b = cur;
            cur = cur->bucket_next;
        }

        // Remove from list
        n->prev->next = n->next;
        n->next->prev = n->prev;

        delete n;
        _size--;
    }

    size_t count(const Key& key) const {
        return find_node(key) ? 1 : 0;
    }

    iterator find(const Key& key) {
        Node* n = find_node(key);
        if (!n) return end();
        return iterator(n, this);
    }

    const_iterator find(const Key& key) const {
        Node* n = find_node(key);
        if (!n) return cend();
        return const_iterator(n, this);
    }
};

}  // namespace sjtu

#endif
