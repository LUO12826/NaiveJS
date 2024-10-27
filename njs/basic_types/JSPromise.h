#ifndef NJS_JS_PROMISE_H
#define NJS_JS_PROMISE_H

#include <cstdint>
#include <array>
#include "JSObject.h"
#include "njs/vm/NjsVM.h"
#include "testing_and_comparison.h"

namespace njs {

using u32 = uint32_t;
using std::array;
using std::vector;

struct PromiseThenRecord {
  bool then_relay {true};
  JSValue on_fulfilled;
  JSValue on_rejected;
  JSValue next_resolve;
  JSValue next_reject;

  void set_undefined() {
    on_fulfilled.set_undefined();
    on_rejected.set_undefined();
    next_resolve.set_undefined();
    next_reject.set_undefined();
  }

  bool gc_scan_children(GCHeap& heap) {
    bool child_young = false;
    gc_check_and_visit_object(child_young, on_fulfilled);
    gc_check_and_visit_object(child_young, on_rejected);
    gc_check_and_visit_object(child_young, next_resolve);
    gc_check_and_visit_object(child_young, next_reject);
    return child_young;
  }

  void gc_mark_children() {
    gc_check_and_mark_object(on_fulfilled);
    gc_check_and_mark_object(on_rejected);
    gc_check_and_mark_object(next_resolve);
    gc_check_and_mark_object(next_reject);
  }

  bool gc_has_young_child(GCObject *oldgen_start) {
    gc_check_object_young(on_fulfilled);
    gc_check_object_young(on_rejected);
    gc_check_object_young(next_resolve);
    gc_check_object_young(next_reject);
    return false;
  }
};

class JSPromise : public JSObject {
friend class JSPromisePrototype;

 public:
  static inline JSFunctionMeta *resolve_func_meta;
  static inline JSFunctionMeta *reject_func_meta;
  static inline JSFunctionMeta *then_fulfilled_func_for_finally_meta;
  static inline JSFunctionMeta *then_rejected_func_for_finally_meta;
  static inline JSFunctionMeta *func_return_auxiliary_meta;
  static inline JSFunctionMeta *func_throw_auxiliary_meta;

  enum State: uint8_t {
    // do not change the order or numeric value
    FULFILLED = 0,
    REJECTED,
    PENDING,
  };

  static Completion New(NjsVM& vm, JSValueRef executor) {
    assert(executor.is_function());
    HANDLE_COLLECTOR;

    JSValue promise(vm.heap.new_object<JSPromise>(vm));
    gc_handle_add(promise);
    auto resolve_reject = build_resolve_reject_function(vm, promise);
    gc_handle_add(resolve_reject[0]);
    gc_handle_add(resolve_reject[1]);

    Completion comp = vm.call_function(executor, undefined, undefined,
                                       {resolve_reject.data(), 2});
    gc_handle_add(comp.get_value());
    if (comp.is_throw()) {
      settling_function(vm, resolve_reject[1], undefined,
                                {&comp.get_value(), 1}, CallFlags());
    }

    return promise;
  }

  static void add_internal_function_meta(NjsVM& vm) {
    resolve_func_meta = build_func_meta(settling_function, FULFILLED);
    reject_func_meta = build_func_meta(settling_function, REJECTED);

    then_fulfilled_func_for_finally_meta = build_func_meta(then_callback_for_finally, FULFILLED);
    then_rejected_func_for_finally_meta = build_func_meta(then_callback_for_finally, REJECTED);
    func_return_auxiliary_meta = build_func_meta(passthrough_return_auxiliary);
    func_throw_auxiliary_meta = build_func_meta(passthrough_throw_auxiliary);

    vm.func_meta.emplace_back(resolve_func_meta);
    vm.func_meta.emplace_back(reject_func_meta);
    vm.func_meta.emplace_back(then_fulfilled_func_for_finally_meta);
    vm.func_meta.emplace_back(then_rejected_func_for_finally_meta);
    vm.func_meta.emplace_back(func_return_auxiliary_meta);
    vm.func_meta.emplace_back(func_throw_auxiliary_meta);
  }

  static Completion passthrough_return_auxiliary(vm_func_This_args_flags) {
    return func.as_func->this_or_auxiliary_data;
  }

  static Completion passthrough_throw_auxiliary(vm_func_This_args_flags) {
    return CompThrow(func.as_func->this_or_auxiliary_data);
  }

  static array<JSValue, 2> build_resolve_reject_function(NjsVM& vm, JSValueRef promise) {
    NOGC;
    auto *resolve_func = vm.new_function(resolve_func_meta);
    auto *reject_func = vm.new_function(reject_func_meta);
    resolve_func->has_auxiliary_data = true;
    reject_func->has_auxiliary_data = true;
    resolve_func->this_or_auxiliary_data = promise;
    reject_func->this_or_auxiliary_data = promise;

    return {JSValue(resolve_func), JSValue(reject_func)};
  }

