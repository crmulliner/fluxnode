#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// --- START - mock mutex and semaphore functions
int xSemaphoreCreateMutex()
{
    return 0;
}

int xSemaphoreGive(int arg)
{
    return 0;
}

int xSemaphoreTake(int arg1, int arg2)
{
    return 0;
}

#define portMAX_DELAY 0
#define SemaphoreHandle_t int
#define xSemaphoreCreateBinary xSemaphoreCreateMutex

// --- END - mock mutex and semaphore functions

#include "queue.h"

typedef struct lm_lora_msg_t lm_lora_msg_t;
typedef lm_lora_msg_t *lm_lora_msg_ptr_t;
struct lm_lora_msg_t
{
    lm_lora_msg_ptr_t next;
    lm_lora_msg_ptr_t prev;
    int data;
};

int main()
{
    work_queue_t queue;
    work_queue_t *q = &queue;

    lm_lora_msg_ptr_t fn;
    int q_len;

    WORK_QUEUE_INIT(q);
    assert(q->recv_queue == NULL);

    lm_lora_msg_ptr_t n = malloc(sizeof(lm_lora_msg_t));
    n->next = NULL;
    n->prev = NULL;

    // add
    WORK_QUEUE_RECV_ADD(q, n);
    assert(q->recv_queue != NULL);

    WORK_QUEUE_RECV_LEN(q, q_len, fn);
    assert(q_len == 1);

    // take head
    lm_lora_msg_ptr_t head;
    WORK_QUEUE_RECV_TAKE_HEAD(q, head);
    assert(q->recv_queue == NULL);
    assert(head != NULL);

    WORK_QUEUE_RECV_LEN(q, q_len, fn);
    assert(q_len == 0);

    WORK_QUEUE_RECV_INSERT_HEAD(q, head);
    assert(q->recv_queue == head);

    WORK_QUEUE_RECV_LEN(q, q_len, fn);
    assert(q_len == 1);

    // take head, add node, re-insert head
    WORK_QUEUE_RECV_TAKE_HEAD(q, head);
    assert(q->recv_queue == NULL);
    assert(head != NULL);

    lm_lora_msg_ptr_t n1 = malloc(sizeof(lm_lora_msg_t));
    n1->next = NULL;
    n1->prev = NULL;

    WORK_QUEUE_RECV_ADD(q, n1);
    assert(q->recv_queue == n1);

    WORK_QUEUE_RECV_INSERT_HEAD(q, head);
    assert(q->recv_queue == head);

    WORK_QUEUE_RECV_LEN(q, q_len, fn);
    assert(q_len == 2);
}