#ifndef MYLIB_H
#define MYLIB_H

#include "thread.h"
#include "cpu.h"

#include <iostream>
#include <ucontext.h>
#include <cstdlib>
#include <queue>
#include <vector>
#include <unordered_map>
#include <stdexcept>

using namespace std;

// ReadyQ stores ready threads
extern queue<thread::impl*> readyQ;

//finishQ stores finished threads (used for deleting the context of the thread)
extern queue<thread::impl*> finishQ;

// sleeping_cpu stores sleeping cpu when there is no ready threads
extern queue<cpu*> sleeping_cpu;

// Every time a thread is pushed to the readyQ, if there is any sleeping cpu, wake it up
extern void check_and_send();

// This function delete the context and stack dynamically allocated when thread is created 
extern void delete_context_and_stack();

// IPI_handler to deal with IPI
extern void IPI_handler();

class cpu::impl {

public:
    ucontext* cp; // the context of cpu::init it self
    thread::impl * running_thread; // the current thread running on the cpu

	impl() {
    };
	~impl() {
	};
	friend class cpu;
};

class thread::impl {

public:
    queue<thread::impl*> wait_to_join;  // the queue for waiting to join on queue
    ucontext* thread_cp; // the context of the thread
    thread* object_ptr; // used for deleting the thread in thread::join()

    impl() {

    };
    ~impl() {

    };
    friend class thread;
};

class mutex::impl {
    
public:
    queue<thread::impl*> wait_on_lock; // queue of threads waiting on this lock
    bool if_free = true; // If the lock is free
    thread::impl * require_thread = NULL; // The thread currently holding the lock

    impl() {};
    ~impl() {

    };
    friend class mutex;
};

class cv::impl {
    
public:
    queue<thread::impl*> wait_on_cv; //queue of threads waiting on this cv

    impl() {};    
    ~impl() {
        
    };
    friend class cv;
};

#endif


