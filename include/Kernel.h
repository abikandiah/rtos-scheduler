#pragma once

class Task;

class Kernel {
  Kernel();
  void submit(Task& task);
  void start();
};
