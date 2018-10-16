#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ev.h>

#include "crt.h"

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

