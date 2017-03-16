#include "mutex.h"
#include "myLib.h"

mutex::mutex(){
	impl_ptr = new impl();
}

mutex::~mutex(){
	delete impl_ptr;
}

void mutex::lock() {
	cpu::interrupt_disable();
	while (guard.exchange(true)) {}

	/* If lock is free, get the lock */
	if (impl_ptr->if_free) {
		impl_ptr->if_free = false;
		impl_ptr->require_thread = cpu::self()->impl_ptr->running_thread;
	}
	/* If lock is not free */
	else {
		// add current_thread to waiting queue of the lock
    	thread::impl* current_thread = cpu::self()->impl_ptr->running_thread;
    	impl_ptr->wait_on_lock.push(current_thread);
    	if (!readyQ.empty()) {
	    	// swap to readyQ front() 
	    	thread::impl* next_thread = readyQ.front();
	    	readyQ.pop();
	   	 	cpu::self()->impl_ptr->running_thread = next_thread;
	   	 	swapcontext(current_thread->thread_cp, next_thread->thread_cp);
		}
		else {
			// go back to cpu
			cpu::self()->impl_ptr->running_thread = nullptr;
			swapcontext(current_thread->thread_cp, cpu::self()->impl_ptr->cp);
		}
	}
	guard.store(false);
	cpu::interrupt_enable();
}

void mutex::unlock() {
	cpu::interrupt_disable();
	while (guard.exchange(true)) {}
	// Assertion: release a mutex it hasn't locked
	if (impl_ptr->require_thread != cpu::self()->impl_ptr->running_thread){
		guard.store(false);
		cpu::interrupt_enable();
		throw runtime_error("release a mutex it hasn't locked");
	}
	// Release the lock
	else{
		impl_ptr->if_free = true;
		impl_ptr->require_thread = NULL;
		if (! impl_ptr->wait_on_lock.empty()) {
			thread::impl* successor = impl_ptr->wait_on_lock.front();
			//Hand-off lock: set status to busy (if_free = false in this case)
			impl_ptr->if_free = false;
			impl_ptr->require_thread = successor;
			impl_ptr->wait_on_lock.pop();
			readyQ.push(successor);
			check_and_send();
		}
		guard.store(false);
		cpu::interrupt_enable();
		return;
	}
}

