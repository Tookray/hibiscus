#pragma once

#include <concepts>

template <typename T>
class Node {
public:
        Node<T> *previous;
        Node<T> *next;

        T value;
};

template <typename T>
class List {
private:
        Node<T> *head = nullptr;
        Node<T> *tail = nullptr;

public:
        List() = default;
        ~List() = default;

        // NOTE: Semantically, 'node' belongs to the list and 'new_node' does not.
        //       The methods are implemented under the above assumptions.

        Node<T> *back();

        Node<T> *front();

        void insert_after(Node<T> *node, Node<T> *new_node);

        void insert_before(Node<T> *node, Node<T> *new_node);

        Node<T> *pop_back();

        Node<T> *pop_front();

        void push_back(Node<T> *new_node);

        void push_front(Node<T> *new_node);

        void remove(Node<T> *node);
};

template <typename T>
Node<T> *List<T>::back() {
        return tail;
}

template <typename T>
Node<T> *List<T>::front() {
        return head;
}

template <typename T>
void List<T>::insert_after(Node<T> *node, Node<T> *new_node) {
        new_node->next = node->next;
        new_node->previous = node;

        if (node->next != nullptr) {
                node->next->previous = new_node;
        } else {
                tail = new_node;
        }

        node->next = new_node;
}

template <typename T>
void List<T>::insert_before(Node<T> *node, Node<T> *new_node) {
        new_node->next = node;
        new_node->previous = node->previous;

        if (node->previous != nullptr) {
                node->previous->next = new_node;
        } else {
                head = new_node;
        }

        node->previous = new_node;
}

template <typename T>
Node<T> *List<T>::pop_back() {
        if (tail == nullptr) {
                return nullptr;
        }

        Node<T> *node = tail;

        if (tail->previous != nullptr) {
                tail->previous->next = nullptr;
        } else {
                head = nullptr;
        }

        tail = tail->previous;

        return node;
}

template <typename T>
Node<T> *List<T>::pop_front() {
        if (head == nullptr) {
                return nullptr;
        }

        Node<T> *node = head;

        if (head->next != nullptr) {
                head->next->previous = nullptr;
        } else {
                tail = nullptr;
        }

        head = head->next;

        return node;
}

template <typename T>
void List<T>::push_back(Node<T> *new_node) {
        if (tail == nullptr) {
                head = new_node;
        } else {
                tail->next = new_node;
        }

        new_node->previous = tail;
        new_node->next = nullptr;

        tail = new_node;
}

template <typename T>
void List<T>::push_front(Node<T> *new_node) {
        if (head == nullptr) {
                tail = new_node;
        } else {
                head->previous = new_node;
        }

        new_node->next = head;
        new_node->previous = nullptr;

        head = new_node;
}

template <typename T>
void List<T>::remove(Node<T> *node) {
        if (node == head) {
                head = node->next;
        } else {
                node->previous->next = node->next;
        }

        if (node == tail) {
                tail = node->previous;
        } else {
                node->next->previous = node->previous;
        }
}
