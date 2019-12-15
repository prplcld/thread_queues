#include "pch.h"
#include <iostream>
#include <mutex>
#include <thread>
#include <deque>
#include <queue>
#include <condition_variable>
#include <atomic>

using namespace std;

const int task_num = 4*1024*1024;
const int consumer_num = 1;
const int QUEUE_SIZE = 8;

template <typename T> 
class BlockingQueue {
private:
	mutex m;
	condition_variable condition;
	deque<T> q;
public:
	
	void push(T value) {
		lock_guard<mutex> lg(this->m);
		q.push_front(value);
		//cout << "push" << endl;
		this->condition.notify_one();
	}
	T pop() {
		unique_lock<mutex> ul(this->m);
		this->condition.wait(ul, [=] {return !this->q.empty(); });
		T rc(move(this->q.back()));
		this->q.pop_back();
		//cout << "pop" << endl;
		return rc;
	}

	int get_size() {
		lock_guard<mutex> lock(this->m);
		return q.size();
	}
};

class Producer {
	BlockingQueue<int>& queue;
public:
	Producer(BlockingQueue<int> &q) : queue(q){}

	void produce() {
		for (int i = 0; i < task_num; i++) {
			queue.push(i);
		}
	}
};

class Consumer {
	BlockingQueue<int>& queue;
public:
	Consumer(BlockingQueue<int> &q) : queue(q){}

	void consume() {
		int amount = 0;
		while (queue.get_size() >= consumer_num) {
			queue.pop();
			amount++;
			//this_thread::sleep_for(chrono::nanoseconds(10));
		}
		cout << amount << endl;
	}
};

class QueueConditionMutex {
public:
	queue<int> queue;

	QueueConditionMutex(int maxSize) {
		this->maxSize = maxSize;
	}
	void push(int val) {
		unique_lock<mutex> mlock(mtx);
		if (queue.size() == maxSize) {
			condPop.notify_one();
			condPush.wait(mlock);
		}
		queue.push(val);
		condPop.notify_one();
	}

	bool pop(int& val) {
		unique_lock<mutex> mlock(mtx);
		if (queue.empty()) {
			condPop.wait_for(mlock, chrono::milliseconds(1));
			if (queue.empty())
			{
				mlock.unlock();
				return false;
			}
		}
		val = queue.front();
		queue.pop();
		condPush.notify_one();
		mlock.unlock();
		return true;
	}
private:
	mutex mtx;

	condition_variable condPop;
	condition_variable condPush;
	int maxSize;

};

int main()
{
	BlockingQueue<int> queue;
	Producer p(queue);
	thread prod(&Producer::produce, p);
	Consumer c(queue);
	thread cons(&Consumer::consume, c);
	prod.join();
	cons.join();


}


