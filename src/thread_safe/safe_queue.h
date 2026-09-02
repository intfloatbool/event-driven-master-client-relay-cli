#pragma once
#include <pthread.h>
#include <stdbool.h>

typedef struct ifb_strct_safe_queue_node {
  void *data;
  struct ifb_strct_safe_queue_node *next;
} ifb_strct_safe_queue_node;

typedef struct {
  ifb_strct_safe_queue_node *head;
  ifb_strct_safe_queue_node *tail;
  pthread_mutex_t mutex;
  pthread_cond_t cond; // позволяет потоку "спать" пока очередь пуста
} ifb_strct_safe_queue;

void ifb_safe_queue_push(ifb_strct_safe_queue *q, void *data);

// блокирующий из-за pthread_cond_wait , ждет пока в очереди появятся данные.
void *ifb_safe_queue_pop(ifb_strct_safe_queue *q);

bool ifb_safe_queue_try_pop(ifb_strct_safe_queue *q, void **out_data);
