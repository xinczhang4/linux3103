
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
using namespace std;

#define MAX_LOOP 1000

struct service_t {
	string type;	// C (cpu), D (disk), K (keyboard), LU(mutex)
	int time_cost;
	string mutex_name;
	service_t() : type(""), time_cost(-1) {}
	service_t(string type, string desc) : type(type) {
		if (type == "L" or type == "U") {
			this->time_cost = 0;
			mutex_name = desc;
		}
		else
			this->time_cost = stoi(desc);
	}
};


struct process_t
{
	int process_id;
	int arrival_time;
	vector<service_t> service_seq;
	service_t cur_service;
	int cur_service_idx;
	int cur_service_tick;	// num of ticks that has been spent on current service
	vector<int> working;	// working sequence on CPU, for loging output
	int ready_queue_idx;	//index of the ready queue in FB
	int time_quantum;	//time quantum for clock interrupt

	// Call when current service completed
	// if there are no service left, return true. Otherwise, return false
	bool proceed_to_next_service()
	{
		this->cur_service_idx++;
		this->cur_service_tick = 0;
		if (this->cur_service_idx >= this->service_seq.size()) {	// all services are done, process should end
			return true;
		}
		else {		// still requests services
			this->cur_service = this->service_seq[this->cur_service_idx];
			return false;
		}
	};

	// Log the working ticks on CPU (from `start_tick` to `end_tick`)
	void log_working(int start_tick, int end_tick)
	{
		this->working.push_back(start_tick);
		this->working.push_back(end_tick);
	};
};

struct mutex_t {
	string name;
	bool islocked;
	void lock() {
		islocked = true;
	};
	void unlock() {
		islocked = false;
	};
};


// write output log
int write_file(vector<process_t> processes, const char* file_path)
{
	ofstream outputfile;
	outputfile.open(file_path);
	for (vector<process_t>::iterator p_iter = processes.begin(); p_iter != processes.end(); p_iter++) {
		outputfile << "process " << p_iter->process_id << endl;
		for (vector<int>::iterator w_iter = p_iter->working.begin(); w_iter != p_iter->working.end(); w_iter++) {
			outputfile << *w_iter << " ";
		}
		outputfile << endl;
	}
	outputfile.close();
	return 0;
}

// Split a string according to a delimiter
void split(const string& s, vector<string>& tokens, const string& delim = " ")
{
	string::size_type last_pos = s.find_first_not_of(delim, 0);
	string::size_type pos = s.find_first_of(delim, last_pos);
	while (string::npos != pos || string::npos != last_pos) {
		tokens.push_back(s.substr(last_pos, pos - last_pos));
		last_pos = s.find_first_not_of(delim, pos);
		pos = s.find_first_of(delim, last_pos);
	}
}

vector<process_t> read_processes(const char* file_path)
{
	vector<process_t> process_queue;
	ifstream file(file_path);
	string str;
	while (getline(file, str)) {
		process_t new_process;
		stringstream ss(str);
		int service_num;
		char syntax;
		ss >> syntax >> new_process.process_id >> new_process.arrival_time >> service_num;
		for (int i = 0; i < service_num; i++) {	// read services sequence
			getline(file, str);
			str = str.erase(str.find_last_not_of(" \n\r\t") + 1);
			vector<string> tokens;
			split(str, tokens, " ");
			service_t ser(tokens[0], tokens[1]);
			new_process.service_seq.push_back(ser);
		}
		new_process.cur_service_idx = 0;
		new_process.cur_service_tick = 0;
		new_process.cur_service = new_process.service_seq[new_process.cur_service_idx];
		process_queue.push_back(new_process);
	}
	return process_queue;
}

// move the process at the front of q1 to the back of q2 (q1 head -> q2 tail)
int move_process_from(vector<process_t>& q1, vector<process_t>& q2)
{
	if (!q1.empty()) {
		process_t& tmp = q1.front();
		q2.push_back(tmp);
		q1.erase(q1.begin());
		return 1;
	}
	return 0;
}

