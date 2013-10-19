/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <xenia/cpu/thread_state.h>

#include <xenia/core/memory.h>
#include <xenia/cpu/processor.h>


using namespace xe;
using namespace xe::cpu;


ThreadState::ThreadState(
    Processor* processor,
    uint32_t stack_size, uint32_t thread_state_address, uint32_t thread_id) :
    stack_size_(stack_size), thread_state_address_(thread_state_address),
    thread_id_(thread_id) {
  memory_ = processor->memory();

  stack_address_ = xe_memory_heap_alloc(memory_, 0, stack_size, 0);

  // Allocate with 64b alignment.
  ppc_state_ = (xe_ppc_state_t*)xe_malloc_aligned(sizeof(xe_ppc_state_t));
  XEASSERT(((uint64_t)ppc_state_ & 0xF) == 0);
  xe_zero_struct(ppc_state_, sizeof(xe_ppc_state_t));

  // Stash pointers to common structures that callbacks may need.
  ppc_state_->membase       = xe_memory_addr(memory_, 0);
  ppc_state_->processor     = processor;
  ppc_state_->thread_state  = this;

  // Set initial registers.
  ppc_state_->r[1]          = stack_address_ + stack_size;
  ppc_state_->r[13]         = thread_state_address_;
}

ThreadState::~ThreadState() {
  xe_free_aligned(ppc_state_);
  xe_memory_heap_free(memory_, stack_address_, 0);
  xe_memory_release(memory_);
}
