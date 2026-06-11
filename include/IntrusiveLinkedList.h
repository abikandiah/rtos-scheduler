#pragma once

template <typename T>
class IntrusiveLinkedList {
 public:
  IntrusiveLinkedList() : head_(nullptr), tail_(nullptr) {}

  void push_front(T* t) {
    if (head_) {
      t->next = head_;
      head_->prev = t;
    } else {
      t->next = nullptr;
      tail_ = t;
    }
    head_ = t;
    head_->prev = nullptr;
  }

  void push_back(T* t) {
    if (tail_) {
      tail_->next = t;
      t->prev = tail_;
    } else {
      t->prev = nullptr;
      head_ = t;
    }
    tail_ = t;
    tail_->next = nullptr;
  }

  T* pop_front() {
    T* front = head_;
    if (front) {
      head_ = front->next;
      front->next = nullptr;

      if (head_) {
        head_->prev = nullptr;
      }
    }

    if (head_ == nullptr) {
      tail_ = nullptr;
    }
    return front;
  }

  T* pop_back() {
    T* back = tail_;
    if (back) {
      tail_ = back->prev;
      back->prev = nullptr;

      if (tail_) {
        tail_->next = nullptr;
      }
    }

    if (tail_ == nullptr) {
      head_ = nullptr;
    }
    return back;
  }

  T* peek_front() const { return head_; }

  void remove(T* t) {
    if (t != head_) {
      t->prev->next = t->next;
    } else {
      head_ = t->next;
    }
    if (t != tail_) {
      t->next->prev = t->prev;
    } else {
      tail_ = t->prev;
    }

    t->next = nullptr;
    t->prev = nullptr;
  }

  bool is_empty() const { return head_ == nullptr; }

  IntrusiveLinkedList(const IntrusiveLinkedList&) = delete;
  IntrusiveLinkedList& operator=(const IntrusiveLinkedList&) = delete;

 private:
  T* head_;
  T* tail_;
};