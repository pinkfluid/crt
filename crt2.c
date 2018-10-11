#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ev.h>

typedef struct crt crt_t;

#define CRT_STACK_DEPTH             32

#define CRT_OK                      0                   /* Status OK */
#define CRT_ERROR                   (CRT_OK - 1)        /* General ERROR */
#define CRT_ERROR_CANCEL            (CRT_OK - 2)        /* Cancelled */
#define CRT_ERROR_STACK_OVERFLOW    (CRT_OK - 3)        /* Stack too small */
#define CRT_ERROR_INVALID_DEPTH     (CRT_OK - 4)        /* Used "return" from inside CRT context */
#define CRT_ERROR_RUNTIME           (CRT_OK - 5)        /* Invalid line */

struct crt
{
    int         crt_depth;                      /* Current stack depth */
    int         crt_stack[CRT_STACK_DEPTH];     /* CRT stack */
    void       *crt_data;                       /* Random data */
};

#define CRT_INIT(C)                                                             \
do                                                                              \
{                                                                               \
    memset(C, 0, sizeof(crt_t));                                                \
    (C)->crt_depth = 0 - 1;                                                     \
}                                                                               \
while (0)

#define CRT(C)                                                                  \
{                                                                               \
    crt_t *__crt = (C);                                                         \
    int __crt_depth = ++__crt->crt_depth;                                       \
    int __crt_line = __crt->crt_stack[__crt_depth];                             \
                                                                                \
    /* XXX Handle stack overflow here */                                        \
                                                                                \
    switch (__crt_line)                                                         \
    {                                                                           \
        default:                                                                \
            if (__crt_line > 0)                                                 \
            {                                                                   \
                /* Jump to invalid line -- hard error */                        \
                CRT_EXIT(CRT_ERROR_RUNTIME);                                    \
            }                                                                   \
                                                                                \
        case CRT_OK:;

#define CRT_END                                                                 \
            /* Co-routine terminated successfully */                            \
            CRT_EXIT(CRT_OK);                                                   \
            break;                                                              \
        case CRT_ERROR_CANCEL:                                                  \
            goto __crt_exit;                                                    \
    }                                                                           \
__crt_exit:                                                                     \
                                                                                \
    if (__crt->crt_depth != __crt_depth)                                        \
    {                                                                           \
        assert(!"RETURN FROM CRT");                                             \
    }                                                                           \
    __crt->crt_depth--;                                                         \
}

#define CRT_SET_STATUS(C, code) ((C)->crt_stack[__crt_depth+1] = (code))
#define CRT_STATUS(C)           ((C)->crt_stack[(C)->crt_depth + 1])
#define CRT_RUNNING(C)          (CRT_STATUS(C) > 0)
#define CRT_CANCELLED(C)        (CRT_STATUS(C) == CRT_ERROR_CANCEL)

#define CRT_YIELD(...)                                                          \
do                                                                              \
{                                                                               \
    __crt->crt_stack[__crt->crt_depth--] = __LINE__;                            \
    return __VA_ARGS__;                                                         \
    case __LINE__:;                                                             \
}                                                                               \
while (0)

#define CRT_EXPAND(...)    __VA_ARGS__

#define CRT_AWAIT_NC(expr, ...)                                                 \
do                                                                              \
{                                                                               \
    CRT_SET_STATUS(__crt, CRT_OK);                                              \
    CRT_SET();                                                                  \
                                                                                \
    {                                                                           \
        expr;                                                                   \
    }                                                                           \
                                                                                \
    if (CRT_RUNNING(__crt))                                                     \
    {                                                                           \
        CRT_RETURN(__VA_ARGS__);                                                \
    }                                                                           \
}                                                                               \
while (0)

#define CRT_AWAIT(expr, ...)                                                    \
do                                                                              \
{                                                                               \
    CRT_AWAIT_NC(expr, __VA_ARGS__);                                            \
    /* Propagate cancellations */                                               \
    if (CRT_CANCELLED(__crt)) CRT_EXIT(CRT_ERROR_CANCEL);                       \
}                                                                               \
while (0)

#define CRT_EXIT(code)                                                          \
do                                                                              \
{                                                                               \
    __crt->crt_stack[__crt->crt_depth] = (code);                                \
    goto __crt_exit;                                                            \
}                                                                               \
while (0)

