#include "Data.h"
#include "sgt.h"
#include <bits/stdc++.h>
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace std;

int n, m, numTrans, constVal, lambda;

std::mutex logLock;

int totalAborts = 0;
long double totalCommitDelay = 0;

ofstream logFile("log1.txt");
// SGT sgt;

void updtMemory() {
	int status = -1;
	int abortCnt = 0;
	std::default_random_engine generator;
	std::exponential_distribution<double> distribution(lambda);

	//   logLock.lock();
	//   std::cout << "came here\n";
	//   logLock.unlock();
	for (int curTrans = 0; curTrans < numTrans; ++curTrans) {
		abortCnt = 0;
		status = -1;
		std::chrono::time_point<std::chrono::high_resolution_clock> critStartTime,
			critEndTime;
		critStartTime = std::chrono::high_resolution_clock::now();
		int cnt = 0;
		while (1) {
			//   cout << "Transaction " << curTrans << " " << cnt++ << endl;
			Transaction id = sgt.begin_transaction();
			int randIters = max(rand() % (m + 1), 1); // Atleast one opeartion

			int locVal = 0;
			vector<int> visited(m, 0);
			bool abort = 0;

			for (int i = 0; i < randIters; ++i) {
			// cout << "Write Set size " << liveTrans[id].writeSet.size() <<endl;
			int randInd = rand() % m;
			int randVal = rand() % constVal;
			while (visited[randInd]) randInd = rand() % m; // To make sure same operation is not perfomed twice
			visited[randInd] = 1;

			if (!sgt.read(id, *sharedMem[randInd], locVal)) {
				abort = 1;
				break;
			}

			std::chrono::time_point<std::chrono::high_resolution_clock> readTime =
				std::chrono::high_resolution_clock::now();
			logLock.lock();
			time_t readTimeTT =
				std::chrono::high_resolution_clock::to_time_t(readTime);
			logFile << "Thread ID: " << std::this_thread::get_id()
					<< " Transaction " << id.id << " reads from " << randInd
					<< " a value " << locVal << " at time " << readTimeTT << endl;
			logLock.unlock();
			// cout << locVal << " " << locVal + randVal << endl;
			locVal += randVal;
			// cout << "Write Set size " << liveTrans[id].writeSet.size() << endl;
			if(!sgt.write(id, *sharedMem[randInd], locVal)) {
				abort = 1;
				break;
			}

			std::chrono::time_point<std::chrono::high_resolution_clock> writeTime =
				std::chrono::high_resolution_clock::now();
			logLock.lock();
			time_t writeTimeTT =
				std::chrono::high_resolution_clock::to_time_t(writeTime);
			logFile << "Thread ID: " << std::this_thread::get_id()
					<< " Transaction " << id.id << " writes to " << randInd
					<< " a value " << locVal << " at time " << writeTimeTT << endl;
			logLock.unlock();

			double randTime = distribution(generator);
			//
			std::this_thread::sleep_for(
				std::chrono::milliseconds((int)(1000 * randTime)));
			usleep(randTime);
		}
			// cout << "Commit!\n";
		if(!abort) {
			status = sgt.try_commit(id);
			cout << status << endl;
		}
		// cout << "Cool1\n";
		std::chrono::time_point<std::chrono::high_resolution_clock> commitTime =
			std::chrono::high_resolution_clock::now();
		logLock.lock();
		time_t commitTimeTT =
			std::chrono::high_resolution_clock::to_time_t(commitTime);
		logFile << "Thread ID: " << std::this_thread::get_id() << " Transaction "
				<< id.id << " tryCommits with result " << status << " at time"
				<< commitTimeTT << endl;
		logLock.unlock();
		// cout << "cool2\n";
		if (status == 0) {
		// cout << "okay1\n";
		++abortCnt;
		++totalAborts;
		// cout << "okay2\n";
		} else
		break;
		}
		// cout << "okay3\n";
		critEndTime = std::chrono::high_resolution_clock::now();
		// cout << "okay4\n";
		std::chrono::duration<double> commitDelay = critEndTime - critStartTime;
		// cout << "okay5\n";
		totalCommitDelay += commitDelay.count();
		// cout << "cool3\n";
	}
}

void init_Shared() {
  for (int i = 0; i < m; ++i) {
    sharedMem.push_back(new Data(0, i));
  }
}

int main() {
  ifstream input("inp_params.txt");

  input >> n >> m >> numTrans >> constVal >> lambda;
  cout << n << " " << m << " " << numTrans << " " << constVal << " " << lambda
       << endl;

  initSharedData(m);
  std::thread user[n];
  cout << sharedMem[rand() % m]->value << endl;
      // op_mutex = (std::shared_mutex *)malloc(m * sizeof(std::shared_mutex));

      for (int i = 0; i < n; ++i) {
    user[i] = std::thread(updtMemory);
  }

  for (int i = 0; i < n; ++i) {
    user[i].join();
  }
  cout << "Total number of aborts: " << totalAborts << endl;
  cout << "Average Commit Delay: " << totalCommitDelay / (n * numTrans) << endl;
  return 0;
}