#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include <stdio.h>

#include "locker.h"

template <typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000) ;
    ~threadpool();

    bool append(T* request) ;

private:
    static void* worker(void* arg) ;
    void run();

private:
    int m_thread_number; // 线程数
    pthread_t * m_threads; // 线程数组

    int m_max_requests; // 请求队列最大长度
    std::list<T*> m_workqueue; // 请求队列

    locker m_queuelocker; //互斥锁
    sem m_queuestat; // 信号量
    bool m_stop; // 是否结束线程
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
    
    if(thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number]; //创建线程数组

    if(!m_threads) {
        throw std::exception();
    }

    for(int i = 0; i < thread_number; i++) {
        printf("create the %d thread\n", i);

        // worker 必须是静态函数
        if(pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        }

        if(pthread_detach(m_threads[i])) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool () {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append (T* request) {
    
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker (void* arg) {
    threadpool* pool = (threadpool *) arg;

    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run () {
    while(!m_stop) {
        m_queuestat.wait(); //阻塞
        m_queuelocker.lock();

        if(m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front(); //从队列里拿数据
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request) { // 没获取到
            continue;
        }

        request->process(); // 
    }
}

#endif