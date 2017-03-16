#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <deque>
#include <climits>
#include <cmath>
#include "thread.h"

using namespace std;

mutex mutex1; // the lock of disk_queue
cv serviceCV; // service thread wakes up requester threads when disk_queue is not full
vector<cv> CVs;; // requester threads wake up service thread when disk_queue is full

struct issue
{
	int requesterID;
	int trackID;
};

vector<deque<int>> unissued_requests; // unissued requests in the requesters
vector<issue> disk_queue;
vector<bool> if_free_requester; // unresolved[requesterID] is true when there is an unresolved request of requester[requesterID] in disk_queue
vector<int> num_active; // num_active[requesterID] is the # of active(unserviced) requests from requester[requesterID]
int max_disk_queue;
int active_requester = 0; // total # of requesters that are still active

vector<int> free_set;


bool if_full(); // return if disk_queue is full
void requester_thread(void* id);
void service_thread(void* a);

bool if_full() {
	if (active_requester >= max_disk_queue) {
		return (disk_queue.size() == max_disk_queue);
	}
	else {
		return (disk_queue.size() == active_requester);
	}
}

void service_thread(void* a) {
	int num_requester = unissued_requests.size();

	// start all the requesters
	for (int i = 0; i < num_requester; i++) {
		thread temp ((thread_startfunc_t) requester_thread, (void *) i);
	}
	

	int current_track = 0;
	mutex1.lock();
	while (active_requester > 0) {
		while (! if_full()) {
			serviceCV.wait(mutex1); // wait for disk_queue to be full
		}

		// find next request to be serviced
		int to_be_serviced;
		int min_diff = INT_MAX;
		for (int i = 0; i < disk_queue.size(); i++) {
			if (abs(disk_queue[i].trackID - current_track) < min_diff) {
				min_diff = abs(disk_queue[i].trackID - current_track);
				to_be_serviced = i;
			}
		}

		// service a request
		int req_id = disk_queue[to_be_serviced].requesterID;
		int track_id = disk_queue[to_be_serviced].trackID;
		disk_queue.erase(disk_queue.begin() + to_be_serviced);
		cout << "service requester " << req_id << " track " << track_id << endl;
		current_track = track_id;
		

		// update num_active[], if_free_requester[], free_set and active_requester
		num_active[req_id]--;
		if (num_active[req_id] == 0) {
			active_requester--; // totol # of active_requester -1
			if (active_requester == 0) break;
		}
		else if (!unissued_requests[req_id].empty()) {
			if_free_requester[req_id] = true; // no unresolved request for requester(req_id) in disk_queue
			free_set.push_back(req_id);
		}


		if ( !if_full() && !free_set.empty() ) CVs[free_set.back()].signal(); // wake up a free requester thread

	}
	mutex1.unlock();

}

void requester_thread(void* id) {
	intptr_t requester_id = (intptr_t) id;
	mutex1.lock();
	while (num_active[requester_id] > 0) {
		while (if_full() || if_free_requester[requester_id] == false ) {
			CVs[requester_id].wait(mutex1); // wait for disk_queue to be not full
		}
		
		// issue a request
		int track_id = unissued_requests[requester_id].front();
		unissued_requests[requester_id].pop_front();
		cout << "requester " << requester_id << " track " << track_id << endl;
		issue temp = {requester_id, track_id};
		disk_queue.push_back(temp);

		// update if_free_requester[], free_set[]
		if_free_requester[requester_id] = false;
		for (int i = free_set.size()-1; i >= 0; i--) {
			if (free_set[i] == requester_id) {
				free_set.erase(free_set.begin() + i);
				break;
			}
		}
	
		if (if_full()) serviceCV.signal(); 
		
	}
	mutex1.unlock();
}

int main(int argc, char* argv[]) {
	max_disk_queue = atoi(argv[1]);

	//read input files into vector<deque<int>> unissued_requests;
	int num_requester = argc - 2;
	for (int i = 0; i < num_requester; i++) {
		ifstream inputFile;
		inputFile.open(argv[i+2]);
		deque<int> track;
		int temp;
		while (inputFile >> temp) {
			track.push_back(temp);
		}
		unissued_requests.push_back(track);
		inputFile.close();
	}

	// initialize CVs
	CVs.resize(num_requester);

	// initialize num_active[], if_free_requester[] and free_set and active_requester
	for (int i = 0; i < num_requester; i++) {
		num_active.push_back(unissued_requests[i].size());
		if (!unissued_requests[i].empty()) {
			free_set.push_back(i);
			if_free_requester.push_back(true);
			active_requester++;
		}
		else if_free_requester.push_back(false);
	}

	cpu::boot((thread_startfunc_t) service_thread, (void*) num_requester, 0);
}