  // get the `then` function from an object. for checking whether an object is `thenable`.
  static Completion get_then_function(NjsVM& vm, JSValueRef val) {
    if (val.is_object()) {
      return TRYCC(val.as_object->get_prop(vm, AtomPool::k_then));
    }
    return undefined;
  }

  // the arguments for the executor: `resolve` and `reject` function
  static Completion settling_function(vm_func_This_args_flags) {
    assert(func.as_func->has_auxiliary_data);
    assert(func.as_func->this_or_auxiliary_data.as_object->get_class() == CLS_PROMISE);

    JSValueRef arg = args.size() > 0 ? args[0] : undefined;
    auto new_state = static_cast<State>(func.as_func->meta->magic);
    return settling_internal(vm, func.as_func->this_or_auxiliary_data, new_state, arg);
  }

  static Completion settling_internal(NjsVM& vm, JSValueRef promise_val,
                                        State new_state, JSValueRef arg) {
    if (same_value(arg, promise_val)) {
      return vm.throw_error(JS_TYPE_ERROR, u"Promise self resolution detected");
    }

    // for resolve(thenable)
    if (new_state == FULFILLED) {
      if (JSValue then_func = TRYCC(get_then_function(vm, arg)); then_func.is_function()) {
        vector<JSValue> task_args { promise_val, arg, then_func };

        vm.micro_task_queue.push_back(JSTask {
            .use_native_func = true,
            .native_task_func = JSPromise::resolve_thenable,
            .args = std::move(task_args),
        });
        return undefined;
      }
    }

    promise_val.as_Object<JSPromise>()->settle(vm, new_state, arg);
    return undefined;
  }

  // if `resolve` is called on a thenable in the executor, use this method to call the
  // `then` method of the thenable (with `resolve` and `reject` callback as its argument)
  static Completion resolve_thenable(vm_func_This_args_flags) {
    assert(args.size() == 3);
    HANDLE_COLLECTOR;
    JSValueRef promise = args[0];
    JSValueRef thenable = args[1];
    JSValueRef then = args[2];

    auto resolve_reject = build_resolve_reject_function(vm, promise);
    gc_handle_add(resolve_reject[0]);
    gc_handle_add(resolve_reject[1]);

    auto comp = vm.call_function(then, thenable, undefined, {resolve_reject.data(), 2});
    gc_handle_add(comp.get_value());
    if (comp.is_throw()) {
      vm.call_function(resolve_reject[1], undefined, undefined, {&comp.get_value(), 1});
    }
    return undefined;
  }

  // for Promise.resolve or Promise.reject
  // TODO: figure out how to pass the `promise_ctor`
  static Completion Promise_resolve_reject(NjsVM& vm, JSValueRef promise_ctor,
                                           JSValue& arg, bool is_reject) {
    HANDLE_COLLECTOR;
    if (not is_reject && arg.is_object()) {
      // if the arg is a promise, directly return it
      if (object_class(arg) == ObjClass::CLS_PROMISE) {
        return arg;
      }
    }
    // else, the arg can be a thenable or other.
    // `settling_internal` will handle this properly.

    JSValue promise(vm.heap.new_object<JSPromise>(vm));
    gc_handle_add(promise);

    // this will not throw because arg cannot be a promise here.
    // (`settling_internal` will throw on promise self resolution)
    settling_internal(vm, promise, is_reject ? REJECTED : FULFILLED, arg);
    return promise;
  }

  static Completion then_callback_for_finally(vm_func_This_args_flags) {
    assert(func.as_func->has_auxiliary_data);
    HANDLE_COLLECTOR;
    JSValueRef on_finally = func.as_func->this_or_auxiliary_data;
    auto state = static_cast<State>(func.as_func->meta->magic);

    JSValue finally_res = TRYCC(vm.call_function(on_finally, undefined, {}));
    gc_handle_add(finally_res);
    JSValue promise = TRYCC(Promise_resolve_reject(vm, undefined, finally_res, false));
    gc_handle_add(promise);

    auto then_func_meta = state == FULFILLED ? func_return_auxiliary_meta : func_throw_auxiliary_meta;
    JSValue then_func(vm.new_function(then_func_meta));
    gc_handle_add(then_func);

    then_func.as_func->has_auxiliary_data = true;
    then_func.as_func->this_or_auxiliary_data = args[0];

    return promise.as_Object<JSPromise>()->then(vm, then_func, undefined);
  }

