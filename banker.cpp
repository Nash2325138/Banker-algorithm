#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <iomanip>

#include <string>
#include <sstream>

#include <thread>
#include <mutex>

#include <chrono>
#include <vector>
#include <bitset>
#include "color.h"

#define NUM_CST 5
#define NUM_RSRC 3

int total[NUM_RSRC];
int available[NUM_RSRC];
int maximum[NUM_CST][NUM_RSRC];
int allocation[NUM_CST][NUM_RSRC];
int need[NUM_CST][NUM_RSRC];

std::mutex mtx;

int request_resources(int customer_id, int request[]);
int release_resources(int customer_id, int release[]);
void release_all(int customer_id);
bool safety_algo(int customer_id, int request[]);

void process_create(int customer_id);
void initialize(char const *argv[]);

void fprint_snapshot(FILE* fp);
inline char transChar(int i) {
	return (i < 26) ? 'A'+i : 'a'+i-26;
}
void put_table(const char* descryption, int table[NUM_CST][NUM_RSRC]);
FILE* out_fp;
int main(int argc, char const *argv[])
{
	// see if the argument number is correct
	if (argc != NUM_RSRC + 1) {
		fprintf(stderr, "Usage: ./executable\n");
		for (int i = 0; i < NUM_RSRC; ++i) {
			fprintf(stderr, " <type_%d_resource_num>", i);
		}
		putchar('\n');
		exit(EXIT_FAILURE);
	}

	out_fp = fopen("log.txt", "w");
	srand(time(NULL));
	initialize(argv);
	fprint_snapshot(stdout);
	fprint_snapshot(out_fp);

	std::vector<std::thread> threads;
	for (int i = 0; i < NUM_CST; ++i) {
		threads.emplace_back(process_create, i);
	}
	for (std::thread& th : threads) {
		th.join();
	}

	printf(WHITE "All processes finished their jobs, you can refer to log.txt to see all the details\n" NONE);
	return 0;
}

void process_create(int customer_id) {
	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(rand()%10));
		if (rand()%2) {
			int request[NUM_RSRC];
			int sum = 0;
			for (int i = 0; i < NUM_RSRC; ++i) {
				request[i] = rand() % (need[customer_id][i]+1);
				sum += request[i];
			}
			if (sum == 0) continue;
			while (request_resources(customer_id, request) != 0) {
				// if request fail, repeat the same requesting
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
			}
			
			bool enough = true;
			for (int i = 0; i < NUM_RSRC; ++i) {
				if (need[customer_id][i] > 0) {
					enough = false;
					break;
				}
			}
			if (enough) {
				printf(LIGHT_GREEN "--[V]-- Process %d has finished !!!\n" NONE, customer_id);
				release_all(customer_id);
				return;
			}
		} 
		// since we don't need to do random release anymore, the code below is abandoned.
		else {
			// int release[NUM_RSRC];
			// int sum = 0;
			// for (int i = 0; i < NUM_RSRC; ++i) {
			// 	release[i] = rand() % (allocation[customer_id][i] + 1);
			// 	sum += release[i];
			// }
			// if (sum > 0) release_resources(customer_id, release);
		}
	}
}
void catInfo(std::string& info, int customer_id, int arr[], const char* desc) {
	std::stringstream ss;
	ss << desc << " of process " << customer_id << " -> (";
	for (int i = 0; i < NUM_RSRC; ++i) {
		if (i) ss << ",";
		ss << std::setw(4) << arr[i];
	}
	ss << ")";
	info += ss.str();
}

