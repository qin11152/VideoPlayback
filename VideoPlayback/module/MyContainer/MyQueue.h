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

	// ��ʼ��ģ��
	int32_t initModule(uint32_t uiMaxQueueSize = 30);

	// ����ʼ��ģ��
	int32_t uninitModule();

	// ������ݵ�����
	int32_t addPacket(T packet);

	// �Ӷ�����ȡ������
	int32_t getPacket(T& packet);

	uint32_t getSize();

	int32_t clearQueue();

	int32_t resume();

private:
	bool m_bRunningState{ false }; // ����״̬
	uint32_t m_uiMaxQueueSize{ 0 };       // ������󳤶�

	std::mutex m_mutex;                     // ������
	std::condition_variable m_packetCV;     // ��������
	std::queue<T> m_packetQueue;            // ���У��洢ģ�����͵�����
};

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
		m_packetQueue.pop();
	}
	return 0;
}

template <typename T>
uint32_t MyPacketQueue<T>::getSize()
{
	return m_packetQueue.size();
}

// ��������
template <typename T>
MyPacketQueue<T>::~MyPacketQueue() {
	uninitModule();
}

// ��ʼ��ģ��
template <typename T>
int32_t MyPacketQueue<T>::initModule(uint32_t uiMaxQueueSize) 
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_uiMaxQueueSize = uiMaxQueueSize;
	m_bRunningState = true;
	return 0; // �ɹ�
}

// ����ʼ��ģ��
template <typename T>
int32_t MyPacketQueue<T>::uninitModule() 
{
	if (!m_bRunningState)
	{
		return -1;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	m_bRunningState = false;

	// ��ն���
	while (!m_packetQueue.empty()) 
	{
		m_packetQueue.pop();
	}

	// ֪ͨ���еȴ����߳�
	m_packetCV.notify_all();
	return 0; // �ɹ�
}

// ������ݵ�����
template <typename T>
int32_t MyPacketQueue<T>::addPacket(T packet) {
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_packetQueue.size() > m_uiMaxQueueSize)
		{
			m_packetCV.wait(lock, [this]() { return m_packetQueue.size() <= m_uiMaxQueueSize || !m_bRunningState; });
		}
		if (!m_bRunningState) 
		{
			return -1; // ����δ����
		}
		m_packetQueue.push(std::move(packet));
	}
	m_packetCV.notify_one(); // ֪ͨ�ȴ����߳�
	return 0; // �ɹ�
}

// �Ӷ�����ȡ������
template <typename T>
int32_t MyPacketQueue<T>::getPacket(T& packet) {
	std::unique_lock<std::mutex> lock(m_mutex);

	// �ȴ����������ݻ����ֹͣ����
	m_packetCV.wait(lock, [this]() { return !m_packetQueue.empty() || !m_bRunningState; });

	if (!m_bRunningState && m_packetQueue.empty()) 
	{
		return -1; // ������ֹͣ��û������
	}

	packet = std::move(m_packetQueue.front()); // ȡ������ͷ������
	m_packetQueue.pop();                       // ��������ͷ��
	m_packetCV.notify_one();
	return 0; // �ɹ�
}