//FCFS 
int fcfs(vector<process_t> processes, const char* output_path)
{
	vector<process_t> ready_queue;
	vector<process_t> disk_block_queue;
	vector<process_t> keyboard_block_queue;
	vector<process_t> mutex_block_queue;
	vector<process_t> processes_done;
	mutex_t mutex;
	mutex.islocked = false;

	int complete_num = 0;
	int dispatched_tick = 0;
	int cur_process_id = -1, prev_process_id = -1;

	// main loop
	for (int cur_tick = 0; cur_tick < MAX_LOOP; cur_tick++) {
		// long term scheduler
		for (int i = 0; i < processes.size(); i++) {
			if (processes[i].arrival_time == cur_tick) {		// process arrives at current tick
				ready_queue.push_back(processes[i]);
			}
		}
		// Disk I/O device scheduling
		if (!disk_block_queue.empty()) {
			process_t& cur_io_process = disk_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				move_process_from(disk_block_queue, ready_queue);
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}
		// Keyboard I/O device scheduling
		if (!keyboard_block_queue.empty()) {
			process_t& cur_io_process = keyboard_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				move_process_from(keyboard_block_queue, ready_queue);
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}
		// Mutex scheduling
		if (!mutex_block_queue.empty()) {
			process_t& cur_mutex_process = mutex_block_queue.front();	// always provide service to the first process in block queue

			if (!mutex.islocked) {	//lock the mutex if it is not locked
				mutex.lock();
				cur_mutex_process.proceed_to_next_service();

				if (cur_mutex_process.cur_service.type == "D") {
					move_process_from(mutex_block_queue, disk_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "K") {
					move_process_from(mutex_block_queue, keyboard_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "C") {
					move_process_from(mutex_block_queue, ready_queue);
				}
				else if (cur_mutex_process.cur_service.type == "U") {
					mutex.unlock();
					move_process_from(mutex_block_queue, ready_queue);
				}				
			}
		}

		// CPU scheduling
		if (ready_queue.empty()) {	// no process for scheduling
			prev_process_id = -1;	// reset the previous dispatched process ID to empty
		}
		else {
			process_t& cur_process = ready_queue.front();	// always dispatch the first process in ready queue
			cur_process_id = cur_process.process_id;
			if (cur_process_id != prev_process_id) {		// store the tick when current process is dispatched
				dispatched_tick = cur_tick;
			}
			cur_process.cur_service_tick++; 	// increment the num of ticks that have been spent on current service
			if (cur_process.cur_service_tick >= cur_process.cur_service.time_cost) {	// current service is completed
				bool process_completed = cur_process.proceed_to_next_service();

				//check for mutex
				if (!process_completed && cur_process.cur_service.type == "U") {
					mutex.unlock();
					process_completed = cur_process.proceed_to_next_service();
				}

				if (!process_completed && cur_process.cur_service.type == "L") {
					if (!mutex.islocked) {
						mutex.lock();
						process_completed = cur_process.proceed_to_next_service();
					}
					else {
						cur_process.log_working(dispatched_tick, cur_tick + 1);
						move_process_from(ready_queue, mutex_block_queue);
						continue;
					}
				}

				if (process_completed) {		// the whole process is completed
					complete_num++;
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					move_process_from(ready_queue, processes_done);		// remove current process from ready queue
				}
				else if (cur_process.cur_service.type == "D") {		// next service is disk I/O, block current process
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					move_process_from(ready_queue, disk_block_queue);
				}
				else if (cur_process.cur_service.type == "K") {		// next service is keyboard I/O, block current process
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					move_process_from(ready_queue, keyboard_block_queue);
				}
			}
			prev_process_id = cur_process_id;	// log the previous dispatched process ID
		}
		if (complete_num == processes.size()) {	// all process completed
			break;
		}
	}
	write_file(processes_done, output_path);	// write output
	return 1;
}

int rr(vector<process_t> processes, const char* output_path) {
	vector<process_t> ready_queue;
	vector<process_t> disk_block_queue;
	vector<process_t> keyboard_block_queue;
	vector<process_t> mutex_block_queue;
	vector<process_t> processes_done;
	mutex_t mutex;
	mutex.islocked = false;

	int complete_num = 0;
	int dispatched_tick = 0;
	int cur_process_id = -1, prev_process_id = -1;

	for (int cur_tick = 0; cur_tick < MAX_LOOP; cur_tick++) {
		//long time scheduler
		for (int i = 0; i < processes.size(); i++) {
			if (processes[i].arrival_time == cur_tick) {		// process arrives at current tick
				ready_queue.push_back(processes[i]);
			}
		}

		//Disk I/O device scheduling
		if (!disk_block_queue.empty()) {
			process_t& cur_io_process = disk_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				move_process_from(disk_block_queue, ready_queue);
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}

		// Keyboard I/O device scheduling
		if (!keyboard_block_queue.empty()) {
			process_t& cur_io_process = keyboard_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				move_process_from(keyboard_block_queue, ready_queue);
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}
		// Mutex scheduling
		if (!mutex_block_queue.empty()) {
			process_t& cur_mutex_process = mutex_block_queue.front();	// always provide service to the first process in block queue
			if (!mutex.islocked) {		//lock the mutex if it is not locked
				mutex.lock();
				cur_mutex_process.proceed_to_next_service();

				if (cur_mutex_process.cur_service.type == "D") {
					move_process_from(mutex_block_queue, disk_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "K") {
					move_process_from(mutex_block_queue, keyboard_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "C") {
					move_process_from(mutex_block_queue, ready_queue);
				}
				else if (cur_mutex_process.cur_service.type == "U") {
					mutex.unlock();
					move_process_from(mutex_block_queue, ready_queue);
				}

			}
		}

		// CPU scheduling
		if (ready_queue.empty()) {	// no process for scheduling
			prev_process_id = -1;	// reset the previous dispatched process ID to empty
		}
		else {

			process_t& cur_process = ready_queue.front();

			cur_process_id = cur_process.process_id;
			if (cur_process_id != prev_process_id || cur_process.time_quantum ==0) {		// store the tick when current process is dispatched
				dispatched_tick = cur_tick;
				cur_process.time_quantum = 0;
			}
			cur_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
			cur_process.time_quantum++;		//increment the service tick
			
			if (cur_process.time_quantum == 5 && cur_process.cur_service_tick < cur_process.cur_service.time_cost) {	//clock interrupt
				cur_process.time_quantum = 0;
				cur_process.log_working(dispatched_tick, cur_tick + 1);
				move_process_from(ready_queue, ready_queue);
			}
			if (cur_process.time_quantum <= 5 && cur_process.cur_service_tick >= cur_process.cur_service.time_cost) {	// current service is completed

				bool process_completed = cur_process.proceed_to_next_service();


				if (!process_completed && cur_process.cur_service.type == "U") {
					mutex.unlock();
					process_completed = cur_process.proceed_to_next_service();
				}

				if (!process_completed && cur_process.cur_service.type == "L") {
					if (!mutex.islocked) {
						mutex.lock();
						process_completed = cur_process.proceed_to_next_service();
					}
					else {
						cur_process.time_quantum = 0;	//reset the sevice tick to 0
						cur_process.log_working(dispatched_tick, cur_tick + 1);//process 1234 ��ǰ���� 
						move_process_from(ready_queue, mutex_block_queue);
						continue;
					}
				}

				if (process_completed) {		// the whole process is completed
					complete_num++;
					cur_process.log_working(dispatched_tick, cur_tick + 1);//����process������ 

					// remove current process from ready queue
					move_process_from(ready_queue, processes_done);
				}
				else if (cur_process.cur_service.type == "D") {		// next service is disk I/O, block current process
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					move_process_from(ready_queue, disk_block_queue);
				}
				else if (cur_process.cur_service.type == "K") {		// next service is keyboard I/O, block current process
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					move_process_from(ready_queue, keyboard_block_queue);
				}
				else if (cur_process.cur_service.type == "C" && cur_process.time_quantum == 5) {
					cout << "flag" << endl;
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1); //0 5 5 10 ��û�� 
					move_process_from(ready_queue, ready_queue);
				}

			}
			prev_process_id = cur_process_id;	// log the previous dispatched process ID

		}
		if (complete_num == processes.size()) {	// all process completed
			break;
		}
	}
	write_file(processes_done, output_path);	// write output
	return 1;
}

process_t& get_first_process(vector<process_t>& RQ0, vector<process_t>& RQ1, vector<process_t>& RQ2) {
	if (!RQ0.empty()) {
		return RQ0.front();
	}
	else if (!RQ1.empty()) {
		return RQ1.front();
	}
	else {
		return RQ2.front();
	}
}

int fb(vector<process_t> processes, const char* output_path) {

	vector<process_t> RQ0;
	vector<process_t> RQ1;
	vector<process_t> RQ2;
	vector<process_t> disk_block_queue;
	vector<process_t> keyboard_block_queue;
	vector<process_t> mutex_block_queue;
	vector<process_t> processes_done;
	mutex_t mutex;
	mutex.islocked = false;
	process_t init;

	int complete_num = 0;
	int dispatched_tick = 0;
	int service_tick = 0;
	int cur_process_id = -1, prev_process_id = -1;

	for (int cur_tick = 0; cur_tick < MAX_LOOP; cur_tick++) {

		//insert new process in RQ0
		for (int i = 0; i < processes.size(); i++) {
			if (processes[i].arrival_time == cur_tick) {		// process arrives at current tick
				RQ0.push_back(processes[i]);
				process_t& cur = RQ0.back();
				cur.ready_queue_idx = 0;
			}
		}

		//Disk I/O device scheduling
		if (!disk_block_queue.empty()) {
			process_t& cur_io_process = disk_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				if (cur_io_process.ready_queue_idx == 0) {
					move_process_from(disk_block_queue, RQ0);
				}
				else if (cur_io_process.ready_queue_idx == 1) {
					move_process_from(disk_block_queue, RQ1);
				}
				else {
					move_process_from(disk_block_queue, RQ2);
				}
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}

		// Keyboard I/O device scheduling
		if (!keyboard_block_queue.empty()) {
			process_t& cur_io_process = keyboard_block_queue.front();	// always provide service to the first process in block queue
			if (cur_io_process.cur_service_tick >= cur_io_process.cur_service.time_cost) {	// I/O service is completed
				cur_io_process.proceed_to_next_service();
				if (cur_io_process.ready_queue_idx == 0) {
					move_process_from(keyboard_block_queue, RQ0);
				}
				else if (cur_io_process.ready_queue_idx == 1) {
					move_process_from(keyboard_block_queue,RQ1);
				}
				else {
					move_process_from(keyboard_block_queue, RQ2);
				}
			}
			cur_io_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
		}
		// Mutex scheduling
		if (!mutex_block_queue.empty()) {
			process_t& cur_mutex_process = mutex_block_queue.front();	// always provide service to the first process in block queue
			if (!mutex.islocked) {
				mutex.lock();
				cur_mutex_process.proceed_to_next_service();

				if (cur_mutex_process.cur_service.type == "D") {
					move_process_from(mutex_block_queue, disk_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "K") {
					move_process_from(mutex_block_queue, keyboard_block_queue);
				}
				else if (cur_mutex_process.cur_service.type == "C") {
					if (cur_mutex_process.ready_queue_idx == 0) {
						move_process_from(mutex_block_queue, RQ0);
					}
					else if (cur_mutex_process.ready_queue_idx == 1) {
						move_process_from(mutex_block_queue, RQ1);
					}
					else {
						move_process_from(mutex_block_queue, RQ2);
					}
				}
				else if (cur_mutex_process.cur_service.type == "U") {
					mutex.unlock();
					if (cur_mutex_process.ready_queue_idx == 0) {
						move_process_from(mutex_block_queue, RQ0);
					}
					else if (cur_mutex_process.ready_queue_idx == 1) {
						move_process_from(mutex_block_queue, RQ1);
					}
					else {
						move_process_from(mutex_block_queue, RQ2);
					}
				}

			}
		}

		// CPU scheduling
		if (RQ0.empty() && RQ1.empty() && RQ2.empty()) {	// no process for scheduling
			prev_process_id = -1;	// reset the previous dispatched process ID to empty
		}
		else {
			process_t& cur_process = get_first_process(RQ0,RQ1,RQ2);

			cur_process_id = cur_process.process_id;
			if (cur_process_id != prev_process_id) {		// store the tick when current process is dispatched
				dispatched_tick = cur_tick;
				cur_process.time_quantum = 0;
			}
			cur_process.cur_service_tick++;		// increment the num of ticks that have been spent on current service
			cur_process.time_quantum++;		//increment the service tick

			if (cur_process.time_quantum == 5 && cur_process.cur_service_tick < cur_process.cur_service.time_cost) {	//demote the process that up its quantum time
				cur_process.time_quantum = 0;
				cur_process.log_working(dispatched_tick, cur_tick + 1);
				if (cur_process.ready_queue_idx == 0) {
					cur_process.ready_queue_idx = 1;
					move_process_from(RQ0, RQ1);
				}
				else if (cur_process.ready_queue_idx == 1) {
					cur_process.ready_queue_idx = 2;
					move_process_from(RQ1, RQ2);
				}
			}
			if (cur_process.time_quantum <= 5 && cur_process.cur_service_tick >= cur_process.cur_service.time_cost) {	// current service is completed

				bool process_completed = cur_process.proceed_to_next_service();


				if (!process_completed && cur_process.cur_service.type == "U") {
					mutex.unlock();
					process_completed = cur_process.proceed_to_next_service();
				}

				if (!process_completed && cur_process.cur_service.type == "L") {
					if (!mutex.islocked) {
						mutex.lock();
						process_completed = cur_process.proceed_to_next_service();
					}
					else {
						cur_process.time_quantum = 0;	//reset the sevice tick to 0
						cur_process.log_working(dispatched_tick, cur_tick + 1);
						if (cur_process.ready_queue_idx == 0) {
							move_process_from(RQ0, mutex_block_queue);
						}
						else if (cur_process.ready_queue_idx == 1) {
							move_process_from(RQ1, mutex_block_queue);
						}
						else {
							move_process_from(RQ2, mutex_block_queue);
						}
						continue;
					}
				}

				if (process_completed) {		// the whole process is completed
					complete_num++;
					cur_process.log_working(dispatched_tick, cur_tick + 1);

					// remove current process from ready queue
					if (cur_process.ready_queue_idx == 0) {
						move_process_from(RQ0, processes_done);
					}
					else if (cur_process.ready_queue_idx == 1) {
						move_process_from(RQ1, processes_done);
					}
					else {
						move_process_from(RQ2, processes_done);
					}
				}
				else if (cur_process.cur_service.type == "D") {		// next service is disk I/O, block current process
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					if (cur_process.ready_queue_idx == 0) {
						move_process_from(RQ0, disk_block_queue);
					}
					else if (cur_process.ready_queue_idx == 1) {
						move_process_from(RQ1, disk_block_queue);
					}
					else {
						move_process_from(RQ2, disk_block_queue);
					}
				}
				else if (cur_process.cur_service.type == "K") {		// next service is keyboard I/O, block current process
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					if (cur_process.ready_queue_idx == 0) {
						move_process_from(RQ0, keyboard_block_queue);
					}
					else if (cur_process.ready_queue_idx == 1) {
						move_process_from(RQ1, keyboard_block_queue);
					}
					else {
						move_process_from(RQ2, keyboard_block_queue);
					}
				}
				else if (cur_process.cur_service.type == "C"&&cur_process.time_quantum==5) {
					cur_process.time_quantum = 0;	//reset the sevice tick to 0
					cur_process.log_working(dispatched_tick, cur_tick + 1);
					if (cur_process.ready_queue_idx == 0) {
						cur_process.ready_queue_idx = 1;
						move_process_from(RQ0, RQ1);
					}
					else if (cur_process.ready_queue_idx == 1) {
						cur_process.ready_queue_idx = 2;
						move_process_from(RQ1, RQ2);
					}
				}
				
			}
			prev_process_id = cur_process_id;	// log the previous dispatched process ID

		}
		if (complete_num == processes.size()) {	// all process completed
			break;
		}
	}
	write_file(processes_done, output_path);	// write output
	return 1;
}

int main(int argc, char* argv[])
{
	if (argc != 4) {
		cout << "Incorrect inputs: too few arguments" << endl;
		return 0;
	}
	const char* algorithm = argv[1];
	const char* process_path = argv[2];
	const char* output_path = argv[3];
	vector<process_t> process_queue = read_processes(process_path);

	if (strcmp(algorithm, "FCFS") == 0) {
		fcfs(process_queue, output_path);
	}
	else if (strcmp(algorithm, "FB") == 0) {
		fb(process_queue, output_path);
	}
	else if (strcmp(algorithm, "RR") == 0) {
		rr(process_queue, output_path);
	}
	else {
		cout << "Incorrect algorithm!" << endl;
	}
	return 0;
}
