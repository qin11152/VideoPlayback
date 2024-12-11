#include "MyQueue.h"

//VideoPacketQueue::~VideoPacketQueue()
//{
//
//}
//
//int32_t VideoPacketQueue::initModule()
//{
//	m_bRunningState = true;
//}
//
//int32_t VideoPacketQueue::uninitModule()
//{
//	m_bRunningState = false;
//	m_VideoPakcetCV.notify_all();
//}
//
//int32_t VideoPacketQueue::addPacket(std::pair<AVPacket*, PacketType> packet)
//{
//	std::unique_lock<std::mutex> lck(m_mutex);
//	m_packetQueue.push(packet);
//}
//
//int32_t VideoPacketQueue::getPacket(std::pair<AVPacket*, PacketType>& packet)
//{
//	std::unique_lock<std::mutex> lck(m_mutex);
//	if (m_packetQueue.empty())
//	{
//		m_VideoPakcetCV.wait(lck, [this]() {return !m_packetQueue.empty() || !m_bRunningState; });
//	}
//	packet = m_packetQueue.front();
//	m_packetQueue.pop();
//}
