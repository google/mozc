/*
 * This is small example to use rx APIs.
 * You may read this code from main().
 *
 *  Copyright Yusuke Tabata 2009 (tabata.yusuke@gmail.com)
 */

#include "rx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* small utilities to handle memory blocks */
struct image {
  unsigned char *ptr;
  int size;
};

static void save_image(struct image *im) {
  unsigned char *ptr = im->ptr;
  im->ptr = malloc(im->size);
  memcpy(im->ptr, ptr, im->size);
}

static void free_image(struct image *im) {
  free(im->ptr);
}


/* examples of each API */

void example_rx_builder(struct image *im) {
  struct rx_builder *builder;
  int idx;
  /* Creates rx_builder object */
  builder = rx_builder_create();
  /* You may use narrower than 8 bits to save space. At default
     and here it uses 8bit standard char. So you usually don't have to
     use this API */
  rx_builder_set_bits(builder, 8);
  /* Adds all strings. */
  rx_builder_add(builder, "abc");
  rx_builder_add(builder, "ebc");
  /* Then, builds the structure */
  rx_builder_build(builder);
  /* Gets memory image from builder */
  im->ptr = rx_builder_get_image(builder);
  im->size = rx_builder_get_size(builder);
  save_image(im);

  /* You can get index of the key after the image is built */
  idx = rx_builder_get_key_index(builder, "abc");
  printf("\"abc\" is located at %d\n", idx);
  /* dumps internal information (do nothing for now) */
  rx_builder_dump(builder);
  /* Releases rx_builder object */
  rx_builder_release(builder);
}

int example_cb(void *cookie, const char *s, int len, int id) {
  char *c = (char *)cookie;
  printf("cookie=(%s)\n", c);
  printf("current key=%s(%dchars), id=%d\n", s, len, id);
  /* returns non-zero value if you want to stop more traversal. */
  return 0;
}

const char *example_expansion_cb(const void *expansion_data,
                                 const char s) {
  const char **data = (const char **)expansion_data;
  int i;
  for (i = 0; data[i] && data[i][0]; ++i) {
    if (data[i][0] == s && !strncmp(data[i] + 1, " => ", 4)) {
      /* Skip the "x => " part. */
      printf("expanding '%c' into \"%s\"\n", s, data[i] + 5);
      return data[i] + 5;
    }
  }
  /* Not found. */
  return 0;
}

void example_rx_lookup(struct image *im) {
  struct rx *rx;
  char buf[10];
  const char *expansion_data[] = { "a => ae", 0 };

  /* Opens rx image from the pointer */
  rx = rx_open(im->ptr);
  /* Searches given key "abc". this calls given callback
     function example_cb at here for each matched key.
     When is_predictive (second argument) is non zero, this performs predictive
     lookup and finds strings have key as prefix like "abcd" if exist.
     If is_predictive is 0, this function searches prefix strings of
     given key including the key itself.
     Last argument cookie is an opaque pointer and will be given to
     callback function.
  */
  rx_search(rx, 0, "abc", example_cb, "cookie");
  /* Searches given key with "key expansion".
     For example, if you provide a function that expand 'a' into 'e' to
     rx_search_expand, it can find both "abc" and "ebc" using the key "abc".
  */
  rx_search_expand(rx, 0, "abc", example_cb, "cookie",
                   example_expansion_cb, expansion_data);

  /* performs reverse lookup with buffer and its length. This
     returns address of the buffer when it success.*/
  if (rx_reverse(rx, 0, buf, 10)) {
    printf("0 th string is %s\n", buf);
  }
  /* Closes rx object */
  rx_close(rx);
}

void example_rbx_builder(struct image *im) {
  struct rbx_builder *builder;
  /* Creates rbx_builder object */
  builder = rbx_builder_create();
  /* Sets length encoding paramenter. At default
     and this example, it is usually set as
     4 + 1n. Using default parameters is recommended.
  */
  rbx_builder_set_length_coding(builder, 4, 1);
  /* Adds all blobs. At here 4 bytes blob "abc\0" is added. */
  rbx_builder_push(builder, "abc", 4);
  /* Then, builds the structure */
  rbx_builder_build(builder);
  /* Gets memory image from builder */
  im->ptr = rbx_builder_get_image(builder);
  im->size = rbx_builder_get_size(builder);
  save_image(im);

  /* Releases rbx_builder object */
  rbx_builder_release(builder);
}

void example_rbx_lookup(struct image *im) {
  struct rbx *rbx;
  const unsigned char *p;
  int len;

  /* Opens rbx image from the pointer */
  rbx = rbx_open(im->ptr);
  /* Gets nth blob and length */
  p = rbx_get(rbx, 0, &len);
  printf("0th blob is %s length=%d\n", p, len);
  /* Closes rbx object */
  rbx_close(rbx);
}

void example_rx() {
  struct image img;
  /* This example shows rx_builder and rx APIs */
  example_rx_builder(&img);
  example_rx_lookup(&img);

  free_image(&img);
}

void example_rbx() {
  struct image img;
  /* This example shows rbx_builder and rbx APIs */
  example_rbx_builder(&img);
  example_rbx_lookup(&img);

  free_image(&img);
}

int main(int argc, char **argv) {
  /* rx library has 2 types of storages, rx and rbx.
   */
  example_rx();
  example_rbx();
  return 0;
}