  /*
   * args[0]: is rejected or not
   * args[1]: then callback function
   * args[2]: the `resolve` function of the promise returned by `then`
   * args[3]: the `reject` function of the promise returned by `then`
   * args[4]: result of the promise
   */
  static Completion then_task(vm_func_This_args_flags) {
    HANDLE_COLLECTOR;
    assert(args.size() == 5);
    bool is_reject = args[0].as_bool;
    JSValue& callback = args[1];
    JSValue& next_resolve = args[2];
    JSValue& next_reject = args[3];
    JSValue& result = args[4];
    JSValue& promise = next_resolve.as_func->this_or_auxiliary_data;

    if (callback.is_function()) {
      auto comp = vm.call_function(callback, undefined, undefined, {&result, 1});
      JSValue& ret = comp.get_value();
      gc_handle_add(ret);
      if (not comp.is_throw()) [[likely]] {
        JSValue maybe_then = TRYCC(get_then_function(vm, ret));
        gc_handle_add(maybe_then);

        if (maybe_then.is_function()) [[unlikely]] {
          if (same_value(promise, ret)) [[unlikely]] {
            JSValue err = vm.build_error(JS_TYPE_ERROR,
                                         u"Chaining cycle detected for promise #<Promise>");
            vm.call_internal(next_reject, undefined, undefined, {&err, 1}, CallFlags());
          } else {
            TRYCC(vm.call_function(maybe_then, ret, undefined, args.subspan(2, 2)));
          }
        } else {
          // the return value of the callback is not a promise like
          vm.call_internal(next_resolve, undefined, undefined, {&ret, 1}, CallFlags());
        }
      } else {
        vm.call_internal(next_reject, undefined, undefined, {&ret, 1}, CallFlags());
      }
    } else {
      vm.call_internal(is_reject ? next_reject : next_resolve, undefined, undefined,
                       {&result, 1}, CallFlags());
    }

    return undefined;
  }

  /*
   * args[0]: then callback function
   * args[1]: result of the promise
   */
  static Completion then_task_no_relay(vm_func_This_args_flags) {
    assert(args.size() == 2);
    JSValue& callback = args[0];
    JSValue& result = args[1];

    if (callback.is_function()) {
      vm.call_function(callback, undefined, undefined, {&result, 1});
    }

    return undefined;
  }

  explicit JSPromise(NjsVM& vm): JSObject(vm, CLS_PROMISE, vm.promise_prototype) {}
  ~JSPromise() override {
    if (exec_state) {
      free(exec_state);
    }
  }

  void settle(NjsVM& vm, State s, JSValueRef data) {
    // can only settle once
    if (state != PENDING) return;
    state = s;
    result = data;
    gc_write_barrier(result);
    process_then(vm);
  }

  JSValue then(NjsVM& vm, JSValueRef on_fulfilled, JSValueRef on_rejected) {
    NOGC;
    JSValue promise(vm.heap.new_object<JSPromise>(vm));
    auto [resolve_func, reject_func] = build_resolve_reject_function(vm, promise);

    if (state == PENDING) {
      put_then_record(vm, on_fulfilled, on_rejected, resolve_func, reject_func, true);
    }
    else {
      assert(not then_record_using_inline && then_records.empty());
      vector<JSValue> args {
          JSValue(state == REJECTED),
          (state == REJECTED ? on_rejected : on_fulfilled),
          resolve_func,
          reject_func,
          result,
      };
      vm.micro_task_queue.push_back(JSTask {
          .use_native_func = true,
          .native_task_func = JSPromise::then_task,
          .args = std::move(args),
      });
    }

    return promise;
  }

  void then_internal(NjsVM& vm, JSValueRef on_fulfilled, JSValueRef on_rejected) {
    if (state == PENDING) {
      put_then_record(vm, on_fulfilled, on_rejected, undefined, undefined, false);
    }
    else {
      assert(not then_record_using_inline && then_records.empty());
      vector<JSValue> args {
          (state == REJECTED ? on_rejected : on_fulfilled),
          result,
      };
      vm.micro_task_queue.push_back(JSTask {
          .use_native_func = true,
          .native_task_func = then_task_no_relay,
          .args = std::move(args),
      });
    }
  }

