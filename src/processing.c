#include "processing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "image.h"
#include "threadpool.h"

typedef image_t* (*filter_fn)(image_t* img);

static const filter_fn filters[] = {
    filter_scale_up2,      //
    filter_desaturate,     //
    filter_gaussian_blur,  //
    filter_edge_detect,    //
    NULL,                  //
};

// Fonction qui traite une image
void* process_one_image(void* arg) {
  struct work_item* item = arg;
  const char* fname = item->input_file;
  printf("processing image: %s\n", fname);

  image_t* img = image_create_from_png(fname);
  if (!img) {
    printf("failed to load image %s\n", fname);
    goto err;
  }

  int i = 0;
  while (filters[i]) {
    filter_fn fn = filters[i];
    image_t* next = fn(img);
    if (!next) {
      printf("failed to process image%s\n", fname);
      image_destroy(img);
      goto err;
    }
    image_destroy(img);
    img = next;
    i++;
  }
  image_save_png(img, item->output_file);
  image_destroy(img);

  return 0;
err:
  return (void*)-1UL;
}

int process_serial(struct list* items) {
  struct list_node* node = list_head(items);
  while (!list_end(node)) {
    unsigned long ret = (unsigned long)process_one_image(node->data);
    if (ret < 0) {
      return -1;
    }
    node = node->next;
  }
  return 0;
}

int process_multithread(struct list* items, int nb_thread) {
  // Créer un pool de threads avec nb_thread threads
  struct pool* pool = threadpool_create(nb_thread);
  if (!pool) {
    printf("Échec de la création du pool de threads\n");
    return -1;
  }

  // Ajouter toutes les images à la file d'attente des tâches
  struct list_node* node = list_head(items);
  while (!list_end(node)) {
    struct work_item* item = node->data;
    threadpool_add_task(pool, process_one_image, item);
    node = node->next;
  }

  // Attendre que le traitement soit terminé
  threadpool_join(pool);

  return 0;
}

struct work_item* make_work_item(const char* input_file, const char* output_dir) {
  struct work_item* item = malloc(sizeof(struct work_item));
  item->input_file = strdup(input_file);
  item->output_file = strdup(output_dir);
  return item;
}

void free_work_item(void* item) {
  struct work_item* w = item;
  free(w->input_file);
  free(w->output_file);
  free(item);
}
