#include "thread.h"
#include "myLib.h"

/* 
   Every time a thread is pushed to the readyQ, check if there is sleeping cpu.
   If there is any sleeping cpu, wake up the sleeping cpu.
*/
void check_and_send(){
    if (!sleeping_cpu.empty()){
        sleeping_cpu.front()->interrupt_send();
        sleeping_cpu.pop();
    }
}

/* This function allows the thread to go back to cpu */
void big_func(thread_startfunc_t func, void* args) {
    // call func
    guard.store(false);
    cpu::interrupt_enable();
    func(args);
    cpu::interrupt_disable();
    while (guard.exchange(true)) {}

    delete_context_and_stack();
    thread::impl * current_thread = cpu::self()->impl_ptr->running_thread;
    //current_thread->finish = true;
    finishQ.push(current_thread);
    if (cpu::self()->impl_ptr->running_thread->object_ptr != nullptr) {
        cpu::self()->impl_ptr->running_thread->object_ptr->impl_ptr = nullptr;
    }
    // wake up the threads waiting for it  
    while (! current_thread->wait_to_join.empty()) {
        thread::impl * wake_up = current_thread->wait_to_join.front();
        current_thread->wait_to_join.pop();
        readyQ.push(wake_up);
        check_and_send();
    }
    
    
    // go to next ready thread
    if (!readyQ.empty()) {
        thread::impl* next_thread = readyQ.front();
        readyQ.pop();
        cpu::self()->impl_ptr->running_thread = next_thread;
        swapcontext(current_thread->thread_cp, next_thread->thread_cp);
    }

    //go back to cpu
    else {
        cpu::self()->impl_ptr->running_thread = nullptr;
        swapcontext(current_thread->thread_cp, cpu::self()->impl_ptr->cp);
    }
}

thread::thread(thread_startfunc_t func, void * args) {
    cpu::interrupt_disable();
    while (guard.exchange(true)) {}
    // Assertion: func is a null pointer
    if (func == nullptr) {
        guard.store(false);
        cpu::interrupt_enable();
        throw bad_alloc();
    }
    try{
        impl_ptr = new impl();
        impl_ptr->thread_cp = new ucontext;
        impl_ptr->object_ptr = this;        
        // make a new context for the new thread
        getcontext(impl_ptr->thread_cp);
        char *stack = new char [STACK_SIZE];

        impl_ptr->thread_cp->uc_stack.ss_sp = stack;
        impl_ptr->thread_cp->uc_stack.ss_size = STACK_SIZE;
        impl_ptr->thread_cp->uc_stack.ss_flags = 0;
        impl_ptr->thread_cp->uc_link = nullptr;
        makecontext(impl_ptr->thread_cp, (void (*)()) big_func, 2, func, args);

        // push the new_thread onto readyQ;
        readyQ.push(impl_ptr);
        check_and_send();
    }
    // Assertion: bad allocation of the context
    catch(bad_alloc ass){
        delete (char*) impl_ptr->thread_cp->uc_stack.ss_sp;
        delete impl_ptr->thread_cp;
        guard.store(false);
        cpu::interrupt_enable();
        throw bad_alloc();
    }
    guard.store(false);
    cpu::interrupt_enable();
}

void thread::yield() {
    cpu::interrupt_disable();
    while (guard.exchange(true)) {}
    // no ready threads
    if (readyQ.empty()){
        guard.store(false);
        cpu::interrupt_enable();
        return;
    }
    // ready threads avaiable
    else{
        // add itself to readyQ
        thread::impl* this_thread = cpu::self()->impl_ptr->running_thread;
        if (this_thread == nullptr){
            guard.store(false);
            cpu::interrupt_enable();
            return;
        }
        readyQ.push(this_thread);
        check_and_send();
        // swap to readyQ front()
        thread::impl* next_thread = readyQ.front();
        readyQ.pop();
        cpu::self()->impl_ptr->running_thread = next_thread;
        swapcontext(this_thread->thread_cp, next_thread->thread_cp);

        guard.store(false);
        cpu::interrupt_enable();
    }
}

thread::~thread(){
    // set object_ptr to nullptr
    if (impl_ptr != nullptr) impl_ptr->object_ptr = nullptr;
}

void thread::join() {
    cpu::interrupt_disable();
    while (guard.exchange(true)) {}
    // if thread complete, its impl_ptr will be nullptr, then return immediately
    if (impl_ptr == nullptr) {
        guard.store(false);
        cpu::interrupt_enable();
        return;
    }
    // if thread not complete
    thread::impl* current_thread = cpu::self()->impl_ptr->running_thread;
    // add current thread to joinQ
    impl_ptr->wait_to_join.push(current_thread);

    // if readyQ not empty, switch to next ready thread
    if (!readyQ.empty()) {
        thread::impl* next_thread = readyQ.front();
        readyQ.pop();
        cpu::self()->impl_ptr->running_thread = next_thread;
        swapcontext(current_thread->thread_cp, next_thread->thread_cp);
    }
    else { //else go back to cpu
        cpu::self()->impl_ptr->running_thread = nullptr;
        swapcontext(current_thread->thread_cp, cpu::self()->impl_ptr->cp);
    }
    guard.store(false);
    cpu::interrupt_enable();
}