#define CRT_CANCEL(C)                                                           \
do                                                                              \
{                                                                               \
    int __crti;                                                                 \
                                                                                \
    /* Find end of stack */                                                     \
    for (__crti = 0; __crti < CRT_STACK_DEPTH; __crti++)                        \
    {                                                                           \
        if ((C)->crt_stack[__crti] == 0) break;                                 \
    }                                                                           \
                                                                                \
    if (__crti > 0) __crti--;                                                   \
                                                                                \
    (C)->crt_stack[__crti] = CRT_ERROR_CANCEL;                                  \
}                                                                               \
while (0)

/**
 * Rarely used
 */
#define CRT_SET()                                                               \
do                                                                              \
{                                                                               \
    case __LINE__:;                                                             \
    __crt->crt_stack[__crt->crt_depth] = __LINE__;                              \
}                                                                               \
while (0)

#define CRT_RETURN(...)                                                         \
do                                                                              \
{                                                                               \
    __crt->crt_depth--;                                                         \
    return __VA_ARGS__;                                                         \
}                                                                               \
while (0)

typedef int async_main_t(crt_t *crt, void *arg);
typedef struct async_task async_task_t;

struct async_task
{
    crt_t           at_crt;                 /* Main co-routine object */
    async_main_t   *at_main;                /* Async task body function */
    void           *at_main_data;           /* Task body function context */
    bool            at_done;                /* True if task completed */
    int             at_returncode;          /* Exit status */
    ev_timer        at_timer;               /* Generic sleep/timeout watcher */
    void           *at_what_scheduled;      /* What scheduled this task */
};

static void async_task_run(async_task_t *self);

void async_task_start(async_task_t *self, async_main_t *task_main, void *data)
{
    memset(self, 0, sizeof(*self));

    self->at_main = task_main;
    self->at_main_data = data;

    CRT_INIT(&self->at_crt);

    async_task_run(self);
}

void async_task_run(async_task_t *self)
{
    if (!self->at_done)
    {
        /* Run coroutine */
        self->at_returncode = self->at_main(&self->at_crt, self->at_main_data);
        self->at_done = !CRT_RUNNING(&self->at_crt);
    }
}

void async_task_cancel(async_task_t *self)
{
    CRT_CANCEL(&self->at_crt);
    async_task_run(self);
}

void async_task_ev_generic_fn(struct ev_loop *loop, ev_watcher *watcher, int revents)
{
    (void)revents;
    (void)loop;

    async_task_t *self = watcher->data;

    self->at_what_scheduled = watcher;

    async_task_run(self);
}

/**
 * Sleep for @p timeout seconds
 */
void async_task_sleep(crt_t *crt, double timeout)
{
    async_task_t *self = (async_task_t *)crt;

    CRT(crt)
    {
        self->at_timer.data = self;

        ev_timer_init(&self->at_timer, (void *)async_task_ev_generic_fn, timeout, 0.0);
        ev_timer_start(EV_DEFAULT, &self->at_timer);

        do
        {
            CRT_YIELD();
        }
        while (self->at_what_scheduled != &self->at_timer);
    }
    CRT_END;

    ev_timer_stop(EV_DEFAULT, &self->at_timer);
}

async_task_t t1;
async_task_t t2;


int task_t(crt_t *crt, void *data)
{
    (void)data;

    static int ii = 0;

    CRT(crt)
    {
        for (ii = 0; ii < 3; ii++)
        {
            printf("T1 = %d\n", ii);
            CRT_AWAIT(async_task_sleep(crt, 1.0), -1);
        }

    }
    CRT_END;

    printf("t1 ENDED = %d\n", CRT_STATUS(crt));

    printf("t1 will cancel t2 now\n");
    async_task_cancel(&t2);

    return 1;
}

int task_t2(crt_t *crt, void *data)
{
    (void)data;

    static int ii = 0;

    CRT(crt)
    {
        for (ii = 0; ii < 10; ii++)
        {
            printf("T2 = %d\n", ii);
            CRT_AWAIT(async_task_sleep(crt, 1.0), -1);
        }
    }
    CRT_END;


    if (CRT_CANCELLED(crt))
    {
        printf("t2 was cancelled\n");
    }
    else
    {
        printf("t2 ENDED: %d\n", CRT_STATUS(crt));
    }


    return 2;
}

int main(void)
{

    async_task_start(&t1, task_t, NULL);
    async_task_start(&t2, task_t2, NULL);

    ev_run(EV_DEFAULT, 0);

    printf("MAIN EXIT: t1 = %d, t2 = %d\n", t1.at_returncode, t2.at_returncode);

    return 0;
}

