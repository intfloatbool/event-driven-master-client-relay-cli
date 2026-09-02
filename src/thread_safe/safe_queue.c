#include "safe_queue.h"
#include <stdlib.h>

void ifb_safe_queue_push(ifb_strct_safe_queue *q, void *data) {
  ifb_strct_safe_queue_node *node =
      (ifb_strct_safe_queue_node *)malloc(sizeof(ifb_strct_safe_queue_node));
  node->data = data;
  node->next = NULL;

  pthread_mutex_lock(&q->mutex);
  if (q->tail) {
    q->tail->next = node;
  } else {
    q->head = node;
  }

  q->tail = node;

  // сигнализируем сетевому потоку, что данные появились
  pthread_cond_signal(&q->cond);
  pthread_mutex_unlock(&q->mutex);
}

// блокирующий из-за pthread_cond_wait , ждет пока в очереди появятся данные.
void *ifb_safe_queue_pop(ifb_strct_safe_queue *q) {
  pthread_mutex_lock(&q->mutex);

  // ждем, пока в очереди что-то появится
  while (q->head == NULL) {
    pthread_cond_wait(&q->cond, &q->mutex);
  }

  ifb_strct_safe_queue_node *temp = q->head;

  void *data = temp->data;
  q->head = q->head->next;
  if (q->head == NULL) {
    q->tail = NULL;
  }

  free(temp);

  pthread_mutex_unlock(&q->mutex);
  return data;
}

bool ifb_safe_queue_try_pop(ifb_strct_safe_queue *q, void **out_data) {
  pthread_mutex_lock(&q->mutex);

  if (q->head == NULL) {
    // empty queue
    pthread_mutex_unlock(&q->mutex);
    return false;
  }

  ifb_strct_safe_queue_node *temp = q->head;
  *out_data = temp->data;
  q->head = q->head->next;
  if (q->head == NULL) {
    q->tail = NULL;
  }
  free(temp);
  pthread_mutex_unlock(&q->mutex);
  return true;
}
