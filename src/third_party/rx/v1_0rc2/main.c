/*
  Copyright Yusuke Tabata 2008 (tabata.yusuke@gmail.com)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rx.h"

static int cb(void *cookie, const char *c, int len, int id) {
  char buf[256];
  if (c) {
    strcpy(buf, c);
  }
  buf[len] = 0;
  printf("cb(%d %d),%s\n", len, id, buf);
  return 0;
}

static void small_test() {
  struct rx_builder *b = rx_builder_create();
  struct rx *r;
  unsigned char *image;
  char buf[256];
  rx_builder_add(b, "abc");
  rx_builder_add(b, "a");
  rx_builder_add(b, "abd");
  rx_builder_add(b, "b");
  rx_builder_build(b);
  /* rx_builder_get_key_index()*/
  printf(" key_index[a]= %d\n", rx_builder_get_key_index(b, "a"));
  printf(" key_index[abd]=%d\n", rx_builder_get_key_index(b, "abd"));
  image = rx_builder_get_image(b);
  printf("image size=%d\n", rx_builder_get_size(b));

  /* test for the image */
  r = rx_open(image);
  rx_search(r, 0, "abd", cb, buf);
  printf("reverse [%s]\n", rx_reverse(r, 3, buf, 256));
  printf("predictive lookup for [a]\n");
  rx_search(r, 1, "a", cb, buf);
  printf("predictive lookup for [b]\n");
  rx_search(r, 1, "b", cb, buf);

  /* clean up */
  rx_close(r);
  rx_builder_release(b);
}

static struct rx_builder *create_bulk_image(int size) {
  int i;
  char buf[32];
  int key_len = 0;
  struct rx_builder *b = rx_builder_create();
  rx_builder_set_bits(b, 7);
  for (i = 0; i < size; ++i) {
    key_len += sprintf(buf, "%d%d", i * i, i);
    rx_builder_add(b, buf);
  }
  rx_builder_build(b);
  printf("image size=%d(%d)\n", rx_builder_get_size(b), key_len);
  return b;
}

static void do_test() {
  struct rx_builder *b = create_bulk_image(500000);
  unsigned char *image = rx_builder_get_image(b);
  struct rx *r = rx_open(image);
  int i;
  for (i = 500; i < 510; ++i) {
    char buf[32];
    sprintf(buf, "%d%d", i * i, i);
    rx_search(r, 0, buf, cb, NULL);
    printf("reverse %d->[%s]\n", i, rx_reverse(r, i, buf, 32));
  }
  rx_close(r);
  rx_builder_release(b);
}

static void write_image(struct rx_builder *b, const char *fn) {
  unsigned char *image = rx_builder_get_image(b);
  int image_size = rx_builder_get_size(b);
  FILE *fp = fopen(fn, "w");
  fwrite(image, image_size, 1, fp);
  fclose(fp);
}

static void write_bulk_rx(int size, const char *fn) {
  struct rx_builder *b = create_bulk_image(size);
  write_image(b, fn);
  rx_builder_release(b);
}

static void build_rx(const char *in_fn, const char *out_fn) {
  struct rx_builder *b = rx_builder_create();
  FILE *in_fp = fopen(in_fn, "r");
  char buf[256];
  while (fgets(buf, 256, in_fp)) {
    char *p;
    buf[strlen(buf) - 1] = 0;
    if ((p = strchr(buf, ','))) {
      *p = 0;
    }
    rx_builder_add(b, buf);
  }
  rx_builder_build(b);
  write_image(b, out_fn);
  rx_builder_release(b);
}

static int cb_none(void *cookie, const char *c, int len, int id) {
  int *ck = cookie;
  *ck = 1;
  return 0;
}


static void bench_lookup_rx(const char *in_fn) {
  FILE *fp = fopen(in_fn, "r");
  int size;
  char *image;
  struct rx *r;
  int i;
  int nr;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  image = malloc(size);
  fread(image, size, 1, fp);
  fclose(fp);
  r = rx_open((unsigned char *)image);
  printf("file size=%d\n", size);
  nr = 0;
  for (i = 0; i < 100000; ++i) {
    char buf[32];
    int cookie = 0;
    sprintf(buf, "%d%d", i * i, i);
    rx_search(r, 0, buf, cb_none, &cookie);
    if (!cookie) {
      printf("failed to find %s\n", buf);
    } else {
      nr += cookie;
    }
  }
  printf("done %d results\n", nr);
  rx_close(r);
  free(image);
}

static void test_rbx() {
  struct rbx_builder *builder = rbx_builder_create();
  struct rbx *r;
  int len;
  unsigned char *image;
  rbx_builder_set_length_coding(builder, 4, 2);
  rbx_builder_push(builder, "abc", 4);
  rbx_builder_push(builder, "pqrs", 5);
  rbx_builder_push(builder, "0123456789", 11);
  rbx_builder_push(builder, "uv", 3);
  rbx_builder_build(builder);
  printf("rbx image size=%d\n", rbx_builder_get_size(builder));
  image = rbx_builder_get_image(builder);

  r = rbx_open(image);
  rbx_get(r, 0, &len);
  printf("[%s]\n", rbx_get(r, 1, &len));
  printf("[%s]\n", rbx_get(r, 2, &len));
  rbx_get(r, 3, &len);
  rbx_close(r);
  rbx_builder_release(builder);
}

int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "write_bulk")) {
    write_bulk_rx(atoi(argv[2]), argv[3]);
  } else if (argc == 4 && !strcmp(argv[1], "build")) {
    build_rx(argv[2], argv[3]);
  } else if (argc == 3 && !strcmp(argv[1], "bench_lookup")) {
    bench_lookup_rx(argv[2]);
  } else if (argc == 2 && !strcmp(argv[1], "rbx")) {
    test_rbx();
  } else {
    small_test();
    do_test();
  }
  return 0;
}
