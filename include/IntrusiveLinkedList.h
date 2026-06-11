#pragma once

template <typename T>
class IntrusiveLinkedList {
 public:
  IntrusiveLinkedList() noexcept : head_(nullptr), tail_(nullptr) {}

  void push_front(T* t) noexcept {
    t->prev = nullptr;
    t->next = head_;
    if (head_) {
      head_->prev = t;
    } else {
      tail_ = t;
    }
    head_ = t;
  }

  void push_back(T* t) noexcept {
    t->next = nullptr;
    t->prev = tail_;
    if (tail_) {
      tail_->next = t;
    } else {
      head_ = t;
    }
    tail_ = t;
  }

  T* pop_front() noexcept {
    T* front = head_;
    if (front) {
      head_ = front->next;
      if (head_) {
        head_->prev = nullptr;
      } else {
        tail_ = nullptr;
      }
      front->next = nullptr;
    }
    return front;
  }

  T* pop_back() noexcept {
    T* back = tail_;
    if (back) {
      tail_ = back->prev;
      if (tail_) {
        tail_->next = nullptr;
      } else {
        head_ = nullptr;
      }
      back->prev = nullptr;
    }
    return back;
  }

  void remove(T* t) noexcept {
    if (t->prev) {
      t->prev->next = t->next;
    } else {
      head_ = t->next;
    }

    if (t->next) {
      t->next->prev = t->prev;
    } else {
      tail_ = t->prev;
    }

    t->next = nullptr;
    t->prev = nullptr;
  }

  T* peek_front() const noexcept { return head_; }
  T* peek_back() const noexcept { return tail_; }
  bool is_empty() const noexcept { return head_ == nullptr; }

  IntrusiveLinkedList(const IntrusiveLinkedList&) = delete;
  IntrusiveLinkedList& operator=(const IntrusiveLinkedList&) = delete;

 private:
  T* head_;
  T* tail_;
};