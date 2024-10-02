#include <pthread.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

// Worker list
struct NWORKER{
    pthread_t thread_id;
    
    struct NWORKER *next;
    struct NWORKER *pre;
    bool terminate;

    struct MANAGER *pool;
};

// task list
struct NTASK{
    void (*task_func) (void*);
    void* user_data;

    struct NTASK *next;
    struct NTASK *pre;
};

// Thread pool
typedef struct MANAGER{
    struct NWORKER* workers;
    struct NTASK* tasks;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
}ThreadPool;

// add
#define LL_INSERT(item, list) do{   \
    item->pre = NULL;   \
    item->next = list;  \
    if(list)   list->pre = item;    \
    list = item;    \
}while(0)

// remove
#define LL_REMOVE(item, list) do{   \
    if(item->pre) item->pre->next = item->next;  \
    if(item->next) item->next->pre = item->pre;  \
    if(item == list) list = item->next;  \
    item->pre = item->next = NULL;   \
}while(0)


static void* thread_callback(void* arg){
    struct NWORKER* worker = (struct NWORKER*)arg;  
    while(!worker->terminate){
        pthread_mutex_lock(&worker->pool->mtx);
        while(worker->pool->tasks == NULL && !worker->terminate){   // 如果任务队列为空且线程池未终止，则等待
            pthread_cond_wait(&worker->pool->cond, &worker->pool->mtx);
        }
        struct NTASK* task = worker->pool->tasks;   // 取出首节点并移除
        if(task){
            LL_REMOVE(task, worker->pool->tasks);
        }
        pthread_mutex_unlock(&worker->pool->mtx);
        if (task) {         // 执行任务
            task->task_func(task->user_data);
            free(task->user_data); // 释放内存
            free(task); // 释放任务结构体
        }
    }
    return NULL;
}


// create

int thread_pool_create(struct MANAGER* pool, int nthread){
    if(!pool) return -1;    
    if(nthread < 1) nthread = 1; 
    
    memset(pool, 0, sizeof(struct MANAGER));   
    pthread_mutex_init(&pool->mtx, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for(int idx = 0; idx < nthread; ++idx){
        struct NWORKER* worker = (struct NWORKER*)malloc(sizeof(struct NWORKER));
        if(worker == NULL){
            perror("malloc worker");
            exit(-1);
        }
        memset(worker, 0, sizeof(struct NWORKER));
        worker->pool = pool;
        LL_INSERT(worker, pool->workers);
        
        if(pthread_create(&worker->thread_id, NULL, thread_callback, worker) != 0) {
            perror("pthread_create");
            exit(-1);
        }
    }
    return nthread;
}

// destroy
int thread_pool_destroy(struct MANAGER* pool){
    if (!pool) return -1;
    
    struct NWORKER* worker = pool->workers;
    while (worker) {
        worker->terminate = true;
        worker = worker->next;
    }

    pthread_mutex_lock(&pool->mtx);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);

    worker = pool->workers;
    while (worker) {
        pthread_join(worker->thread_id, NULL);
        struct NWORKER* next = worker->next;
        free(worker);
        worker = next;
    }

    pthread_mutex_destroy(&pool->mtx);
    pthread_cond_destroy(&pool->cond);
    return 0;
}

int thread_pool_push(struct MANAGER* pool, struct NTASK* task){
    
    pthread_mutex_lock(&pool->mtx);
    LL_INSERT(task, pool->tasks);
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);

    return 0;
}



#if 1

// 测试用的任务函数
void test_task(void* arg) {
    int* num = (int*)arg;
    std::cout << "Task executed with number: " << *num << std::endl;
}

int main() {
    // 创建线程池
    ThreadPool pool;
    int nthread = 4; // 线程数

    std::cout << "Creating thread pool with " << nthread << " threads." << std::endl;
    thread_pool_create(&pool, nthread);

    // 向线程池中推入任务
    for (int i = 0; i < 10; ++i) {
        struct NTASK* task = (struct NTASK*)malloc(sizeof(struct NTASK));
        if (task == NULL) {
            perror("malloc task");
            exit(-1);
        }
        int* num = (int*)malloc(sizeof(int));
        if (num == NULL) {
            perror("malloc num");
            exit(-1);
        }
        *num = i;

        task->task_func = test_task;
        task->user_data = num;

        std::cout << "Pushing task " << i << " to thread pool." << std::endl;
        thread_pool_push(&pool, task);
    }

    // 等待任务执行
    sleep(2); // 等待足够的时间让任务执行完毕

    // 销毁线程池
    std::cout << "Destroying thread pool." << std::endl;
    thread_pool_destroy(&pool);

    return 0;
}

#endif