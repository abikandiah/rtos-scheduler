#pragma once

class TCB;

class IntrusiveLinkedList {
 public:
  IntrusiveLinkedList() : head_(nullptr), tail_(nullptr) {}

  void add_to_tail(TCB* tcb);
  TCB* pop_head();
  void remove(TCB* tcb);

  TCB* peek_head() const;
  bool is_empty() const { return head_ == nullptr; }

  IntrusiveLinkedList(const IntrusiveLinkedList&) = delete;
  IntrusiveLinkedList& operator=(const IntrusiveLinkedList&) = delete;

 private:
  TCB* head_;
  TCB* tail_;
};