  // `finally` is kind of a wrapper of `then` but behaves slightly differently.
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/finally
  // like
  //     promise.then(
  //      (value) => Promise.resolve(onFinally()).then(() => value),
  //      (reason) =>
  //        Promise.resolve(onFinally()).then(() => {
  //          throw reason;
  //        }),
  //)    ;
  JSValue finally(NjsVM& vm, JSValueRef on_finally) {
    NOGC;
    if (on_finally.is_function()) {
      JSFunction *fulfill = vm.new_function(then_fulfilled_func_for_finally_meta);
      JSFunction *reject = vm.new_function(then_rejected_func_for_finally_meta);
      fulfill->has_auxiliary_data = true;
      reject->has_auxiliary_data = true;
      fulfill->this_or_auxiliary_data = on_finally;
      reject->this_or_auxiliary_data = on_finally;
      JSValue on_fulfilled(fulfill);
      JSValue on_rejected(reject);

      return then(vm, on_fulfilled, on_rejected);
    } else {
      return then(vm, on_finally, on_finally);
    }
  }

  void process_then(NjsVM& vm) {
    if (state == PENDING) return;

    drain_then_records([&, this] (PromiseThenRecord& then_rec) {
      vector<JSValue> args;
      auto then_callback = state == REJECTED ? then_rec.on_rejected : then_rec.on_fulfilled;
      if (then_rec.then_relay) {
        args = {
            JSValue(state == REJECTED),
            then_callback,
            then_rec.next_resolve,
            then_rec.next_reject,
            result,
        };
      } else {
        args = {then_callback, result};
      }

      vm.micro_task_queue.push_back(JSTask {
          .use_native_func = true,
          .native_task_func = then_rec.then_relay ? then_task : then_task_no_relay,
          .args = std::move(args),
      });
    });
  }

  template <typename CB> requires std::invocable<CB, PromiseThenRecord&>
  void drain_then_records(CB callback) {
    if (then_record_using_inline) {
      callback(then_record_inline);
      then_record_inline.set_undefined();
      then_record_using_inline = false;
    }

    for (auto& rec : then_records) {
      callback(rec);
    }
    then_records.clear();
  }

  template <typename CB> requires std::invocable<CB, PromiseThenRecord&>
  void enumerate_then_records(CB callback) {
    if (then_record_using_inline) {
      callback(then_record_inline);
    }
    for (auto& rec : then_records) {
      callback(rec);
    }
  }

  void put_then_record(NjsVM& vm, JSValueRef on_fulfilled, JSValueRef on_rejected,
                       JSValueRef next_resolve, JSValueRef next_reject, bool then_relay) {
    if (not vm.heap.object_in_newgen(this)) {
      gc_write_barrier(on_fulfilled);
      gc_write_barrier(on_rejected);
      gc_write_barrier(next_resolve);
      gc_write_barrier(next_reject);
    }
    if (not then_record_using_inline && then_records.empty()) {
      then_record_using_inline = true;
      then_record_inline.then_relay = then_relay;
      then_record_inline.on_fulfilled = on_fulfilled;
      then_record_inline.on_rejected = on_rejected;
      then_record_inline.next_resolve = next_resolve;
      then_record_inline.next_reject = next_reject;
    } else {
      then_records.emplace_back(then_relay, on_fulfilled, on_rejected, next_resolve, next_reject);
    }
  }

  void dispose_exec_state() {
    assert(exec_state);
    free(exec_state);
    exec_state = nullptr;
  }

  bool gc_scan_children(GCHeap& heap) override {
    bool child_young = false;
    child_young |= JSObject::gc_scan_children(heap);
    gc_check_and_visit_object(child_young, result);
    enumerate_then_records([&] (PromiseThenRecord& rec) {
      child_young |= rec.gc_scan_children(heap);
    });
    if (exec_state) {
      child_young |= exec_state->gc_scan_children(heap);
    }
    return child_young;
  }

  void gc_mark_children() override {
    gc_check_and_mark_object(result);
    JSObject::gc_mark_children();
    enumerate_then_records([&] (PromiseThenRecord& rec) {
      rec.gc_mark_children();
    });
    if (exec_state) {
      exec_state->gc_mark_children();
    }
  }

  bool gc_has_young_child(GCObject *oldgen_start) override {
    if (JSObject::gc_has_young_child(oldgen_start)) return true;
    gc_check_object_young(result);
    if (then_record_inline.gc_has_young_child(oldgen_start)) return true;
    for (auto& rec : then_records) {
      if (rec.gc_has_young_child(oldgen_start)) return true;
    }
    if (exec_state && exec_state->gc_has_young_child(oldgen_start)) {
      return true;
    }
    return false;
  }

  u16string_view get_class_name() override {
    return u"Promise";
  }

  std::string description() override {
    return "Promise";
  }

  State state {PENDING};
  JSValue result;
  bool then_record_using_inline {false};
  PromiseThenRecord then_record_inline;
  vector<PromiseThenRecord> then_records;
  // for async function
  ResumableFuncState *exec_state {nullptr};
};

}

#endif // NJS_JS_PROMISE_H
