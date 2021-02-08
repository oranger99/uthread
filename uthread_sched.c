#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include "uthread_inner.h"

struct sched* 
_sched_get() {
    return pthread_getspecific(uthread_sched_key);
}

static int 
_sched_work_done(struct p *p) {
    /* for debugging */
    // struct uthread *np = NULL;
    // int i = 0;
    // for (np = p->ready.tqh_first; np != NULL; np = np->ready_next.tqe_next) 
    //     i++;
    // printf("[sched id: %d] there are %d ready uthreads on current p [id: %d]\n", _sched_get()->id, i, p->id);
    /* end */

    return TAILQ_EMPTY(&p->ready);
}

/* 用于在整个进程工作结束后释放全局资源，一定要在调用函数之前上锁，防止多个线程同时释放 */
void free_source() {

    struct sched *sched = _sched_get();
    struct global_data *global = sched->global;

    /* 释放sched和p的结构体 */
    printf("releasing global data...\n");
    for (int i = 0; i < global->count_sched; ++i) 
        free(global->all_sched[i].stack);
    free(global->all_sched);
    free(global->all_p);
    free(global);
}

/* 调度器函数 */
void
_sched_run() {
    struct sched *sched = NULL;
    struct uthread *last_ready = NULL, *ut = NULL;

    sched = _sched_get();
start:
    while (sched->p && !_sched_work_done(sched->p)) {
        /* 执行就绪队列中的uthread */
        last_ready = TAILQ_LAST(&sched->p->ready, uthread_que);
        while (!TAILQ_EMPTY(&sched->p->ready)) {
            ut = TAILQ_FIRST(&sched->p->ready);
            TAILQ_REMOVE(&sched->p->ready, ut, ready_next);
            _uthread_resume(ut);
            if (!sched->p)          // NOTE：如果原来的p已经解绑了，那last_ready也就失去意义了
                break;
            if (ut == last_ready)         
                break;
        }
    }

    /* 若整个进程的所有uthread都运行完毕，则释放全局数据、退出进程 */
    if (sched->global->n_uthread == 0) {                
        assert(pthread_mutex_lock(&sched->global->mutex) == 0); // 释放global资源前要加锁，避免其它线程同时释放，这把锁无需释放
        printf("Congratulations, all uthreads done!\n");
        free_source();
        printf("Process is existing...\n");
        exit(0);        // 代替main函数的return语句结束整个进程
    } else 
        goto start;     // 如果系统中还有uthread在运行，让调度器空转（暂时用这种方式代替调度器休眠，后续再优化）    
}

/* 初始化整个运行时系统 */
/* NOTE：函数暂未对free失败的情况做除打印错误信息之外的任何有效处理，这显然是不行的！！ */
int
_runtime_init() {
    /* 创建全局数据 */
    struct global_data *global = NULL;
    if ((global = calloc(1, sizeof(struct global_data))) == NULL) {
        perror("Failed to initialize global data\n");
        return errno;
    }
    TAILQ_INIT(&global->sched_idle);
    TAILQ_INIT(&global->p_idle);
    global->count_sched = MAX_PROCS;    // 创建的sched的个数
    global->count_p = MAX_PROCS;        // 创建的p的个数
    if ((global->all_sched = calloc(1, global->count_sched * sizeof(struct sched))) == NULL) {  // 为sched数组分配空间
        perror("Failed to initialize scheduler");
        return errno;
    }
    if ((global->all_p = calloc(1, global->count_p * sizeof(struct p))) == NULL) {      // 为p数组分配空间
        perror("Failed to initialize p");
        return errno;
    }

    /* 初始化sched和p数组，将它们分别插入全局的idle队列 */
    for (int i = 0; i < MAX_PROCS; ++i) {
        /* 初始化sched，入idle sched队列 */
        struct sched *sched = &global->all_sched[i];
        if ((sched->stack = calloc(1, STACK_SIZE)) == NULL) {   // 为sched分配栈空间
            perror("Failed to allocate stack for sched");
            return errno;
        }
        sched->id = i;
        sched->status = BIT(SCHED_ST_IDLE);
        sched->stack_size = STACK_SIZE;
        sched->global = global;
        TAILQ_INSERT_TAIL(&global->sched_idle, sched, ready_next);
        
        /* 初始化p，入idle p队列 */
        struct p *new_p = &global->all_p[i];
        new_p->id = i;
        new_p->status = BIT(P_ST_IDLE);
        TAILQ_INIT(&new_p->ready);
        TAILQ_INSERT_TAIL(&global->p_idle, new_p, ready_next);
    }    

    /* 拿出一个sched，现在就要用 */
    struct sched *first_sched = TAILQ_FIRST(&global->sched_idle);
    TAILQ_REMOVE(&global->sched_idle, first_sched, ready_next);
    first_sched->status = BIT(SCHED_ST_RUNNING);
    /* 为sched分配一个p */
    first_sched->p = TAILQ_FIRST(&global->p_idle);                      
    TAILQ_REMOVE(&global->p_idle, first_sched->p, ready_next);
    first_sched->p->status = BIT(P_ST_RUNNING);

    /* 此后，线程就可以通过_sched_get()获取自己的调度器了 */
    assert(pthread_setspecific(uthread_sched_key, first_sched) == 0);   

    /* 为调度器初始化上下文，直接搬的_uthread_init中的代码 */
    void **stack = (void **)(first_sched->stack + (first_sched->stack_size));   
    stack[-3] = NULL;
    stack[-2] = (void *)first_sched;
    first_sched->ctx.esp = (void *)stack - (4 * sizeof(void *));     
    first_sched->ctx.ebp = (void *)stack - (3 * sizeof(void *)); 
    first_sched->ctx.eip = (void *)_sched_run;

    return 0;
}

/* 用作创建一个新线程时要绑定的函数，该函数是对_sched_run的封装，目的是为了初始化新线程上的uthread_sched_key */
void *
_sched_create_another(void *new_sched) {
    struct sched *sched = (struct sched *)new_sched;
    assert(pthread_setspecific(uthread_sched_key, sched) == 0);
    _sched_run();
}