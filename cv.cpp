#include "cv.h"
#include "myLib.h"

cv::cv(){
	impl_ptr = new impl();
}

cv::~cv(){
	delete impl_ptr;
}

void cv::wait(mutex& mutex1) {
	cpu::interrupt_disable();
	while (guard.exchange(true)) {}
	// unlock mutex1
	if (mutex1.impl_ptr->require_thread!=cpu::self()->impl_ptr->running_thread){
		guard.store(false);
		cpu::interrupt_enable();
		throw runtime_error("release a mutex it hasn't locked");
	}
	else{
		mutex1.impl_ptr->if_free = true;
		mutex1.impl_ptr->require_thread = NULL;
		if (! mutex1.impl_ptr->wait_on_lock.empty()) {
			thread::impl* successor = mutex1.impl_ptr->wait_on_lock.front();
			mutex1.impl_ptr->if_free = false;
			mutex1.impl_ptr->require_thread = successor;
			mutex1.impl_ptr->wait_on_lock.pop();
			readyQ.push(successor);
			check_and_send();
		}
	}
	
	// add itself to waiting list
	thread::impl* current_thread = cpu::self()->impl_ptr->running_thread;
    impl_ptr->wait_on_cv.push(current_thread);
    // go to sleep
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
	// after it wakes up, re-aquire the lock
	
	if (mutex1.impl_ptr->if_free) {
		mutex1.impl_ptr->if_free = false;
		mutex1.impl_ptr->require_thread = cpu::self()->impl_ptr->running_thread;
	}
	else {
		// add current_thread to waiting queue of the lock
    	thread::impl* current_thread = cpu::self()->impl_ptr->running_thread;
    	mutex1.impl_ptr->wait_on_lock.push(current_thread);
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

void cv::signal() {
	cpu::interrupt_disable();
	while (guard.exchange(true)) {}
	/* wake up one waiting thread on this cv if any */
	if (!impl_ptr->wait_on_cv.empty()) {
		thread::impl* wake_up = impl_ptr->wait_on_cv.front();
		impl_ptr->wait_on_cv.pop();
		readyQ.push(wake_up);
		check_and_send();
	}
	guard.store(false);
	cpu::interrupt_enable();
}

void cv::broadcast() {
	cpu::interrupt_disable();
	while (guard.exchange(true)) {}
	/* wake up all waiting threads on this cv if any */
	while (! impl_ptr->wait_on_cv.empty()) {
		thread::impl* wake_up = impl_ptr->wait_on_cv.front();
		impl_ptr->wait_on_cv.pop();
		readyQ.push(wake_up);
		check_and_send();
	}
	guard.store(false);
	cpu::interrupt_enable();
}

