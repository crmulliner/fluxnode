/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct
{
  void *send_queue;
  void *recv_queue;
  SemaphoreHandle_t send_queue_mutex;
  SemaphoreHandle_t recv_queue_mutex;
} work_queue_t;

#define WORK_QUEUE_INIT(q)                         \
  do                                               \
  {                                                \
    q->send_queue = NULL;                          \
    q->recv_queue = NULL;                          \
    q->send_queue_mutex = xSemaphoreCreateMutex(); \
    q->recv_queue_mutex = xSemaphoreCreateMutex(); \
  } while (0)

#define QUEUE_DL_APPEND(head, entry) \
  do                                 \
  {                                  \
    if (head)                        \
    {                                \
      typeof(entry) _tmp = head;     \
      while (_tmp->next != NULL)     \
      {                              \
        _tmp = _tmp->next;           \
      }                              \
      entry->prev = _tmp;            \
      _tmp->next = entry;            \
    }                                \
    else                             \
    {                                \
      head = (typeof(head))entry;    \
    }                                \
  } while (0)

// -- RECV --

// get length of queue - do we need to add locking?
#define WORK_QUEUE_RECV_LEN(q, q_length, entry) \
  do                                            \
  {                                             \
    q_length = 0;                               \
    typeof(entry) _q_l_head = q->recv_queue;    \
    while (_q_l_head != NULL)                   \
    {                                           \
      q_length++;                               \
      _q_l_head = _q_l_head->next;              \
    }                                           \
  } while (0)

// append item at end
#define WORK_QUEUE_RECV_ADD(q, node)                    \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->recv_queue_mutex, portMAX_DELAY); \
    QUEUE_DL_APPEND(q->recv_queue, node);               \
    xSemaphoreGive(q->recv_queue_mutex);                \
  } while (0)

// add item as new head
#define WORK_QUEUE_RECV_INSERT_HEAD(q, node)            \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->recv_queue_mutex, portMAX_DELAY); \
    typeof(node) __head = q->recv_queue;                \
    q->recv_queue = node;                               \
    if (__head)                                         \
      QUEUE_DL_APPEND(q->recv_queue, __head);           \
    xSemaphoreGive(q->recv_queue_mutex);                \
  } while (0)

// get first item
#define WORK_QUEUE_RECV_GET(q, item)                    \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->recv_queue_mutex, portMAX_DELAY); \
    item = (typeof(item))q->recv_queue;                 \
    if (item != NULL)                                   \
    {                                                   \
      if (item->next == NULL)                           \
      {                                                 \
        q->recv_queue = NULL;                           \
      }                                                 \
      else                                              \
      {                                                 \
        q->recv_queue = (void *)item->next;             \
      }                                                 \
    }                                                   \
    xSemaphoreGive(q->recv_queue_mutex);                \
  } while (0)

// take head and all attached nodes (emptys queue)
#define WORK_QUEUE_RECV_TAKE_HEAD(q, head)              \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->recv_queue_mutex, portMAX_DELAY); \
    head = q->recv_queue;                               \
    q->recv_queue = NULL;                               \
    xSemaphoreGive(q->recv_queue_mutex);                \
  } while (0)

// -- SEND --

// append item at end
#define WORK_QUEUE_SEND_ADD(q, node)                    \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->send_queue_mutex, portMAX_DELAY); \
    QUEUE_DL_APPEND(q->send_queue, node);               \
    xSemaphoreGive(q->send_queue_mutex);                \
  } while (0)

// add item as new head
#define WORK_QUEUE_SEND_INSERT_HEAD(q, node)            \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->send_queue_mutex, portMAX_DELAY); \
    typeof(node) __head = q->send_queue;                \
    q->send_queue = node;                               \
    if (__head)                                         \
      QUEUE_DL_APPEND(q->send_queue, __head);           \
    xSemaphoreGive(q->send_queue_mutex);                \
  } while (0)

// get first item
#define WORK_QUEUE_SEND_GET(q, item)                    \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->send_queue_mutex, portMAX_DELAY); \
    item = (typeof(item))q->send_queue;                 \
    if (item != NULL)                                   \
    {                                                   \
      if (item->next == NULL)                           \
      {                                                 \
        q->send_queue = NULL;                           \
      }                                                 \
      else                                              \
      {                                                 \
        q->send_queue = (void *)item->next;             \
      }                                                 \
    }                                                   \
    xSemaphoreGive(q->send_queue_mutex);                \
  } while (0)

// take head and all attached nodes (emptys queue)
#define WORK_QUEUE_SEND_TAKE_HEAD(q, head)              \
  do                                                    \
  {                                                     \
    xSemaphoreTake(q->send_queue_mutex, portMAX_DELAY); \
    head = q->send_queue;                               \
    q->send_queue = NULL;                               \
    xSemaphoreGive(q->send_queue_mutex);                \
  } while (0)

#endif
