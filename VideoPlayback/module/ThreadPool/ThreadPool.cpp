#include "ThreadPool.h"

ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
	stopPool();
	int i = 1;
	i++;
}

void ThreadPool::startPool(int num)
{
	m_iThreadNumber = num;
	m_bRunning = true;
	for (int i = 0; i < m_iThreadNumber; ++i)
	{
		std::thread t(&ThreadPool::doTask, this);
		m_vecThread.push_back(std::move(t));
	}
}

void ThreadPool::stopPool()
{
	//����״̬��Ϊfalse
	m_bRunning = false;
	//�߼�����������һ��
	m_dequeNotEmptyCV.notify_all();
	for (auto& item : m_vecThread)
	{
		//�����߳���Դ
		if (item.joinable())
		{
			item.join();
		}
	}
}

void ThreadPool::doTask()
{
	while (1)
	{
		//Ҫ��ȡ������
		ThreadTask task;
		{
			std::unique_lock<std::mutex> lck(m_mutex);
			//��������״̬��deque��СΪ��
			//if (m_bRunning && 0 == m_dequeTask.size())
			//{
			//    printf("thread id:%d,", GetCurrentThreadId());
			//    printf("wiat state\n");
			//    //û�������ʱ��͵ȴ�
			//    m_dequeNotEmptyCV.wait(lck);
			//}
			//�ȴ�������в��ջ���״̬Ϊ�˳�
			m_dequeNotEmptyCV.wait(lck, [this]() {return !m_bRunning || !m_dequeTask.empty(); });
			//��������״̬��������п����Ǿ��˳�
			if (!m_bRunning && m_dequeTask.empty())
			{
				return;
			}
			//������ͰѶ��˵��ó���
			task = std::move(m_dequeTask.front());
			m_dequeTask.pop_front();
		}
		//�����ϱߵ�������,lck���Զ�������
		//ִ������
		task();
	}
}