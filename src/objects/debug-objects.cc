// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/debug-objects.h"
#include "src/objects/debug-objects-inl.h"

namespace v8 {
namespace internal {

// Check if there is a break point at this source position.
bool DebugInfo::HasBreakPoint(int source_position) {
  // Get the break point info object for this code offset.
  Object* break_point_info = GetBreakPointInfo(source_position);

  // If there is no break point info object or no break points in the break
  // point info object there is no break point at this code offset.
  if (break_point_info->IsUndefined(GetIsolate())) return false;
  return BreakPointInfo::cast(break_point_info)->GetBreakPointCount() > 0;
}

// Get the break point info object for this source position.
Object* DebugInfo::GetBreakPointInfo(int source_position) {
  Isolate* isolate = GetIsolate();
  if (!break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < break_points()->length(); i++) {
      if (!break_points()->get(i)->IsUndefined(isolate)) {
        BreakPointInfo* break_point_info =
            BreakPointInfo::cast(break_points()->get(i));
        if (break_point_info->source_position() == source_position) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->heap()->undefined_value();
}

bool DebugInfo::ClearBreakPoint(Handle<DebugInfo> debug_info,
                                Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  if (debug_info->break_points()->IsUndefined(isolate)) return false;

  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) continue;
    Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
        BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
    if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                            break_point_object)) {
      BreakPointInfo::ClearBreakPoint(break_point_info, break_point_object);
      return true;
    }
  }
  return false;
}

void DebugInfo::SetBreakPoint(Handle<DebugInfo> debug_info, int source_position,
                              Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  Handle<Object> break_point_info(
      debug_info->GetBreakPointInfo(source_position), isolate);
  if (!break_point_info->IsUndefined(isolate)) {
    BreakPointInfo::SetBreakPoint(
        Handle<BreakPointInfo>::cast(break_point_info), break_point_object);
    return;
  }

  // Adding a new break point for a code offset which did not have any
  // break points before. Try to find a free slot.
  static const int kNoBreakPointInfo = -1;
  int index = kNoBreakPointInfo;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) {
      index = i;
      break;
    }
  }
  if (index == kNoBreakPointInfo) {
    // No free slot - extend break point info array.
    Handle<FixedArray> old_break_points = Handle<FixedArray>(
        FixedArray::cast(debug_info->break_points()), isolate);
    Handle<FixedArray> new_break_points = isolate->factory()->NewFixedArray(
        old_break_points->length() +
        DebugInfo::kEstimatedNofBreakPointsInFunction);

    debug_info->set_break_points(*new_break_points);
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  DCHECK(index != kNoBreakPointInfo);

  // Allocate new BreakPointInfo object and set the break point.
  Handle<BreakPointInfo> new_break_point_info =
      isolate->factory()->NewBreakPointInfo(source_position);
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}

// Get the break point objects for a source position.
Handle<Object> DebugInfo::GetBreakPointObjects(int source_position) {
  Object* break_point_info = GetBreakPointInfo(source_position);
  Isolate* isolate = GetIsolate();
  if (break_point_info->IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return Handle<Object>(
      BreakPointInfo::cast(break_point_info)->break_point_objects(), isolate);
}

// Get the total number of break points.
int DebugInfo::GetBreakPointCount() {
  Isolate* isolate = GetIsolate();
  if (break_points()->IsUndefined(isolate)) return 0;
  int count = 0;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined(isolate)) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      count += break_point_info->GetBreakPointCount();
    }
  }
  return count;
}

Handle<Object> DebugInfo::FindBreakPointInfo(
    Handle<DebugInfo> debug_info, Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  if (!debug_info->break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < debug_info->break_points()->length(); i++) {
      if (!debug_info->break_points()->get(i)->IsUndefined(isolate)) {
        Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
            BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
        if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                                break_point_object)) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->factory()->undefined_value();
}

// Remove the specified break point object.
void BreakPointInfo::ClearBreakPoint(Handle<BreakPointInfo> break_point_info,
                                     Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) return;
  // If there is a single break point clear it if it is the same.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    if (break_point_info->break_point_objects() == *break_point_object) {
      break_point_info->set_break_point_objects(
          isolate->heap()->undefined_value());
    }
    return;
  }
  // If there are multiple break points shrink the array
  DCHECK(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array = Handle<FixedArray>(
      FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() - 1);
  int found_count = 0;
  for (int i = 0; i < old_array->length(); i++) {
    if (old_array->get(i) == *break_point_object) {
      DCHECK(found_count == 0);
      found_count++;
    } else {
      new_array->set(i - found_count, old_array->get(i));
    }
  }
  // If the break point was found in the list change it.
  if (found_count > 0) break_point_info->set_break_point_objects(*new_array);
}

// Add the specified break point object.
void BreakPointInfo::SetBreakPoint(Handle<BreakPointInfo> break_point_info,
                                   Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();

  // If there was no break point objects before just set it.
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
    break_point_info->set_break_point_objects(*break_point_object);
    return;
  }
  // If the break point object is the same as before just ignore.
  if (break_point_info->break_point_objects() == *break_point_object) return;
  // If there was one break point object before replace with array.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(2);
    array->set(0, break_point_info->break_point_objects());
    array->set(1, *break_point_object);
    break_point_info->set_break_point_objects(*array);
    return;
  }
  // If there was more than one break point before extend array.
  Handle<FixedArray> old_array = Handle<FixedArray>(
      FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() + 1);
  for (int i = 0; i < old_array->length(); i++) {
    // If the break point was there before just ignore.
    if (old_array->get(i) == *break_point_object) return;
    new_array->set(i, old_array->get(i));
  }
  // Add the new break point.
  new_array->set(old_array->length(), *break_point_object);
  break_point_info->set_break_point_objects(*new_array);
}

bool BreakPointInfo::HasBreakPointObject(
    Handle<BreakPointInfo> break_point_info,
    Handle<Object> break_point_object) {
  // No break point.
  Isolate* isolate = break_point_info->GetIsolate();
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
    return false;
  }
  // Single break point.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    return break_point_info->break_point_objects() == *break_point_object;
  }
  // Multiple break points.
  FixedArray* array = FixedArray::cast(break_point_info->break_point_objects());
  for (int i = 0; i < array->length(); i++) {
    if (array->get(i) == *break_point_object) {
      return true;
    }
  }
  return false;
}

// Get the number of break points.
int BreakPointInfo::GetBreakPointCount() {
  // No break point.
  if (break_point_objects()->IsUndefined(GetIsolate())) return 0;
  // Single break point.
  if (!break_point_objects()->IsFixedArray()) return 1;
  // Multiple break points.
  return FixedArray::cast(break_point_objects())->length();
}

}  // namespace internal
}  // namespace v8
