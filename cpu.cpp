#include "cpu.h"
#include "myLib.h"

queue<thread::impl*> readyQ;
queue<thread::impl*> finishQ;
queue<cpu*> sleeping_cpu;

/* This function delete the context and stack dynamically allocated when thread is created */
void delete_context_and_stack(){
    while (!finishQ.empty()) {
        delete [](char* ) finishQ.front()->thread_cp->uc_stack.ss_sp;
        delete finishQ.front()->thread_cp;
        //if the object is not destroyed, point its impl_ptr to nullptr
        //delete thread impl_ptr
        thread::impl* victim = finishQ.front();
        delete victim;
        finishQ.pop();
    }
}

/* IPI handler is an empty function. When returns, it continues executing the code in the thread */
void IPI_handler(){

}


void cpu::init(thread_startfunc_t func, void * args) {
    /* For timer interrupt, it's just a yield function */
    interrupt_vector_table[0] = (interrupt_handler_t) thread::yield;
    interrupt_vector_table[1] = (interrupt_handler_t) IPI_handler;
    
    // Assertion for bad allocation of cpu context
    try{
    	impl_ptr = new impl();
        cpu::self()->impl_ptr->cp = new ucontext;
    }
    catch(bad_alloc ass){
        delete cpu::self()->impl_ptr->cp;
        cpu::interrupt_enable();
        throw bad_alloc();
    } 

    /* The function is not an empty function, it should also create a thread that executes func(arg). */
	if (func != nullptr) {
        //create a new thread
        cpu::interrupt_enable();
        thread t(func, args); 
        cpu::interrupt_disable();
    }

    while (1) {
        while (guard.exchange(true)){}
        /* If the readyQ is not empty, swap to next ready thread */
        if (!readyQ.empty()){
            thread::impl* next = readyQ.front();
            readyQ.pop();
            impl_ptr->running_thread = next;
            swapcontext(impl_ptr->cp, next->thread_cp);
            delete_context_and_stack();
            guard.store(false);
        }
        /* If there is no ready thread, let cpu sleep */
        else{
            impl_ptr->running_thread = NULL;
            // when the readyQ is empty, suspend the cpu
            sleeping_cpu.push(this);
            guard.store(false);
            cpu::interrupt_enable_suspend();
            cpu::interrupt_disable();
        }
    }

}



