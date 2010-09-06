
#ifdef USE_PRAGMA_IDENT_SRC
#pragma ident "@(#)gcTaskThread.cpp	1.12 03/12/23 16:40:09 JVM"
#endif
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * SUN PROPRIETARY/CONFIDENTIAL.  Use is subject to license terms.
 */

#include "incls/_precompiled.incl"
#include "incls/_gcTaskThread.cpp.incl"

GCTaskThread::GCTaskThread(GCTaskManager* manager,
                           uint           which,
                           uint           processor_id) :
  _manager(manager),
  _which(which),
  _processor_id(processor_id),
  _time_stamps(NULL),
  _time_stamp_index(0)
{
  bool created = os::create_thread(this, os::pgc_thread);
  assert(created, "failed to create thread");

  if (PrintGCTaskTimeStamps) {
    _time_stamps = NEW_C_HEAP_ARRAY(GCTaskTimeStamp, GCTaskTimeStampEntries );

    guarantee(_time_stamps != NULL, "Sanity");
  }
  set_name("GC task thread#%d (ParallelGC)", which);
}

GCTaskThread::~GCTaskThread() {
  if (_time_stamps != NULL) {
    FREE_C_HEAP_ARRAY(GCTaskTimeStamp, _time_stamps);
  }
  free_name();
}

void GCTaskThread::start() {
  os::start_thread(this);
}

GCTaskTimeStamp* GCTaskThread::time_stamp_at(uint index) {
  guarantee(index < GCTaskTimeStampEntries, "increase GCTaskTimeStampEntries");

  return &(_time_stamps[index]);
}

void GCTaskThread::print_task_time_stamps() {
  assert(PrintGCTaskTimeStamps, "Sanity");
  assert(_time_stamps != NULL, "Sanity (Probably set PrintGCTaskTimeStamps late)");

  tty->print_cr("GC-Thread %d entries: %d", _which, _time_stamp_index);
  for(uint i=0; i<_time_stamp_index; i++) {
    GCTaskTimeStamp* time_stamp = time_stamp_at(i);
    tty->print_cr("\t[ %s " INT64_FORMAT " " INT64_FORMAT " ]",
                  time_stamp->name(),
                  time_stamp->entry_time(),
                  time_stamp->exit_time());
  }

  // Reset after dumping the data
  _time_stamp_index = 0;
}

void GCTaskThread::print() const {
  tty->print("\"%s\" ", name());
  Thread::print();
  tty->cr();
}

void GCTaskThread::run() {
  // Set up the thread for stack overflow support
  this->record_stack_base_and_size();
  this->initialize_thread_local_storage();
  // Bind yourself to your processor.
  if (processor_id() != GCTaskManager::sentinel_worker()) {
    if (TraceGCTaskThread) {
      tty->print_cr("GCTaskThread::run: "
                    "  binding to processor %u", processor_id());
    }
    if (!os::bind_to_processor(processor_id())) {
      DEBUG_ONLY(
        warning("Couldn't bind GCTaskThread %u to processor %u",
                      which(), processor_id());
      )
    }
  }
  // Part of thread setup.
  // ??? Are these set up once here to make subsequent ones fast?
  HandleMark   hm_outer;
  ResourceMark rm_outer; 
 
  TimeStamp timer;

  for (;/* ever */;) {
    // These are so we can flush the resources allocated in the inner loop.
    HandleMark   hm_inner;
    ResourceMark rm_inner;
    for (; /* break */; ) {
      // This will block until there is a task to be gotten.
      GCTask* task = manager()->get_task(which());


      // In case the update is costly
      if (PrintGCTaskTimeStamps) {
        timer.update();
      }

      jlong entry_time = timer.ticks();
      char* name = task->name();

      task->do_it(manager(), which());
      manager()->note_completion(which());

      if (PrintGCTaskTimeStamps) {
        assert(_time_stamps != NULL, "Sanity (PrintGCTaskTimeStamps set late?)");

        timer.update();

        GCTaskTimeStamp* time_stamp = time_stamp_at(_time_stamp_index++);

        time_stamp->set_name(name);
        time_stamp->set_entry_time(entry_time);
        time_stamp->set_exit_time(timer.ticks());
      }

      // Check if we should release our inner resources.
      if (manager()->should_release_resources(which())) {
        manager()->note_release(which());
        break;
      }
    }
  }
}

