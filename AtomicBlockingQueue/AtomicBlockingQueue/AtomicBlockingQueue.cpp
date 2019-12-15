#include "pch.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <cinttypes>
#include <mutex>

using namespace std;

#define RING_BUFFER_SIZE 16
class lockless_ring_buffer_spsc
{
public:

	lockless_ring_buffer_spsc()
	{
		write.store(0);
		read.store(0);
	}

	bool try_push(int64_t val)
	{
		const auto current_tail = write.load();
		const auto next_tail = increment(current_tail);
		if (next_tail != read.load())
		{
			buffer[current_tail] = val;
			write.store(next_tail);
			return true;
		}

		return false;
	}

	void push(int64_t val)
	{
		while (!try_push(val));
		
		this_thread::sleep_for(chrono::nanoseconds(10));
	}

	bool try_pop(int64_t* pval)
	{
		auto currentHead = read.load();

		if (currentHead == write.load())
		{
			return false;
		}

		*pval = buffer[currentHead];
		read.store(increment(currentHead));

		return true;
	}

	int64_t pop()
	{
		int64_t ret;
		while (!try_pop(&ret));
		
		this_thread::sleep_for(chrono::nanoseconds(10));
		return ret;
	}

private:
	std::atomic<int64_t> write;
	std::atomic<int64_t> read;
	static const int64_t size = RING_BUFFER_SIZE;
	int64_t buffer[RING_BUFFER_SIZE];

	int64_t increment(int n)
	{
		return (n + 1) % size;
	}
};

template <typename T>
class atomic_queue {
private:
	atomic<int> write;
	atomic<int> read;
	atomic<int> current_size;
	static const int size = RING_BUFFER_SIZE;
	T buffer[RING_BUFFER_SIZE];
	mutex m;
	condition_variable empty;
	condition_variable full;

public:
	atomic_queue() {
		write.store(0);
		read.store(0);
		current_size.store(0);
		for (int i = 0; i < RING_BUFFER_SIZE; i++) {
			buffer[i] = 0;
		}
	}

	void push(T item) {
		if (current_size.load() == RING_BUFFER_SIZE) {
			unique_lock<mutex> lock(m);
			full.wait(lock, [=] {return current_size.load() != RING_BUFFER_SIZE; });
			lock.unlock();
		}
		current_size++;
		const int current_write = write.load();
		write.store(increment(current_write));
		buffer[current_write] = 1;

		empty.notify_one();
	}

	T pop() {
		if (current_size.load() == 0) {
			unique_lock<mutex> lock(m);
			empty.wait(lock, [=] {return current_size.load() != 0; });
			lock.unlock();
		}
		const int current_read = read.load();
		read.store(increment(current_read));
		T res = buffer[current_read];
		buffer[current_read] = 0;
		current_size--;
		full.notify_one();
		return res;
	}

	int increment(int n)
	{
		return (n + 1) % size;
	}
};

int main()
{
	atomic_queue<int> queue;

	std::thread write_thread([&]() {
		for (int i = 0; i < 1000; i++)
		{
			queue.push(1);
			//cout << "+" <<endl;
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	} 
	);
	std::thread write_thread1([&]() {
		for (int i = 0; i < 1000; i++)
		{
			queue.push(1);
			//cout << "+" <<endl;
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}
	);

	std::thread read_thread([&]() {
		int count = 0;
		int i = 0;
		while (true) {
			i = queue.pop();
			if (i == 0)
				break;
			count += i;
		}
		cout << count << endl;
	} 
	);

	std::thread read_thread1([&]() {
		int count = 0;
		int i = 0;
		while (true) {
			i = queue.pop();
			if (i == 0)
				break;
			count += i;
		}
		cout << count << endl;
	}
	);

	write_thread.join();
	
	write_thread1.join();
	read_thread.join();
	read_thread1.join();

	return 0;
}

