#include "AVThread.h"
AVThread::AVThread()
    : mIsRunning(true)
{
    start();
}

AVThread::~AVThread()
{
    stop();
    quit();
    wait(100);
//    if(!isWait){
        terminate();//强行终断
//    }

    mMutex.lock();
    list<Task *>::iterator begin = mFuns.begin();
    list<Task *>::iterator end = mFuns.end();
    while(begin != end){
        Task *task = *begin;
        if(task != NULL)
            delete task;
        ++begin;
    }
    mFuns.clear();
    mMutex.unlock();
}

void AVThread::stop(){
    mIsRunning = false;
}

void AVThread::clearAllTask(int type){
    mMutex.lock();
    list<Task *>::iterator begin = mFuns.begin();
    list<Task *>::iterator end = mFuns.end();
    while(begin != end){
        if(type == -1){
            Task *task = *begin;
            if(task != NULL)
                delete task;
            task = NULL;
            begin = mFuns.erase(begin);
        }else{
            Task *task = *begin;
            if(task != NULL && task->type == type){
                delete task;
                begin = mFuns.erase(begin);
            }else{
                ++begin;
            }
        }
    }
    mFuns.clear();
    mMutex.unlock();
}

int AVThread::size(int type){
    mMutex.lock();
    int size = mFuns.size();
    if(type == -1){
        mMutex.unlock();
        return size;
    }
    size = 0;
    list<Task *>::iterator begin = mFuns.begin();
    list<Task *>::iterator end = mFuns.end();
    while(begin != end){
        Task *task = *begin;
        if(task != NULL && task->type == type){
            ++size;
        }
        ++begin;
    }
    mMutex.unlock();
    return size;
}

void AVThread::addTask(Task* task){
    mMutex.lock();
    mFuns.push_back(task);
    mMutex.unlock();
}

Task *AVThread::getTask(){
    mMutex.lock();
    Task *task = NULL;
    if(mFuns.size() > 0){
        task = mFuns.front();
        mFuns.pop_front();
    }
    mMutex.unlock();
    return task;
}

void AVThread::run(){
    while(mIsRunning){
        Task * task = getTask();
        if(task == NULL){
            QThread::msleep(1);
        }else{
            task->run();
            delete task;
        }
    }
}
