#include "GCHeap.h"
#include "GCObject.h"

namespace njs {

void GCVisitor::do_visit(JSValue &handle, GCObject *obj) {
  GCObject *obj_new = heap.copy_object(obj);
  handle.val.as_ptr = obj_new;
}

} // namespace njs