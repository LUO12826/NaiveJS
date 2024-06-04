#include "JSRunLoop.h"

#include <cstdio>
#include <unistd.h>
#include <sys/event.h>
#include <vector>
#include <thread>
#include "NjsVM.h"

namespace njs {

JSRunLoop::JSRunLoop(NjsVM& vm): vm(vm) {
#ifdef __APPLE__
  mux_fd = kqueue();
  if (mux_fd == -1) {
    perror("kqueue init failed.");
    exit(EXIT_FAILURE);
  }
#elif __linux__
  mux_fd = epoll_create1(0);
  if (mux_fd == -1) {
    perror("epoll_create failed.");
    exit(EXIT_FAILURE);
  }
#endif
  setup_pipe();
  timer_thread = std::thread(&JSRunLoop::timer_loop, this);
}

JSRunLoop::~JSRunLoop() {
  write(pipe_write_fd, "exit", 5);
  timer_thread.join();
  close(mux_fd);
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

#ifdef __APPLE__

  struct kevent pipe_event;
  EV_SET(&pipe_event, pipe_read_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);

  if (kevent(mux_fd, &pipe_event, 1, nullptr, 0, nullptr) == -1) {
    perror("kqueue init failed.");
    exit(EXIT_FAILURE);
  }

#elif __linux__

  // set read fd to non-block mode
  int flags = fcntl(pipe_read_fd, F_GETFL, 0);
  fcntl(pipe_read_fd, F_SETFL, flags | O_NONBLOCK);

  epoll_event pipe_event;
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = pipe_read_fd;

  if (epoll_ctl(mux_fd, EPOLL_CTL_ADD, read_fd, &event) == -1) {
    perror("epoll_ctl add fd failed.");
    exit(EXIT_FAILURE);
  }
#endif
}

void JSRunLoop::timer_loop() {
  const int MAX_EVENTS = 20;

#ifdef __APPLE__

  struct kevent event_slot[MAX_EVENTS];
  while (true) {
    int ret = kevent(mux_fd, nullptr, 0, event_slot, MAX_EVENTS, nullptr);
    if (ret == -1) {
      perror("kqueue poll failed.");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < ret; i++) {
      if (event_slot[i].filter == EVFILT_TIMER) {
        JSTask *task = (JSTask *)event_slot[i].udata;

        if (task->is_timer && not task->repeat) {
          struct kevent event;
          EV_SET(&event, task->task_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
          kevent(mux_fd, &event, 1, nullptr, 0, nullptr);
        }
        post_task(task);
      }
      else if (event_slot[i].filter == EVFILT_READ && event_slot[i].ident == pipe_read_fd) {
        return;
      }
    }
  }

#elif __linux__

  struct epoll_event event_slot[MAX_EVENTS];
  while (true) {
    int ret = epoll_wait(mux_fd, events, MAX_EVENTS, -1);
    if (ret == -1) {
      perror("epoll poll failed.");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < ret; i++) {
      if (event_slot[i].data.fd != pipe_read_fd) [[likely]] {
        JSTask *task = (JSTask *)event_slot[i].data.ptr;

        if (task.is_timer) {
          int timer_fd = task->timer_fd;
          uint64_t expirations;
          read(timer_fd, &expirations, sizeof(uint64_t));
          if (!task->repeat) {
            epoll_ctl(mux_fd, EPOLL_CTL_DEL, timer_fd, NULL);
          }
        }
        post_task(task);
      }
      else {
        return;
      }
    }
  }

#endif
}

size_t JSRunLoop::add_timer(JSFunction* func, size_t timeout, bool repeat) {
  auto& task = task_pool[task_counter];

  task.task_id = task_counter;
  task.is_timer = true;
  task.task_func = JSValue(func);
  task.timeout = timeout;
  task.repeat = repeat;

  if (timeout == 0 && !repeat) {
    macro_queue_lock.lock();
    macro_task_queue.push_back(&task);
    macro_queue_lock.unlock();
  }
  else {
    if (repeat) assert(timeout != 0);

#ifdef __APPLE__

    uint16_t event_flags = EV_ADD | EV_ENABLE;
    if (!task.repeat) event_flags |= EV_ONESHOT;

    struct kevent event;
    EV_SET(&event, task.task_id, EVFILT_TIMER, event_flags,
           NOTE_USECONDS, timeout * 1000, (void*)&task);

    int ret = kevent(mux_fd, &event, 1, nullptr, 0, nullptr);
    if (ret == -1) {
      perror("kqueue add task failed.");
      exit(EXIT_FAILURE);
    }

#elif __linux__

    int timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1) {
      perror("timerfd_create failed");
      exit(EXIT_FAILURE);
    }
    task.timer_fd = timer_fd;

    itimerspec timespec;
    memset(&timespec, 0, sizeof(timespec));
    timespec.it_value.tv_sec = timeout / 1000;
    timespec.it_value.tv_nsec = (timeout % 1000) * 1000000;
    if (repeat) {
      timespec.it_interval.tv_sec = timeout / 1000;
      timespec.it_interval.tv_nsec = (timeout % 1000) * 1000000;
    }

    if (timerfd_settime(timer_fd, 0, &timespec, NULL) == -1) {
      perror("timerfd_settime failed");
      exit(EXIT_FAILURE);
    }

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.ptr = (void *)&task;

    if (epoll_ctl(mux_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1) {
      perror("epoll_ctl failed");
      close(timer_fd);
      exit(EXIT_FAILURE);
    }

#endif
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

#ifdef __linux__
  int timer_fd = iter->second.timer_fd;
#endif
  // remove the task from the task pool
  task_pool.erase(timer_id);

#ifdef __APPLE__
  // remove the timer from kqueue
  struct kevent event;
  EV_SET(&event, timer_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);

  int ret = kevent(mux_fd, &event, 1, nullptr, 0, nullptr);
  if (ret == -1) {
    perror("kqueue remove task failed.");
    exit(EXIT_FAILURE);
  }

#elif __linux__
  int ret = epoll_ctl(mux_fd, EPOLL_CTL_DEL, timer_fd, NULL);
  if (ret == -1) {
    perror("epoll_ctl: remove fd failed.");
    exit(EXIT_FAILURE);
  }
#endif

  return true;
}

JSTask *JSRunLoop::add_task(JSFunction* func) {
  JSTask task {
      .task_id = task_counter++,
      .is_timer = false,
      .task_func = JSValue(func),
  };

  auto [iter, succeeded] = task_pool.emplace(task.task_id, task);
  return &iter->second;
}

}