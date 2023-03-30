#include "threadpool.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "filter.h"
#include "threadpool.h"

// Fonction exécutée par chaque thread travailleur
void* worker(void* arg) {
  struct worker_arg* w = arg;
  struct pool* pool = w->pool;

  // Attendre que tous les threads démarrent
  pthread_barrier_wait(&pool->ready);

  // Boucle principale du thread travailleur
  while (1) {
    pthread_mutex_lock(&pool->lock);

    // Attendre qu'une tâche soit ajoutée ou que le pool cesse de fonctionner
    while (list_empty(pool->task_list) && pool->running) {
      pthread_cond_wait(&pool->work_todo, &pool->lock);
    }

    // Si le pool ne fonctionne pas et que la liste des tâches est vide, sortir de la boucle
    if (!pool->running && list_empty(pool->task_list)) {
      pthread_mutex_unlock(&pool->lock);
      break;
    }

    // Obtenir une tâche de la liste des tâches
    struct list_node* node = list_pop_front(pool->task_list);
    struct task* task = node->data;
    list_node_unlink(node);

    pthread_mutex_unlock(&pool->lock);

    // Exécuter la tâche
    task->func(task->arg);
  }

  return NULL;
}

// Créer un nouveau pool de threads avec un certain nombre de threads
struct pool* threadpool_create(int num) {
  struct pool* pool = malloc(sizeof(struct pool));
  if (!pool) {
    perror("Échec de l'allocation de mémoire pour le pool de threads");
    return NULL;
  }

  // Initialiser les variables du pool
  pool->nb_threads = num;
  pool->running = 1;
  pool->task_list = list_new(NULL, NULL);
  pthread_mutex_init(&pool->lock, NULL);
  pthread_cond_init(&pool->work_todo, NULL);
  pthread_cond_init(&pool->work_done, NULL);
  pthread_barrier_init(&pool->ready, NULL, num + 1);

  // Allouer de la mémoire pour les threads et les arguments des travailleurs
  pool->threads = malloc(num * sizeof(pthread_t));
  pool->args = malloc(num * sizeof(struct worker_arg));
  if (!pool->threads || !pool->args) {
    perror("Échec de l'allocation de mémoire pour les threads ou les arguments des travailleurs");
    threadpool_join(pool);
    return NULL;
  }

  // Créer les threads travailleurs
  for (int i = 0; i < num; i++) {
    pool->args[i].id = i;
    pool->args[i].pool = pool;
    if (pthread_create(&pool->threads[i], NULL, worker, &pool->args[i]) != 0) {
      perror("Échec de la création du thread travailleur");
      threadpool_join(pool);
      return NULL;
    }
  }

  // Attendre que tous les threads démarrent
  pthread_barrier_wait(&pool->ready);

  return pool;
}

// Ajouter une tâche au pool de threads
void threadpool_add_task(struct pool* pool, func_t fn, void* arg) {
  pthread_mutex_lock(&pool->lock);

  // Vérifier si le pool est en cours d'exécution
  if (!pool->running) {
    pthread_mutex_unlock(&pool->lock);
    return;
  }

  // Créer une nouvelle tâche et l'ajouter à la liste des tâches
  struct task* new_task = malloc(sizeof(struct task));
  new_task->func = fn;
  new_task->arg = arg;

  struct list_node* node = list_node_new(new_task);
  list_push_back(pool->task_list, node);

  // Signaler qu'une nouvelle tâche est disponible
  pthread_cond_signal(&pool->work_todo);

  pthread_mutex_unlock(&pool->lock);
}

// Attendre que toutes les tâches soient terminées et libérer les ressources du pool
void threadpool_join(struct pool* pool) {
  pthread_mutex_lock(&pool->lock);
  pool->running = 0;
  pthread_cond_broadcast(&pool->work_todo);
  pthread_mutex_unlock(&pool->lock);

  // Attendre que tous les threads se terminent
  for (int i = 0; i < pool->nb_threads; i++) {
    pthread_join(pool->threads[i], NULL);
  }

  // Détruire les mutex, les conditions et la barrière
  pthread_mutex_destroy(&pool->lock);
  pthread_cond_destroy(&pool->work_todo);
  pthread_cond_destroy(&pool->work_done);
  pthread_barrier_destroy(&pool->ready);

  // Libérer la mémoire allouée pour la liste des tâches, les threads et les arguments des travailleurs
  list_free(pool->task_list);
  free(pool->threads);
  free(pool->args);

  // Libérer la mémoire allouée pour le pool
  free(pool);
}