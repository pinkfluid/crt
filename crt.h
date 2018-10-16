#if !defined(CRT_H_INCLUDED)
#define CRT_H_INCLUDED

#include <string.h>
#include <assert.h>

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
    assert(("Return from within CRT", __crt->crt_depth == __crt_depth));        \
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

#endif /* CRT_H_INCLUDED */

