/*
 *  Copyright Yusuke Tabata 2009 (tabata.yusuke@gmail.com)
 *
 * TODO: Add performance test.
 * TODO: Add test for large data set to cover corner cases.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "rx.h"

static void EXPECT_STRN_EQ(const char *s1, const char *s2, int n) {
  if (strncmp(s1, s2, n)) {
    printf("expect %s, actual %s\n", s1, s2);
    abort();
  }
}

static void EXPECT_INT_EQ(int s1, int s2) {
  if (s1 != s2) {
    printf("expect %d, actual %d\n", s2, s1);
    abort();
  }
}

static void EXPECT_TRUE(int s1) {
  if (!s1) {
    printf("expect true\n");
    abort();
  }
}

struct lookup_info {
  struct rx_builder *builder;
  const char *key;
  int is_predictive;
  int count;
};

static int cb_0(void *cookie, const char *c, int len, int id) {
  char buf[10];
  struct lookup_info *info = cookie;
  int key_len;

  snprintf(buf, len + 1, "%s", c);
  ++info->count;
  EXPECT_INT_EQ(id, rx_builder_get_key_index(info->builder, buf));
  printf("%s id=%d len=%d\n", buf, id, len);
  key_len = strlen(info->key);
  if (info->is_predictive) {
    EXPECT_TRUE(len >= strlen(info->key));
  } else {
    EXPECT_TRUE(len <= strlen(info->key));
  }
  return 0;
}

static const char *cb_expand_chars(const void *expansion_data,
                                   const char s) {
  const char **table = (const char**)expansion_data;
  int i = 0;
  while (table[i] && table[i][0]) {
    if (table[i][0] == s) {
      return table[i] + 1;
    }
    ++i;
  }
  return 0;
}

static void test_rx() {
  struct rx_builder *builder = rx_builder_create();
  struct rx *r;
  struct lookup_info info;
  char buf[10];
  /* expand a -> a/e */
  const char *expansion_table[] = {
    "aae",
    0
  };

  rx_builder_add(builder, "aa");
  rx_builder_add(builder, "abc");
  rx_builder_add(builder, "abcd");
  rx_builder_add(builder, "abd");
  rx_builder_add(builder, "ebc");
  rx_builder_build(builder);

  /* key location */
  EXPECT_INT_EQ(rx_builder_get_key_index(builder, "abc"), 1);
  r = rx_open(rx_builder_get_image(builder));
  /* revese lookup */
  EXPECT_STRN_EQ(rx_reverse(r, 1, buf, 10), "abc", 3);
  /* prefix lookup */
  info.builder = builder;
  info.count = 0;
  info.key = "abcd";
  info.is_predictive = 0;
  rx_search(r, 0, "abcd", cb_0, (void *)&info);
  EXPECT_INT_EQ(info.count, 2);
  /* prefix lookup with key expansion */
  info.count = 0;
  info.key = "abcd";
  info.is_predictive = 0;
  rx_search_expand(r, 0, "abcd", cb_0, (void *)&info,
                   cb_expand_chars, expansion_table);
  EXPECT_INT_EQ(info.count, 3);
  /* predictive lookup */
  info.count = 0;
  info.key = "a";
  info.is_predictive = 1;
  rx_search(r, RX_SEARCH_PREDICTIVE, "a", cb_0, (void *)&info);
  EXPECT_INT_EQ(info.count, 4);
  /* predictive lookup */
  info.count = 0;
  info.key = "a";
  info.is_predictive = 1;
  rx_search_expand(r, RX_SEARCH_PREDICTIVE, "a", cb_0, (void *)&info,
                   cb_expand_chars, expansion_table);
  EXPECT_INT_EQ(info.count, 5);
  /* 1 level lookup */
  info.count = 0;
  info.key = "a";
  info.is_predictive = 1;
  rx_search(r, (RX_SEARCH_PREDICTIVE | RX_SEARCH_1LEVEL), "a", cb_0, (void *)&info);
  EXPECT_INT_EQ(info.count, 3);

  rx_close(r);
  rx_builder_release(builder);
}

static void test_rbx() {
  struct rbx_builder *builder = rbx_builder_create();
  struct rbx *r;
  int len;
  const char *ptr;
  unsigned char *image;
  /* set default value */
  rbx_builder_set_length_coding(builder, 4, 1);

  rbx_builder_push(builder, "abc", 4);
  rbx_builder_push(builder, "pqrs", 5);
  rbx_builder_push(builder, "0123456789", 11);
  rbx_builder_push(builder, "uv", 3);

  rbx_builder_build(builder);
  printf("rbx image size=%d\n", rbx_builder_get_size(builder));
  image = rbx_builder_get_image(builder);

  r = rbx_open(image);
  ptr = (const char *)rbx_get(r, 0, &len);
  EXPECT_STRN_EQ(ptr, "abc", 4);
  EXPECT_INT_EQ(len, 4);
  printf("[%s]\n", rbx_get(r, 1, &len));
  EXPECT_INT_EQ(len, 5);
  printf("[%s]\n", rbx_get(r, 2, &len));
  EXPECT_INT_EQ(len, 11);
  ptr = (const char *)rbx_get(r, 3, &len);
  /* min size was set to 4. so the result will not be 3. */
  EXPECT_INT_EQ(len, 4);
  EXPECT_STRN_EQ(ptr, "uv", 3);
  rbx_close(r);
  rbx_builder_release(builder);
}

static void test_rbx_with_empty_blob() {
  struct rbx_builder *builder = rbx_builder_create();
  struct rbx *r;
  int len;
  const char *ptr;
  unsigned char *image;
  rbx_builder_set_length_coding(builder, 0, 1);

  rbx_builder_push(builder, "abc", 4);
  rbx_builder_push(builder, "", 0);
  rbx_builder_push(builder, "pqrs", 5);
  rbx_builder_build(builder);

  printf("rbx image size=%d\n", rbx_builder_get_size(builder));
  image = rbx_builder_get_image(builder);
  r = rbx_open(image);
  ptr = (const char *)rbx_get(r, 1, &len);
  EXPECT_INT_EQ(len, 0);
  ptr = (const char *)rbx_get(r, 2, &len);
  EXPECT_INT_EQ(len, 5);
  EXPECT_STRN_EQ(ptr, "pqrs", 5);
  rbx_close(r);
  rbx_builder_release(builder);
}

int main(int argc, char **argv) {
  printf("*test_rx\n");
  test_rx();
  printf("\n*test_rbx\n");
  test_rbx();
  printf("\n*test_rbx_with_empty_blob\n");
  test_rbx_with_empty_blob();
  return 0;
}
