#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_


#include<vector>
#include<mutex>
#include <thread>


template<class T>
class RingBuffer
{

public:
	RingBuffer(int n);

	T readNext();
	T peekNext();
	void dropNext();

	void writeNext(T t);
	bool isFull();
	bool isEmpty();


private:
	std::vector<T> data;
	//std::vector<std::mutex> mutexs;
	std::mutex mux;
	//std::mutex smux, emux;
	int size, start, end;

};


/*
Template Implementation (must be on header)
*/

template<class T>
RingBuffer<T>::RingBuffer(int n)
{
	size = n;
	start = 0;
	end = 0;
	//isEmpty = true;

	data = std::vector<T>(n);
	//mutexs = std::vector<mutex>(n, std::mutex());
}

template<class T>
bool RingBuffer<T>::isFull()
{
	//mux.lock();
	return ((end + 1) % size) == start;
	//mux.unlock();
}

template<class T>
bool RingBuffer<T>::isEmpty()
{
	//mux.lock();
	return end == start;
	//mux.unlock();
}

template<class T>
T RingBuffer<T>::readNext()
{
	mux.lock();
	while (isEmpty())
	{
		mux.unlock();
		//std::cout << "sleeping for 10ms" << std::endl;]
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return NULL;
	}
	T ret = data[start];
	data[start] = NULL;
	start = (start + 1) % size;
	mux.unlock();
	return ret;
}

template<class T>
T RingBuffer<T>::peekNext()
{
	mux.lock();
	while (isEmpty())
	{
		mux.unlock();
		//std::cout << "sleeping for 10ms" << std::endl;]
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return NULL;
	}
	T ret = data[start];
	mux.unlock();
	return ret;
}

template<class T>
void RingBuffer<T>::dropNext()
{
	mux.lock();
	while (isEmpty())
	{
		mux.unlock();
		//std::cout << "sleeping for 10ms" << std::endl;]
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return;
	}
	data[start] = NULL;
	start = (start + 1) % size;
	mux.unlock();
}



template<class T>
void RingBuffer<T>::writeNext(T t)
{
	mux.lock();
	if (isFull())
	{
		start = (start + 1) % size;
		std::cout << "Dropping frame from buffer" << std::endl;
	}
	int newEnd = (end + 1) % size;
	data[end] = t;
	end = newEnd;
	mux.unlock();
}



#endif