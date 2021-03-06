// threadpool.cpp
// Compile with:
// g++ -std=c++11 -pthread threadpool.cpp -o threadpool
// (c) Daniël Zondervan // Thread pool pattern (busy-wait task problem) assignment #6 /// 03/08/2018
// (refactor)

#include "stdafx.h"
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <deque>
#include <sstream>

/*
* A thread pool is a group of pre-instantiated, idle worker threads which stand ready to be given work.
* These are preferred over instantiating new threads for each task when there is a large number of short tasks to be done rather than a small number of long ones.
* This prevents having to incur the overhead of creating a thread a large number of times.
*/

/*
* Solution:
*  1. Using atomics and a polling loop.
*  2. Using a condition_variable.
*
*  1. is very cpu heavy, 2. however is very cpu friendly.
*  The assignment is to implement 2.
*/

class thread_pool; // forward declare

using namespace std;

class worker {
public:
	explicit worker(thread_pool &s) : pool_(s) { }
	void operator()() const;
private:
	thread_pool & pool_;
};

class thread_pool {
public:
	explicit thread_pool(size_t threads);
	template<class F> void enqueue(F f);
	~thread_pool();
private:
	friend class worker;

	mutex m_;
	condition_variable cv_;

	vector<thread> workers_;
	deque<function<void()>> tasks_;

	mutex queue_mutex_;
	bool stop_;
};

void worker::operator()() const
{
	// the old code: (suffers from busy-wait problem)
	//while (true)
	//{
	//	unique_lock<mutex> locker(pool_.queue_mutex_);
	//	if (pool_.stop_) return;
	//	if (!pool_.tasks_.empty())
	//	{
	//		const auto task = pool_.tasks_.front();
	//		pool_.tasks_.pop_front();
	//		locker.unlock();
	//		task();
	//	}
	//	else {
	//		locker.unlock();
	//	}
	//}

	//the new code: (uses a condition_variable to solve the busy-wait problem)
	//with the change, time taken changes from > 250ms to ~5ms
	while (true)
	{
		unique_lock<mutex> locker(pool_.queue_mutex_);

		if (pool_.stop_)
			return;

		// wait for condition variable
		while (pool_.tasks_.empty())
			pool_.cv_.wait(locker);

		const auto task = pool_.tasks_.front(); // top/front
		pool_.tasks_.pop_front(); // pop
		locker.unlock(); // unlock
		task(); // do task
	}
}

thread_pool::thread_pool(const size_t threads) : stop_(false)
{
	for (size_t i = 0; i < threads; ++i)
		workers_.push_back(thread(worker(*this)));
}

thread_pool::~thread_pool()
{
	stop_ = true; // stop all threads

	for (auto &thread : workers_)
		thread.join();
}

template<class F>
void thread_pool::enqueue(F f)
{
	// make unqiue lock with the mutex
	unique_lock<mutex> lock(queue_mutex_);
	tasks_.push_back(function<void()>(f));
	cv_.notify_one(); // wake up one of the blocked consumers
}

int main()
{
	const auto num_threads = 4;
	const auto num_workers = 100;

	thread_pool pool(num_threads);

	const auto start = chrono::duration_cast<chrono::milliseconds>(
		chrono::system_clock::now().time_since_epoch()
		);

	// queue a bunch of "work items"
	for (auto i = 0; i < num_workers; ++i)
	{
		// queue a simple cout "hello"
		pool.enqueue([i, start, num_workers]()
		{
			// (improvement) using stringstream to support async/threading writing (slower, but corrected output)
			stringstream msg;
			msg << "Hello from work item " << i << endl;
			cout << msg.str();

			// for the last worker, print the total time taken
			if (i == num_workers-1)
			{
				const auto end = chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now().time_since_epoch()
					);

				cout << "time: " << (end - start).count() << " ms" << endl;
			}
		});
	}

	// wait for keypress to give worker threads the opportunity to finish tasks
	cin.ignore();

	return 0;
}