int request_resources(int customer_id, int request[]) {
	std::string info;
	catInfo(info, customer_id, request, "Requesting");

	// Since from now on, the available, allocation and need will be accessed to correctly
	// compute whether we can allow the request, those data should be locked
	mtx.lock();

	// banker's algorithm
	// 1. See if the process's request exceeds what it need 
	for (int i = 0; i < NUM_RSRC; ++i) {
		if (need[customer_id][i] < request[i]) {
			printf(RED "--[!]-- %s rejected: request exceeds what it needs\n" NONE, info.c_str());
			fprintf(out_fp, RED "--[!]-- %s rejected: request exceeds what it needs\n" NONE, info.c_str());
			fprint_snapshot(out_fp);
			mtx.unlock();
			return -1;
		}
	}
	// 2. See if there are enough abailable resources
	for (int i = 0; i < NUM_RSRC; ++i) {
		if (available[i] < request[i]) {
			printf(RED "--[!]-- %s rejected: no enough alailable resources\n" NONE, info.c_str());
			fprintf(out_fp, RED "--[!]-- %s rejected: no enough alailable resources\n" NONE, info.c_str());
			fprint_snapshot(out_fp);
			mtx.unlock();
			return -1;
		}
	}
	// 3. Execute safety algorithm to see if the request will make system unsafe

	// Assume that we allow the request
	for (int i = 0; i < NUM_RSRC; ++i) {
		allocation[customer_id][i] += request[i];
		need[customer_id][i] -= request[i];
		available[i] -= request[i];
	}
	// see if the system is in safe condition
	if (safety_algo(customer_id, request) == false) {
		// if the request makes system unsafe, reject it and take back the resouces.
		printf(RED "--[!]-- %s rejected: make the system unsafe.\n" NONE, info.c_str());
		fprintf(out_fp, RED "--[!]-- %s rejected: make the system unsafe.\n" NONE, info.c_str());
		for (int i = 0; i < NUM_RSRC; ++i) {
			allocation[customer_id][i] -= request[i];
			need[customer_id][i] += request[i];
			available[i] += request[i];
		}
		fprint_snapshot(out_fp);
		mtx.unlock();
		return -1;
	}

	// if it's safe to allow the request, no need to take back the resources.
	printf(CYAN "--[O]-- %s allowed\n" NONE, info.c_str());
	fprintf(out_fp, CYAN "--[O]-- %s allowed\n" NONE, info.c_str());
	fprint_snapshot(out_fp);
	mtx.unlock();
	return 0;
}
int release_resources(int customer_id, int release[]) {
	std::string info;
	catInfo(info, customer_id, release, "Releasing");
	printf(LIGHT_GREEN "--[+]-- %s\n", info.c_str());
	fprintf(out_fp, LIGHT_GREEN "--[+]-- %s\n", info.c_str());
	fprint_snapshot(out_fp);
	// release the resources 
	mtx.lock();
	for (int i = 0; i < NUM_RSRC; ++i) {
		allocation[customer_id][i] -= release[i];
		need[customer_id][i] += release[i];
		available[i] += release[i];
	}
	mtx.unlock();
	return 0;
}
void release_all(int customer_id) {
	// Release all resources the process have, using when a process get enough resources to finish its job. 
	int release[NUM_RSRC];
	for (int i = 0; i < NUM_RSRC; ++i) {
		release[i] = allocation[customer_id][i];
	}
	release_resources(customer_id, release);
	return;
}

bool safety_algo(int customer_id, int request[]) {
	int work[NUM_RSRC];
	std::bitset<NUM_CST> finish;

	// Initailization work and finish
	for (int i = 0; i < NUM_RSRC; ++i) {
		work[i] = available[i];
	}
	finish.reset();

	while (true) {
		// find a process such its need < work and haven't finished
		bool found = false;
		for (int i = 0; i < NUM_CST; ++i) {
			// test those have not finished
			if (finish[i]) continue;

			// if find a process that can finish, assign it to finish and release its allocation.
			int canFinish = true;
			for (int j = 0; j < NUM_RSRC; ++j) {
				if (need[i][j] > work[j]) {
					canFinish = false;
					break;
				}
			}
			if (canFinish) {
				for (int j = 0; j<NUM_RSRC; ++j) {
					work[j] += allocation[i][j];
				}
				finish.set(i);
				found = true;
				break;
			}
		}
		if (not found) break;;
	}

	// when no process such it's need < work and haven't finished, see if all processes are finished
	if (finish.count() == NUM_CST) {
		return true;
	} else {
		return false;
	}
}
void initialize(char const *argv[])
{
	for (int i = 0; i < NUM_RSRC; ++i) {
		total[i] = available[i] = std::stoi(std::string(argv[i+1]), NULL, 10);
	}
	for (int i = 0; i < NUM_CST; ++i) {
		for (int j = 0; j < NUM_RSRC; ++j) {
			allocation[i][j] = 0;
			need[i][j] = maximum[i][j] = (rand() % available[j]) + 1;
		}
	}
}

char* putRsrcName(char* target) {
	for (int i = 0; i < NUM_RSRC; ++i) {
		target += sprintf(target, "%4c", transChar(i));
	}
	target += sprintf(target, "\n");
	return target;
}
char* put_table(const char* descryption, int table[NUM_CST][NUM_RSRC], char* buffer) {
	char* target = buffer;
	target += sprintf(target, YELLOW "%s:\n     " NONE, descryption);
	target = putRsrcName(target);
	for (int i = 0; i < NUM_CST; ++i) {
		target += sprintf(target, "P%-4d", i);
		for (int j = 0; j < NUM_RSRC; j++) {
			target += sprintf(target, "%4d", table[i][j]);
		}
		target += sprintf(target, "\n");
	}
	return target;
}
char* put_rescouse(const char* desc, int resources[NUM_RSRC], char* buffer) {
	char* target = buffer;
	target += sprintf(target, YELLOW "%s system resources are:\n" NONE, desc);
	target = putRsrcName(target);
	for (int i = 0; i < NUM_RSRC; ++i) {
		target += sprintf(target, "%4d", resources[i]);
	}
	target += sprintf(target, "\n");
	return target;
}
void fprint_snapshot(FILE* fp)
{
	char buffer[100000];
	char* target = buffer;
	target = put_rescouse("Total", total, target);
	target = put_rescouse("Available", available, target);
	target = put_table("Currently allocated resources", allocation, target);
	target = put_table("Maximum resources", maximum, target);
	target = put_table("Possibly needed resources", need, target);
	fprintf(fp, "%s\n", buffer);
}