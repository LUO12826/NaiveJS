#include "JSRunLoop.h"

#include <cstdio>
#include <unistd.h>
#include <sys/event.h>
#include <vector>
#include <thread>
#include "NjsVM.h"

namespace njs {

JSRunLoop::JSRunLoop(NjsVM& vm): vm(vm) {
  kqueue_id = kqueue();
  if (kqueue_id == -1) {
    perror("kqueue init failed.");
    exit(EXIT_FAILURE);
  }
  setup_pipe();
  timer_thread = std::thread(&JSRunLoop::timer_loop, this);
}

JSRunLoop::~JSRunLoop() {
  write(pipe_write_fd, "exit", 5);
  timer_thread.join();
  close(kqueue_id);
  close(pipe_read_fd);
  close(pipe_write_fd);
}

void JSRunLoop::loop() {
  while (!task_pool.empty() || !macro_task_queue.empty()) {
    JSTask *task;
    {
      std::unique_lock<std::mutex> lock(macro_queue_lock);
      macro_queue_cv.wait(lock, [this] { return !macro_task_queue.empty(); });

      task = macro_task_queue.front();
      macro_task_queue.pop_front();
    }
    // the task is canceled.
    if (task == nullptr) continue;

    if (!task->canceled) vm.execute_task(*task);
    if (!task->repeat) task_pool.erase(task->task_id);
  }
}

void JSRunLoop::post_task(JSTask *task) {
  macro_queue_lock.lock();
  macro_task_queue.push_back(task);
  macro_queue_lock.unlock();
  macro_queue_cv.notify_one();
}

void JSRunLoop::setup_pipe() {
  int pipe_fds[2];

  if (pipe(pipe_fds) == -1) {
    perror("pipe creation failed.");
    exit(EXIT_FAILURE);
  }
  pipe_read_fd = pipe_fds[0];
  pipe_write_fd = pipe_fds[1];

  struct kevent pipe_event;
  EV_SET(&pipe_event, pipe_read_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);

  if (kevent(kqueue_id, &pipe_event, 1, nullptr, 0, nullptr) == -1) {
    perror("kqueue init failed.");
    exit(EXIT_FAILURE);
  }
}

void JSRunLoop::timer_loop() {

  std::vector<struct kevent> event_slot(10);

  while (true) {
    if (task_pool.size() > event_slot.size()) {
      event_slot.resize(task_pool.size());
    }

    int ret = kevent(kqueue_id, nullptr, 0, event_slot.data(), event_slot.size(), nullptr);
    if (ret == -1) {
      perror("kqueue poll failed.");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < ret; i++) {
      if (event_slot[i].filter == EVFILT_TIMER) {
        post_task((JSTask *)event_slot[i].udata);
      }
      else if (event_slot[i].filter == EVFILT_READ && event_slot[i].ident == pipe_read_fd) {
        return;
      }
    }
  }
}

size_t JSRunLoop::add_timer(JSFunction* func, size_t timeout, bool repeat) {

  JSTask task {
      .task_id = task_counter,
      .task_func = JSValue(func),
      .timeout = timeout,
      .repeat = repeat,
  };

  auto [iter, succeeded] = task_pool.emplace(task.task_id, task);
  JSTask *task_ptr = &iter->second;

  if (timeout == 0 && !repeat) {
    macro_queue_lock.lock();
    macro_task_queue.push_back(task_ptr);
    macro_queue_lock.unlock();
  }
  else {
    if (task.repeat) assert(task.timeout != 0);

    uint16_t event_flags = EV_ADD | EV_ENABLE;
    if (!task.repeat) event_flags |= EV_ONESHOT;

    struct kevent event;
    EV_SET(&event, task.task_id, EVFILT_TIMER, event_flags,
           NOTE_USECONDS, task.timeout * 1000, (void*)task_ptr);

    int ret = kevent(kqueue_id, &event, 1, nullptr, 0, nullptr);
    if (ret == -1) {
      perror("kqueue add task failed.");
      exit(EXIT_FAILURE);
    }
  }

  task_counter += 1;
  return task_counter - 1;
}

bool JSRunLoop::remove_timer(size_t timer_id) {
  auto iter = task_pool.find(timer_id);
  if (iter == task_pool.end()) {
    return false;
  }
  // remove the task from the macro task queue
  for (auto& task : macro_task_queue) {
    if (task->task_id == timer_id) task = nullptr;
  }
  // remove the task from the task pool
  task_pool.erase(timer_id);

  // remove the timer from kqueue
  struct kevent event;
  EV_SET(&event, timer_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

  int ret = kevent(kqueue_id, &event, 1, nullptr, 0, nullptr);
  if (ret == -1) {
    perror("kqueue remove task failed.");
    exit(EXIT_FAILURE);
  }

  return true;
}

JSTask *JSRunLoop::add_task(JSFunction* func) {
  JSTask task {
      .task_id = task_counter++,
      .task_func = JSValue(func),
  };

  auto [iter, succeeded] = task_pool.emplace(task.task_id, task);
  return &iter->second;
}

}