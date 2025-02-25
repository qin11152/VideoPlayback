#pragma once

#include <mutex>
#include <memory>
#include <queue>
#include <thread>
#include <condition_variable>

template <typename T>
class MyPacketQueue {
public:
	MyPacketQueue() = default;
	~MyPacketQueue();

	// 初始化模块
	int32_t initModule(uint32_t uiMaxQueueSize = 30);

	// 反初始化模块
	int32_t uninitModule();

	// 添加数据到队列尾
	int32_t pushPacket(T packet);

	int32_t addPacket(T packet);

	// 从队列中取出数据
	int32_t getPacket(T& packet);

	T front();

	void pop_front();

	uint32_t getSize();

	int32_t clearQueue();

	int32_t resume();

private:
	bool m_bRunningState{ false }; // 运行状态
	uint32_t m_uiMaxQueueSize{ 0 };       // 队列最大长度

	std::mutex m_mutex;                     // 互斥锁
	std::condition_variable m_packetCV;     // 条件变量
	std::deque<T> m_packetQueue;            // 队列，存储模板类型的数据
};

template <typename T>
void MyPacketQueue<T>::pop_front()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_packetQueue.size() > 0)
	{
		m_packetQueue.pop_front();
		m_packetCV.notify_one();
	}
}

template <typename T>
T MyPacketQueue<T>::front()
{
	return m_packetQueue.front();
}

template <typename T>
int32_t MyPacketQueue<T>::addPacket(T packet)
{
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_packetQueue.size() > m_uiMaxQueueSize)
		{
			m_packetCV.wait(lock, [this]() { return m_packetQueue.size() <= m_uiMaxQueueSize || !m_bRunningState; });
		}
		if (!m_bRunningState)
		{
			return -1; // 队列未运行
		}
		m_packetQueue.push_front(std::move(packet));
	}
	return 0;
}

template <typename T>
int32_t MyPacketQueue<T>::resume()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_packetCV.notify_one();
	return 0;
}

template <typename T>
int32_t MyPacketQueue<T>::clearQueue()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	while (!m_packetQueue.empty())
	{
		m_packetQueue.pop_front();
	}
	return 0;
}

template <typename T>
uint32_t MyPacketQueue<T>::getSize()
{
	return m_packetQueue.size();
}

// 析构函数
template <typename T>
MyPacketQueue<T>::~MyPacketQueue() {
	uninitModule();
}

// 初始化模块
template <typename T>
int32_t MyPacketQueue<T>::initModule(uint32_t uiMaxQueueSize) 
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_uiMaxQueueSize = uiMaxQueueSize;
	m_bRunningState = true;
	return 0; // 成功
}

// 反初始化模块
template <typename T>
int32_t MyPacketQueue<T>::uninitModule() 
{
	if (!m_bRunningState)
	{
		return -1;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	m_bRunningState = false;

	// 清空队列
	while (!m_packetQueue.empty()) 
	{
		m_packetQueue.pop_front();
	}

	// 通知所有等待的线程
	m_packetCV.notify_all();
	return 0; // 成功
}

// 添加数据到队列
template <typename T>
int32_t MyPacketQueue<T>::pushPacket(T packet) {
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_packetQueue.size() > m_uiMaxQueueSize)
		{
			m_packetCV.wait(lock, [this]() { return m_packetQueue.size() <= m_uiMaxQueueSize || !m_bRunningState; });
		}
		if (!m_bRunningState) 
		{
			return -1; // 队列未运行
		}
		m_packetQueue.push_back(std::move(packet));
	}
	m_packetCV.notify_one(); // 通知等待的线程
	return 0; // 成功
}

// 从队列中取出数据
template <typename T>
int32_t MyPacketQueue<T>::getPacket(T& packet) {
	std::unique_lock<std::mutex> lock(m_mutex);

	// 等待队列有数据或队列停止运行
	m_packetCV.wait(lock, [this]() { return !m_packetQueue.empty() || !m_bRunningState; });

	if (!m_bRunningState && m_packetQueue.empty()) 
	{
		return -1; // 队列已停止且没有数据
	}

	packet = std::move(m_packetQueue.front()); // 取出队列头部数据
	m_packetQueue.pop_front();                       // 弹出队列头部
	m_packetCV.notify_one();
	return 0; // 成功
}

