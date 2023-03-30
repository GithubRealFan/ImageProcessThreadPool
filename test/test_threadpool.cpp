#include <gtest/gtest.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <istream>
#include <iterator>
#include <memory>
#include <unordered_map>

#include "barrier.h"
#include "config.h"
#include "threadpool.h"

static const timespec jiffy = {.tv_sec = 1, .tv_nsec = 0};

struct tidarg {
  std::unordered_map<int, int> tidmap;
  int count;
  pthread_mutex_t lock;
  struct barrier bar;
};

bool are_files_identical(const std::string& file1_path,
                         const std::string& file2_path) {
  std::ifstream file1(file1_path, std::ifstream::binary);
  std::ifstream file2(file2_path, std::ifstream::binary);

  if (!file1.is_open() || !file2.is_open()) {
    return false;
  }

  std::istreambuf_iterator<char> file1_iter(file1), file1_end;
  std::istreambuf_iterator<char> file2_iter(file2), file2_end;

  return std::equal(file1_iter, file1_end, file2_iter, file2_end);
}

void* worker_test(void* arg) {
  struct tidarg* info = static_cast<struct tidarg*>(arg);

  int tid = gettid();
  pthread_mutex_lock(&info->lock);
  info->tidmap[tid]++;
  info->count++;
  pthread_mutex_unlock(&info->lock);

  barrier_timewait(&info->bar);
  return NULL;
}

struct threadpool_deleter {
  void operator()(struct pool* p) { threadpool_join(p); }
};

/*
 * On utilise un compteur pour vérifier que le nombre d'exécution de la fonction
 * worker_test correspond à celui attendu.
 *
 * On utilise une table de hachage avec comme clé le TID. La valeur est
 * incrémentée à chaque utilisation, ce qui permet de vérifier que les fils
 * d'exécution sont bien réutilisés.
 *
 * On doit utiliser une barrière avec une échéance (timeout) sinon le test
 * bloque pour une durée infinie avec le code de départ, car worker_test() n'est
 * pas invoqué et donc la condition pour franchir la barrière n'est jamais
 * atteinte.
 */
TEST(ThreadPool, Usage) {
  int n_threads = 4;
  int n_cycles = 10;

  struct tidarg info;
  pthread_mutex_init(&info.lock, NULL);
  barrier_init(&info.bar, n_threads + 1, &jiffy);
  info.count = 0;

  // On utilise std::unique_ptr pour terminer correctement le threadpool si le
  // ASSERT_EQ() échoue.

  std::unique_ptr<struct pool, threadpool_deleter> p(
      threadpool_create(n_threads));
  ASSERT_TRUE(p.get() != nullptr);

  for (int i = 0; i < n_cycles; i++) {
    barrier_reset(&info.bar);
    for (int j = 0; j < n_threads; j++) {
      threadpool_add_task(p.get(), worker_test, &info);
    }
    ASSERT_EQ(barrier_timewait(&info.bar), 0) << "barrier timeout";

    pthread_mutex_lock(&info.lock);
    EXPECT_EQ(info.count, n_threads * (i + 1));
    EXPECT_EQ(info.tidmap.size(), n_threads);

    for (auto& item : info.tidmap) {
      EXPECT_EQ(item.second, (i + 1))
          << "tid: " << item.first << " reused: " << item.second;
    }
    pthread_mutex_unlock(&info.lock);
  }
}

#include "processing.h"
#include "threadpool.h"

static const char* img = SOURCE_DIR "/test/cat.png";
static const char* img_serial = BINARY_DIR "/test/cat-serial.png";
static const char* img_multithread = BINARY_DIR "/test/cat-multithread.png";

TEST(ThreadPool, Processing) {
  struct list* work_list = list_new(NULL, free_work_item);
  struct work_item* item
      = (struct work_item*)calloc(1, sizeof(struct work_item));

  item->input_file = strdup(img);
  item->output_file = strdup(img_serial);
  struct list_node* n = list_node_new(item);
  list_push_back(work_list, n);

  EXPECT_EQ(process_serial(work_list), 0);

  free(item->output_file);
  item->output_file = strdup(img_multithread);
  EXPECT_EQ(process_multithread(work_list, 2), 0);

  list_free(work_list);
  ASSERT_TRUE(are_files_identical(img_serial, img_multithread));
}
