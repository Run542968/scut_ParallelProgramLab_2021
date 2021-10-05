#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#define THREAD 32
#define QUEUE  256
#define MAX_THREADS 64
#define MAX_QUEUE 65536

/* 简化变量定义 */
typedef struct threadpool_t threadpool_t;

/* 定义错误码 */
typedef enum {
    threadpool_invalid        = -1,
    threadpool_lock_failure   = -2,
    threadpool_queue_full     = -3,
    threadpool_shutdown       = -4,
    threadpool_thread_failure = -5
} threadpool_error_t;

typedef enum {
    threadpool_graceful       = 1
} threadpool_destroy_flags_t;


typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown  = 2
} threadpool_shutdown_t;


typedef struct {
    void (*function)(void *);
    void *argument;
} threadpool_task_t;

struct threadpool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  threadpool_task_t *queue;
  int thread_count;
  int queue_size;
  int head;
  int tail;
  int count;
  int shutdown;
  int started;
};

static void *threadpool_thread(void *threadpool);
int threadpool_free(threadpool_t *pool);
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);
int threadpool_add(threadpool_t *pool, void (*routine)(void *),void *arg, int flags);
int threadpool_destroy(threadpool_t *pool, int flags);



int tasks = 0, done = 0;
pthread_mutex_t lock;

void dummy_task(void *arg) {
    int *a=(int *)arg;
    printf("我是队列中的第 %d 个任务，现在被执行！\n",*a);
    pthread_mutex_lock(&lock);
    /* 记录成功完成的任务数 */
    done++;
    pthread_mutex_unlock(&lock);
}

int main(int argc, char **argv)
{
    threadpool_t *pool;

    /* 初始化互斥锁 */
    pthread_mutex_init(&lock, NULL);

    /* 断言线程池创建成功 */
    assert((pool = threadpool_create(THREAD, QUEUE, 0)) != NULL);
    fprintf(stderr, "Pool started with %d threads and queue size of %d\n", THREAD, QUEUE);

    /* 只要任务队列还没满，就一直添加 */
    while(threadpool_add(pool, &dummy_task, (void *)&tasks, 0) == 0) {
        pthread_mutex_lock(&lock);
        tasks++;
        pthread_mutex_unlock(&lock);
    }

    fprintf(stderr, "Added %d tasks\n", tasks);

    /* 不断检查任务数是否完成一半以上，没有则继续休眠 */
    while(tasks != done) {
        usleep(10000);
    }
    /* 这时候销毁线程池,0 代表 immediate_shutdown */
    assert(threadpool_destroy(pool, 0) == 0);
    fprintf(stderr, "Did %d tasks\n", done);

    return 0;
}


threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
    if(thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE) {
        return NULL;
    }

    threadpool_t *pool;
    int i;

    /* 申请内存创建内存池对象 */
    if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
        goto err;
    }

    /* Initialize */
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    /* Allocate thread and task queue */
    /* 申请线程数组和任务队列所需的内存 */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (threadpool_task_t *)malloc
        (sizeof(threadpool_task_t) * queue_size);

    /* Initialize mutex and conditional variable first */
    /* 初始化互斥锁和条件变量 */
    if((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
       (pthread_cond_init(&(pool->notify), NULL) != 0) ||
       (pool->threads == NULL) ||
       (pool->queue == NULL)) {
        goto err;
    }

    /* Start worker threads */
    /* 创建指定数量的线程开始运行 */
    for(i = 0; i < thread_count; i++) {
        if(pthread_create(&(pool->threads[i]), NULL,threadpool_thread, (void*)pool) != 0) {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }

    return pool;

 err:
    if(pool) {
        threadpool_free(pool);
    }
    return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *),void *argument, int flags)
{
    int err = 0;
    int next;

    if(pool == NULL || function == NULL) {
        return threadpool_invalid;
    }

    /* 必须先取得互斥锁所有权 */
    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }

    /* 计算下一个可以存储 task 的位置 */
    next = pool->tail + 1;
    next = (next == pool->queue_size) ? 0 : next;

    do {
        /* Are we full ? */
        /* 检查是否任务队列满 */
        if(pool->count == pool->queue_size) {
            err = threadpool_queue_full;
            break;
        }

        /* Are we shutting down ? */
        /* 检查当前线程池状态是否关闭 */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        /* Add task to queue */
        /* 在 tail 的位置放置函数指针和参数，添加到任务队列 */
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        /* 更新 tail 和 count */
        pool->tail = next;
        pool->count += 1;

        /* pthread_cond_broadcast */
        /*
         * 发出 signal,表示有 task 被添加进来了
         * 如果由因为任务队列空阻塞的线程，此时会有一个被唤醒
         * 如果没有则什么都不做
         */
        if(pthread_cond_signal(&(pool->notify)) != 0) {
            err = threadpool_lock_failure;
            break;
        }
        /*
         * 这里用的是 do { ... } while(0) 结构
         * 保证过程最多被执行一次，但在中间方便因为异常而跳出执行块
         */
    } while(0);

    /* 释放互斥锁资源 */
    if(pthread_mutex_unlock(&pool->lock) != 0) {
        err = threadpool_lock_failure;
    }

    return err;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
    int i, err = 0;

    if(pool == NULL) {
        return threadpool_invalid;
    }

    /* 取得互斥锁资源 */
    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }

    do {
        /* Already shutting down */
        /* 判断是否已在其他地方关闭 */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        /* 获取指定的关闭方式 */
        pool->shutdown = (flags & threadpool_graceful) ?
            graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        /* 唤醒所有因条件变量阻塞的线程，并释放互斥锁 */
        if((pthread_cond_broadcast(&(pool->notify)) != 0) ||
           (pthread_mutex_unlock(&(pool->lock)) != 0)) {
            err = threadpool_lock_failure;
            break;
        }

        /* Join all worker thread */
        /* 等待所有线程结束 */
        for(i = 0; i < pool->thread_count; i++) {
            if(pthread_join(pool->threads[i], NULL) != 0) {
                err = threadpool_thread_failure;
            }
        }
        /* 同样是 do{...} while(0) 结构*/
    } while(0);

    /* Only if everything went well do we deallocate the pool */
    if(!err) {
        /* 释放内存资源 */
        threadpool_free(pool);
    }
    return err;
}

int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0) {
        return -1;
    }

    /* Did we manage to allocate ? */
    /* 释放线程 任务队列 互斥锁 条件变量 线程池所占内存资源 */
    if(pool->threads) {
        free(pool->threads);
        free(pool->queue);

        /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);
    return 0;
}


static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for(;;) {
        /* Lock must be taken to wait on conditional variable */
        /* 取得互斥锁资源 */
        pthread_mutex_lock(&(pool->lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        /* 用 while 是为了在唤醒时重新检查条件 */
        while((pool->count == 0) && (!pool->shutdown)) {
            /* 任务队列为空，且线程池没有关闭时阻塞在这里 */
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        /* 关闭的处理 */
        if((pool->shutdown == immediate_shutdown) ||
           ((pool->shutdown == graceful_shutdown) &&
            (pool->count == 0))) {
            break;
        }

        /* Grab our task */
        /* 取得任务队列的第一个任务 */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        /* 更新 head 和 count */
        pool->head += 1;
        pool->head = (pool->head == pool->queue_size) ? 0 : pool->head;
        pool->count -= 1;

        /* Unlock */
        /* 释放互斥锁 */
        pthread_mutex_unlock(&(pool->lock));

        /* Get to work */
        /* 开始运行任务 */
        (*(task.function))(task.argument);
        /* 这里一个任务运行结束 */
    }

    /* 线程将结束，更新运行线程数 */
    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return(NULL);
}

