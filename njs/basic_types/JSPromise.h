#ifndef NJS_JS_PROMISE_H
#define NJS_JS_PROMISE_H

#include <cstdint>
#include <array>
#include "JSObject.h"
#include "testing_and_comparison.h"

namespace njs {

using u32 = uint32_t;
using std::array;
using std::vector;

class GCHeap;
class NjsVM;

struct PromiseThenRecord {
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

  enum State: uint8_t {
    // do not change the order or numeric value
    FULFILLED = 0,
    REJECTED,
    PENDING,
  };

  // TODO: pause GC
  static Completion New(NjsVM& vm, JSValueRef executor) {
    assert(executor.is_function());

    JSValue promise(new JSPromise(vm));
    auto resolve_reject = build_resolve_reject_func(vm, promise);

    Completion comp = vm.call_function(executor, undefined, undefined,
                                       {resolve_reject.data(), 2});
    if (comp.is_throw()) {
      promise_settling_function(vm, resolve_reject[1], undefined,
                                {&comp.get_value(), 1}, CallFlags());
    }

    return promise;
  }

  static array<JSValue, 2> build_resolve_reject_func(NjsVM& vm, JSValueRef promise) {
    auto *resolve_func = vm.new_function(resolve_func_meta);
    auto *reject_func = vm.new_function(reject_func_meta);
    resolve_func->has_auxiliary_data = true;
    reject_func->has_auxiliary_data = true;
    resolve_func->this_or_auxiliary_data = promise;
    reject_func->this_or_auxiliary_data = promise;

    return {JSValue(resolve_func), JSValue(reject_func)};
  }

  static Completion get_then_function(NjsVM& vm, JSValue val) {
    if (val.is_object()) {
      return TRYCC(val.as_object->get_prop(vm, AtomPool::k_then));
    }
    return undefined;
  }

  static Completion promise_settling_function(vm_func_This_args_flags) {
    assert(func.as_func->has_auxiliary_data);
    assert(func.as_func->this_or_auxiliary_data.as_object->get_class() == CLS_PROMISE);
    JSValueRef arg = args.size() > 0 ? args[0] : undefined;

    if (same_value(arg, func.as_func->this_or_auxiliary_data)) {
      return vm.throw_error(JS_TYPE_ERROR, u"Promise self resolution detected");
    }

    auto *promise = func.as_func->this_or_auxiliary_data.as_Object<JSPromise>();
    auto state = static_cast<State>(func.as_func->meta->magic);
    promise->settle(vm, state, arg);
    return undefined;
  }

  /*
   * args[0]: is rejected or not
   * args[1]: then callback function
   * args[2]: the `resolve` function of the promise returned by `then`
   * args[3]: the `reject` function of the promise returned by `then`
   * args[4]: result of the promise
   */
  static Completion promise_then_task(vm_func_This_args_flags) {
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
      if (not comp.is_throw()) [[likely]] {
        JSValue maybe_then = TRYCC(get_then_function(vm, ret));

        if (maybe_then.is_function()) [[unlikely]] {
          if (same_value(promise, ret)) [[unlikely]] {
            JSValue err = vm.build_error(JS_TYPE_ERROR,
                                         u"Chaining cycle detected for promise #<Promise>");
            vm.call_internal(next_reject, undefined, undefined, {&err, 1}, CallFlags());
          } else {
            TRYCC(vm.call_function(maybe_then, ret, undefined, args.subarray(2, 2)));
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

  explicit JSPromise(NjsVM& vm): JSObject(vm, CLS_PROMISE, vm.promise_prototype) {}

  void settle(NjsVM& vm, State s, JSValueRef data) {
    // can only settle once
    if (state != PENDING) return;
    state = s;
    result = data;
    WRITE_BARRIER(result);
    process_then(vm);
  }

  JSValue then(NjsVM& vm, JSValueRef on_fulfilled, JSValueRef on_rejected) {
    JSValue promise(new JSPromise(vm));
    auto [resolve_func, reject_func] = build_resolve_reject_func(vm, promise);

    if (state == PENDING) {
      put_then_record(vm, on_fulfilled, on_rejected, resolve_func, reject_func);
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
          .native_task_func = JSPromise::promise_then_task,
          .args = std::move(args),
      });
    }

    return promise;
  }

  void process_then(NjsVM& vm) {
    if (state == PENDING) return;

    drain_then_records([&, this] (PromiseThenRecord& then_rec) {
      vector<JSValue> args {
          JSValue(state == REJECTED),
          (state == REJECTED ? then_rec.on_rejected : then_rec.on_fulfilled),
          then_rec.next_resolve,
          then_rec.next_reject,
          result,
      };
      vm.micro_task_queue.push_back(JSTask {
          .use_native_func = true,
          .native_task_func = JSPromise::promise_then_task,
          .args = std::move(args),
      });
    });
  }

  template <typename CB> requires std::invocable<CB, PromiseThenRecord&>
  void drain_then_records(CB cb) {
    if (then_record_using_inline) {
      cb(then_record_inline);
      then_record_inline.set_undefined();
      then_record_using_inline = false;
    }

    for (auto& rec : then_records) {
      cb(rec);
    }
    then_records.clear();
  }

  template <typename CB> requires std::invocable<CB, PromiseThenRecord&>
  void enumerate_then_records(CB cb) {
    if (then_record_using_inline) {
      cb(then_record_inline);
    }
    for (auto& rec : then_records) {
      cb(rec);
    }
  }

  void put_then_record(NjsVM& vm, JSValueRef on_fulfilled, JSValueRef on_rejected,
                       JSValueRef next_resolve, JSValueRef next_reject) {
    if (not vm.heap.object_in_newgen(this)) {
      WRITE_BARRIER(on_fulfilled);
      WRITE_BARRIER(on_rejected);
      WRITE_BARRIER(next_resolve);
      WRITE_BARRIER(next_reject);
    }
    if (not then_record_using_inline && then_records.empty()) {
      then_record_using_inline = true;
      then_record_inline.on_fulfilled = on_fulfilled;
      then_record_inline.on_rejected = on_rejected;
      then_record_inline.next_resolve = next_resolve;
      then_record_inline.next_reject = next_reject;
    } else {
      then_records.push_back(PromiseThenRecord {
          on_fulfilled, on_rejected, next_resolve, next_reject
      });
    }
  }

  bool gc_scan_children(GCHeap& heap) override {
    bool child_young = false;
    gc_check_and_visit_object(child_young, result);
    enumerate_then_records([&] (PromiseThenRecord& rec) {
      child_young |= rec.gc_scan_children(heap);
    });
    return child_young;
  }

  void gc_mark_children() override {
    gc_check_and_mark_object(result);
    enumerate_then_records([&] (PromiseThenRecord& rec) {
      gc_check_and_mark_object(rec.on_fulfilled);
      gc_check_and_mark_object(rec.on_rejected);
      gc_check_and_mark_object(rec.next_resolve);
      gc_check_and_mark_object(rec.next_reject);
    });
  }

  bool gc_has_young_child(GCObject *oldgen_start) override {
    if (JSObject::gc_has_young_child(oldgen_start)) return true;

    if (then_record_inline.gc_has_young_child(oldgen_start)) return true;
    for (auto& rec : then_records) {
      if (rec.gc_has_young_child(oldgen_start)) return true;
    }
    gc_check_object_young(result);
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
};

}

#endif // NJS_JS_PROMISE_H
