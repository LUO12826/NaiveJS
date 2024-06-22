#include "JSFunctionMeta.h"

#include "njs/gc/GCHeap.h"
#include "njs/utils/macros.h"

namespace njs {

bool ResumableFuncState::gc_scan_children(GCHeap& heap) {
  bool child_young = false;
  gc_check_and_visit_object(child_young, This);
  JSValue *scan_end = active ? *stack_frame.sp_ref : stack_frame.sp;
  for (JSValue *val = stack_frame.buffer; val <= scan_end; val++) {
    gc_check_and_visit_object(child_young, *val);
  }
  return child_young;
}

void ResumableFuncState::gc_mark_children() {
  gc_check_and_mark_object(This);
  JSValue *scan_end = active ? *stack_frame.sp_ref : stack_frame.sp;
  for (JSValue *val = stack_frame.buffer; val <= scan_end; val++) {
    gc_check_and_mark_object(*val);
  }
}

bool ResumableFuncState::gc_has_young_child(GCObject *oldgen_start) {
  gc_check_object_young(This);
  JSValue *scan_end = active ? *stack_frame.sp_ref : stack_frame.sp;
  for (JSValue *val = stack_frame.buffer; val <= scan_end; val++) {
    gc_check_object_young(*val);
  }
  return false;
}

}