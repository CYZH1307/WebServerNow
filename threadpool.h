#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
<<<<<<< HEAD
#include <pthread.h>
=======
#include <stdio.h>

>>>>>>> 0c5329cbafa18961736caa9520355bd901b42454
#include "locker.h"
#include "http_conn.h"

<<<<<<< HEAD
// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template<typename T>
=======
template <typename T>
>>>>>>> 0c5329cbafa18961736caa9520355bd901b42454
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;  
    
    // 描述线程池的数组，大小为m_thread_number    
    pthread_t * m_threads;

<<<<<<< HEAD
    // 请求队列中最多允许的、等待处理的请求的数量  
    int m_max_requests; 
    
    // 请求队列
    std::list< T* > m_workqueue;  
=======
    int m_max_requests; // 请求队列最大长度
    std::list<T*> m_workqueue; // 请求队列
>>>>>>> 0c5329cbafa18961736caa9520355bd901b42454

    // 保护请求队列的互斥锁
    locker m_queuelocker;   

    // 是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程          
    bool m_stop;                    
};

<<<<<<< HEAD
=======
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
    
    if(thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }
>>>>>>> 0c5329cbafa18961736caa9520355bd901b42454


<<<<<<< HEAD
#endif
=======
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
>>>>>>> 0c5329cbafa18961736caa9520355bd901b42454
