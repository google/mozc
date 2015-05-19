/*
  Copyright Yusuke Tabata 2008 (tabata.yusuke@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
/* Little Endian only*/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rx.h"

#define MAX_DEPTH 256
#define CHUNK_SIZE 32 /* bytes */

#ifdef __GNUC__
#define bitcount(x, z) (z?__builtin_popcount(x):32-__builtin_popcount(x))
#else
static int bitcount(unsigned int x, int z) {
  x = ((x & 0xaaaaaaaa) >> 1) + (x & 0x55555555);
  x = ((x & 0xcccccccc) >> 2) + (x & 0x33333333);
  x = ((x >> 4) + x) & 0x0f0f0f0f;
  x = (x >> 8) + x;
  x = ((x >> 16) + x) & 0x3f;
  if (!z) {
    x = 32 - x;
  }
  return x;
}
#endif

struct bit_stream {
  unsigned char *bits;
  int bits_array_len;
  int nr_bits_in_array;
  /**/
  unsigned int scratch;
  int scratch_bits;
};

struct rx_builder {
  int nr_bits;
  int max_nodes;
  /* array of bits */
  struct bit_stream stream;
  struct bit_stream edges;
  struct bit_stream terminals;
  struct bit_stream transitions[7];
  /* source strings */
  int nr_strs;
  int str_array_size;
  char **strs;
  int *indexes;
};

struct bv {
  const unsigned char *v;
  int nr_bytes;
  int *index;
  int index_len;
};

struct rx {
  const unsigned char *edges;
  const unsigned char *terminals;
  int nr_bits;
  const unsigned char *transitions[7];
  struct bv *ev;
  struct bv *tv;
};

static void init_bit_stream(struct bit_stream *bs) {
  bs->bits = NULL;
  bs->bits_array_len = 0;
  bs->nr_bits_in_array = 0;
  bs->scratch = 0;
  bs->scratch_bits = 0;
}

static void release_bit_stream(struct bit_stream *bs) {
  free(bs->bits);
}

struct rx_builder *rx_builder_create() {
  struct rx_builder *b;
  int i;
  /**/
  b = (struct rx_builder *)malloc(sizeof(*b));
  /**/
  b->nr_bits = 8;
  /**/
  init_bit_stream(&b->stream);
  init_bit_stream(&b->edges);
  init_bit_stream(&b->terminals);
  for (i = 0; i < 7; ++i) {
    init_bit_stream(&b->transitions[i]);
  }
  /**/
  b->nr_strs = 0;
  b->str_array_size = 0;
  b->strs = NULL;
  return b;
}

void rx_builder_set_bits(struct rx_builder *builder, int bits) {
  if (bits > 0 && bits <= 8) {
    builder->nr_bits = bits;
  }
}

void rx_builder_release(struct rx_builder *b) {
  int i;
  for (i = 0; i < b->nr_strs; ++i) {
    free(b->strs[i]);
  }
  free(b->strs);
  release_bit_stream(&b->stream);
  release_bit_stream(&b->edges);
  release_bit_stream(&b->terminals);
  for (i = 0; i < 7; ++i) {
    release_bit_stream(&b->transitions[i]);
  }
  free(b->indexes);
  free(b);
}

void rx_builder_add(struct rx_builder *b, const char *key) {
  if (key[0] == 0) {
    return ;
  }
  if (b->nr_strs == b->str_array_size) {
    b->str_array_size *= 2;
    b->str_array_size++;
    b->strs = (char **)realloc(b->strs, b->str_array_size * sizeof(char *));
  }
  b->strs[b->nr_strs] = strdup(key);
  ++b->nr_strs;
}

void rx_builder_dump(struct rx_builder *b) {
  /* do nothing for now */
  (void)b;
}

static void ensure_bits(struct bit_stream *bs, int nr) {
  while (bs->bits_array_len * 8 <= bs->nr_bits_in_array + nr) {
    bs->bits_array_len = bs->bits_array_len * 2 + 1;
    bs->bits = (unsigned char *)realloc(bs->bits, bs->bits_array_len);
  }
}

static void push_bit(struct bit_stream *bs, int z) {
  if (z) {
    bs->scratch |= (1 << bs->scratch_bits);
  }
  bs->scratch_bits++;
  if (bs->scratch_bits == 32) {
    unsigned int *array = (unsigned int *)bs->bits;
    array[bs->nr_bits_in_array / 32] = bs->scratch;
    bs->scratch = 0;
    bs->scratch_bits = 0;
    bs->nr_bits_in_array += 32;
  }
}

static void push_bytes(struct bit_stream *bs,
		       const unsigned char *buf, int len) {
  int i;
  if (!(bs->nr_bits_in_array & 7)) {
    // aligned
    memcpy(&bs->bits[bs->nr_bits_in_array / 8], buf, len);
    bs->nr_bits_in_array += 8 * len;
    return ;
  }
  for (i = 0; i < len; ++i) {
    int j;
    unsigned char byte = buf[i];
    /*printf("(%c)", byte);*/
    for (j = 0; j < 8; ++j) {
      push_bit(bs, byte & 1);
      byte >>= 1;
    }
  }
}

static int byte_order_swap(int s) {
  return ((s << 24) & 0xff000000) |
    ((s << 8) & 0x00ff0000) |
    ((s >> 8) & 0x0000ff00) |
    ((s >> 24) & 0x000000ff);
}

static int is_be() {
  int x = 1;
  char *p = (char *)&x;
  return *p == 0;
}

static void push_int(struct bit_stream *bs, int num) {
  if (is_be()) {
    num = byte_order_swap(num);
  }
  push_bytes(bs, (unsigned char *)&num, 4);
}

static void copy_stream(struct bit_stream *bdst, struct bit_stream *bsrc) {
  ensure_bits(bdst, bsrc->nr_bits_in_array);
  assert((bsrc->nr_bits_in_array & 7) == 0);
  assert((bdst->nr_bits_in_array & 7) == 0);
  push_bytes(bdst, bsrc->bits, bsrc->nr_bits_in_array / 8);
}

static void write_pad32(struct bit_stream *bs) {
  ensure_bits(bs, 32);
  while (bs->scratch_bits > 0) {
    push_bit(bs, 0);
  }
}

static void write_transition(struct rx_builder *b,
			     int t) {
  if (b->nr_bits == 8) {
    push_bytes(&b->transitions[0], (unsigned char *)&t, 1);
  } else {
    int i;
    for (i = 0; i < b->nr_bits; ++i) {
      int bit = 0;
      if (t & (1 << i)) {
	bit = 1;
      }
      push_bit(&b->transitions[i], bit);
    }
  }
}

static void write_tree(struct rx_builder *b) {
  int i;
  int nr;
  int nth;
  int depth = 0;
  int nth_terminal = 0;
  /* copies str array. This reduces number of strings to be scanned
   * when depth is high */
  int nr_strs = b->nr_strs;
  char **strs = (char **)malloc(sizeof(char *) *nr_strs);
  char **back_strs = (char **)malloc(sizeof(char *) *nr_strs);
  int *orig_indexes = (int *)malloc(sizeof(int) *nr_strs);
  int *back_indexes = (int *)malloc(sizeof(int) *nr_strs);
  for (i = 0; i < nr_strs; ++i) {
    orig_indexes[i] = i;
  }
  memcpy(strs, b->strs, sizeof(char *) *nr_strs);
  /* root */
  push_bit(&b->edges, 1);
  push_bit(&b->edges, 0);
  push_bit(&b->terminals, 0);
  write_transition(b, 0);

  do {
    char **tmp_strs;
    int *tmp_indexes;
    nr = 0;
    nth = 0;
    for (i = 0; i < nr_strs; ++i) {
      int len;
      len = strlen(strs[i]);
      if (len >= depth) {
	++nr;
      } else {
	continue;
      }
      back_strs[nth] = strs[i];
      back_indexes[nth] = orig_indexes[i];
      ++nth;
      /* emits 1 */
      if (i == 0 ||
	  (depth <= len - 1 &&
	   !strncmp(strs[i], strs[i - 1], depth))) {
	if ((i == 0 && len > depth)
	    || (i > 0 && strs[i][depth] != strs[i - 1][depth])) {
	  push_bit(&b->edges, 1);
	}
      } else {
	if (len > depth) {
	  push_bit(&b->edges, 1);
	}
      }
      /* emits 0 */
      if (i == nr_strs - 1) {
	if (depth <= len) {
	  push_bit(&b->edges, 0);
	}
      } else {
	if (len >= depth &&
	    strncmp(strs[i], strs[i+1], depth)) {
	  push_bit(&b->edges, 0);
	}
      }
      /* emits terminal */
      if (depth + 1 == len) {
	push_bit(&b->terminals, 1);
	write_transition(b, strs[i][depth]);
	b->indexes[orig_indexes[i]] = nth_terminal;
	++nth_terminal;
      } else if (depth + 1 < len &&
		 (i == 0 || strncmp(strs[i], strs[i - 1], depth + 1))) {
	push_bit(&b->terminals, 0);
	write_transition(b, strs[i][depth]);
      }
    }
    ++depth;
    /* swap strs */
    tmp_strs = strs;
    strs = back_strs;
    back_strs = tmp_strs;
    tmp_indexes = orig_indexes;
    orig_indexes = back_indexes;
    back_indexes = tmp_indexes;
    nr_strs = nth;
  } while (nr > 0);
  free(strs);
  free(back_strs);
  free(orig_indexes);
  free(back_indexes);
}

static void write_streams(struct rx_builder *b) {
  int i;
  ensure_bits(&b->terminals, b->max_nodes);
  if (b->nr_bits < 8) {
    for (i = 0; i < b->nr_bits; ++i) {
      ensure_bits(&b->transitions[i], b->max_nodes);
    }
  } else {
    ensure_bits(&b->transitions[0], (b->max_nodes) * 8);
  }
  ensure_bits(&b->edges, b->max_nodes * 2);
  write_tree(b);
  /**/
  for (i = 0; i < 7; ++i) {
    write_pad32(&b->transitions[i]);
  }
  write_pad32(&b->edges);
  write_pad32(&b->terminals);
}

static int cmp_str(const void *p1, const void *p2) {
  return strcmp(*(char *const *)p1, *(char *const *)p2);
}

static void sort_strs(struct rx_builder *b) {
  int i;
  int nr = 0;
  char *prev_str = NULL;
  char **new_strs;
  qsort(b->strs, b->nr_strs, sizeof(char *),
	cmp_str);
  b->max_nodes = 0;
  /* uniqify */
  new_strs = (char **)malloc(b->nr_strs * sizeof(char *));
  for (i = 0; i < b->nr_strs; ++i) {
    if (!prev_str || strcmp(b->strs[i], prev_str)) {
      new_strs[nr] = b->strs[i];
      prev_str = b->strs[i];
      b->max_nodes += strlen(b->strs[i]);
      ++nr;
    } else {
      free(b->strs[i]);
    }
  }
  free(b->strs);
  b->strs = new_strs;
  b->nr_strs = nr;
  b->indexes = (int *)malloc(sizeof(int) * nr);
}

int rx_builder_build(struct rx_builder *b) {
  int edge_byte_count;
  int terminal_byte_count;
  int transitions_byte_count;
  sort_strs(b);
  write_streams(b);
  /* meta data */
  edge_byte_count = b->edges.nr_bits_in_array / 8;
  terminal_byte_count = b->terminals.nr_bits_in_array / 8;
  transitions_byte_count = b->transitions[0].nr_bits_in_array / 8;
  /*printf("edge_byte_count=%d\n", edge_byte_count);
  printf("terminal_byte_count=%d\n", terminal_byte_count);
  printf("transition_byte_count=%d\n", transitions_byte_count);*/
  ensure_bits(&b->stream, 128);
  push_int(&b->stream, edge_byte_count);
  push_int(&b->stream, terminal_byte_count);
  push_int(&b->stream, b->nr_bits);
  push_int(&b->stream, transitions_byte_count);
  /**/
  copy_stream(&b->stream, &b->edges);
  copy_stream(&b->stream, &b->terminals);
  if (b->nr_bits == 8) {
    copy_stream(&b->stream, &b->transitions[0]);
  } else {
    int i;
    for (i = 0; i < b->nr_bits; ++i) {
      copy_stream(&b->stream, &b->transitions[i]);
    }
  }
  return 0;
}

unsigned char *rx_builder_get_image(struct rx_builder *b) {
  return b->stream.bits;
}

int rx_builder_get_size(struct rx_builder *b) {
  return b->stream.nr_bits_in_array / 8;
}

int rx_builder_get_key_index(struct rx_builder *b, const char *key) {
  int idx;
  char **res = (char **)bsearch((void *)&key, (void *)b->strs,
				b->nr_strs, sizeof(char *),
				cmp_str);
  if (!res) {
    return -1;
  }
  idx = res - b->strs;
  return b->indexes[idx];
}

static int index_len(int n) {
  int i;
  for (i = 1; i < n; i *=2);
  return i;
}

static int bv_get(const struct bv *b, int n) {
  int idx = n / 8;
  int pos = n & 7;
  if (pos < b->nr_bytes * 8) {
    return (b->v[idx] >> pos) & 1;
  }
  return 0;
}

static int count_bits_in_chunk(struct bv *b, int c, int limit) {
  int i;
  int nr = 0;
  unsigned int *p = (unsigned int *)&b->v[CHUNK_SIZE * c];
  limit -= CHUNK_SIZE * c;
  limit /= sizeof(int);
  for (i = 0; (size_t)i < CHUNK_SIZE / sizeof(int) &&
	 i < limit; ++i) {
    nr += bitcount(*p, 1);
    ++p;
  }
  return nr;
}

static struct bv *bv_alloc(const unsigned char *v, int nr_bytes) {
  struct bv *b = (struct bv *)malloc(sizeof(*b));
  int nr_chunk = (nr_bytes + CHUNK_SIZE - 1) / CHUNK_SIZE;
  int i;
  int total = 0;

  b->v = v;
  b->nr_bytes = nr_bytes;
  /* make index heap */
  b->index_len = index_len(nr_chunk);
  b->index = (int *)malloc(b->index_len * sizeof(int));
  /**/
  for (i = 0; i < b->index_len; ++i) {
    total += count_bits_in_chunk(b, i, nr_bytes);
    b->index[i] = total;
  }
  return b;
}

static void bv_free(struct bv *b) {
  free(b->index);
  free(b);
}

/* assumes @start is aligned by 4bytes */
static int bv_rank_naive(struct bv *b, int start, int n, int z) {
  int i;
  int nr = 0;
  int shift;
  unsigned int w;
  unsigned int *p = (unsigned int *)&b->v[start / 8];
  for (i = start; i + 32 <= n; i += 32) {
    nr += bitcount(*p, z);
    ++p;
  }
  w = *p;
  shift = 31 - (n - i);
  w <<= shift;
  if (z) {
    return nr + bitcount(w, 1);
  } else {
    return nr + bitcount(w, 0) - shift;
  }
}

static int get_total(const struct bv *b, int i, int z) {
  if (z) {
    return b->index[i];
  }
  return CHUNK_SIZE * 8 * (i + 1) - b->index[i];
}

static int bv_rank(struct bv *b, int n, int z) {
  int chunk = n / (CHUNK_SIZE * 8);
  int res = bv_rank_naive(b, chunk * CHUNK_SIZE * 8, n, z);
  if (chunk > 0) {
    res += get_total(b, chunk - 1, z);
  }

  return res;
}

static int bv_select_naive(const struct bv *b, int start, int n, int z) {
  int i = start;
  unsigned int *p = (unsigned int *)&b->v[i / 8];
  unsigned int w;
  do {
    int bc = bitcount(*p, z);
    if (bc > n) {
      break;
    }
    n -= bc;
    i += 32;
    ++p;
  } while (1);
  w = *p;
  while (n >= 0) {
    if (z) {
      if (w & 1) {
	--n;
      }
    } else {
      if (!(w & 1)) {
	--n;
      }
    }
    ++i;
    w >>= 1;
  }
  return i - 1;
}

static int find_chunk(const struct bv *b, int n, int z,
		      int start, int end) {
  int mid = (start + end) / 2;
  if (get_total(b, mid, z) >= n) {
    if (mid == 0 || get_total(b, mid - 1, z) <= n) {
      return mid;
    }
    return find_chunk(b, n, z, start, mid);
  } else {
    return find_chunk(b, n, z, mid, end);
  }
}

static int bv_select(const struct bv *b, int n, int z) {
  int c = find_chunk(b, n, z, 0, b->index_len);
  if (c > -1) {
    int total;
    if (c == 0) {
      total = 0;
    } else {
      total = get_total(b, c - 1, z);
    }
    return bv_select_naive(b, c * CHUNK_SIZE * 8, n - total, z);
  }
  /* just in case */
  return bv_select_naive(b, 0, n, z);
}

static int read_int(int im) {
  if (is_be()) {
    im = byte_order_swap(im);
  }
  return im;
}

struct rx *rx_open(const unsigned char *bits) {
  struct rx *r = (struct rx *)malloc(sizeof(*r));
  int *c = (int *)bits;
  /*printf("%d bytes for edges\n", c[0]);
  printf("%d bytes for terminals\n", c[1]);
  printf("%d bits for transitions\n", c[2]);*/

  r->edges = &bits[16];
  r->terminals = &bits[16 + read_int(c[0])];
  r->transitions[0] = &bits[16 + (read_int(c[0]) + read_int(c[1]))];
  r->nr_bits = read_int(c[2]);
  if (r->nr_bits < 8) {
    int i;
    int size = read_int(c[3]);
    for (i = 1; i < r->nr_bits; ++i) {
      r->transitions[i] = &r->transitions[0][size * i];
    }
  }
  r->ev = bv_alloc(r->edges, read_int(c[0]));
  r->tv = bv_alloc(r->terminals, read_int(c[1]));
  return r;
}

void rx_close(struct rx *r) {
  bv_free(r->ev);
  bv_free(r->tv);
  free(r);
}

static unsigned char get_transition(const struct rx *r, int pos) {
  if (r->nr_bits != 8) {
    int idx = pos / 8;
    int mask = 1 << (pos & 7);
    int i;
    int val = 0;
    for (i = 0; i < r->nr_bits; ++i) {
      if (r->transitions[i][idx] & mask) {
	val |= (1 << i);
      }
    }
    return val;
  }
  return r->transitions[0][pos];
}

static unsigned char *rx_upward(const struct rx *r, int pos, unsigned char *s) {
  int parent;
  do {
    int tv_rank = bv_rank(r->ev, pos, 1);
    parent = bv_select(r->ev, bv_rank(r->ev, pos, 0) - 1, 1);
    --s;
    *s = get_transition(r, tv_rank - 1);
    /*printf("tv_rank=%d [%c]\n", tv_rank, *s);*/
    /*printf("%d->%d\n", pos, parent);*/
    pos = parent;
  } while (parent > 1);
  return s;
}

char *rx_reverse(const struct rx *r, int n, char *src, int len) {
  int tv_pos = bv_select(r->tv, n, 1);
  int ev_pos = bv_select(r->ev, tv_pos, 1);
  unsigned char buf[MAX_DEPTH + 1];
  unsigned char *res;
  buf[MAX_DEPTH] = 0;

  res = rx_upward(r, ev_pos, &buf[MAX_DEPTH]);
  if (strlen((char *)res) < (size_t)len) {
    strcpy(src, (char *)res);
    return src;
  }
  return NULL;
}

#define FIND_EDGE 0
#define FIND_TERMINAL 1

static void rx_find(const struct rx *r,
		    const unsigned char *src,
                    unsigned char *buf,
		    void *cookie,
		    int (*cb)(void *cookie, const char *s, int len, int id),
                    const char *(cb_expand_chars)(const void *expansion_data,
                                                  const char s),
                    const void *expansion_data,
		    int find_type,
		    int cur, int pos) {
  if (!src[cur]) {
    return ;
  }

  char dummy[2];
  const char *expanded = NULL;

  if (cb_expand_chars) {
    const char *result = cb_expand_chars(expansion_data, src[cur]);
    if (result && result[0]) {
      expanded = result;
    }
  }
  if (!expanded) {
    /* No expansion so use only the original character. */
    dummy[0] = src[cur];
    dummy[1] = 0;
    expanded = dummy;
  }

  while (bv_get(r->ev, pos)) {
    int tv_rank = bv_rank(r->ev, pos, 1);
    /*printf("tv_rank=%d\n", tv_rank);*/
    char ch = get_transition(r, tv_rank - 1);
    if (strchr(expanded, ch)) {
      buf[cur] = ch;
      buf[cur + 1] = 0;
      if (find_type == FIND_EDGE ||
	  bv_get(r->tv, tv_rank - 1)) {
	int id;
	int rv;
	if (find_type == FIND_TERMINAL) {
	  /* rank of terminal */
	  id = bv_rank(r->tv, tv_rank -1, 1) - 1;
	} else {
	  /* rank of edge */
	  id = bv_rank(r->ev, pos, 1) - 2;
	}
        rv = cb(cookie, (const char *)buf, cur + 1, id);
      }
      int child_pos = bv_select(r->ev, bv_rank(r->ev, pos, 1) - 1, 0) + 1;
      /*printf("match %d->%d (%c),%d\n", pos, child_pos,
	r->transitions[tv_rank - 1], child_pos);*/
      rx_find(r, src, buf, cookie, cb, cb_expand_chars, expansion_data,
              find_type, cur + 1, child_pos);
    }
    /* move forward to next sibling */
    ++pos;
  }
}

struct traverse_stat {
  const struct rx *r;
  int flags;
  int depth_limit;
  int initial_edge_pos;
  int initial_buf_index;
  void *cookie;
  int (*cb)(void *cookie, const char *s,
	    int len, int id);
};

static int rx_traverse(struct traverse_stat *ts,
		       unsigned char *buf,
		       int buf_index,
		       int edge_pos) {
  const struct rx *r = ts->r;
  int child_pos;
  int tv_pos;
  int found_terminal = 0;
  /*printf("%d-[%s]@%d\n", buf_index, buf, edge_pos);*/
  tv_pos = bv_rank(r->ev, edge_pos, 1) - 1;
  if (bv_get(r->tv, tv_pos)) {
    int id = bv_rank(r->tv, tv_pos, 1) - 1;
    int rv = ts->cb(ts->cookie, (char *)buf, strlen((char *)buf), id);
    if (rv) {
      return rv;
    }
    found_terminal = 1;
  }
  if ((ts->flags & RX_SEARCH_1LEVEL) &&
      edge_pos != ts->initial_edge_pos &&
      found_terminal) {
    return 0;
  }
  if (buf_index >= ts->initial_buf_index + ts->depth_limit) {
    return 0;
  }
  child_pos = bv_select(r->ev, bv_rank(r->ev, edge_pos, 1) - 1, 0) + 1;
  while (bv_get(r->ev, child_pos)) {
    int rank = bv_rank(r->ev, child_pos, 1) - 1;
    int rv;
    buf[buf_index] = get_transition(r, rank);
    buf[buf_index + 1] = 0;
    rv = rx_traverse(ts, buf, buf_index + 1, child_pos);
    if (rv) {
      return rv;
    }
    ++child_pos;
  }
  return 0;
}

static int rx_start_traverse(const struct rx *r,
			     int flags,
			     void *cookie,
			     int (*cb)(void *cookie, const char *s,
				       int len, int id),
			     unsigned char *buf,
			     int buf_index,
			     int edge_pos) {
  struct traverse_stat ts;
  ts.r = r;
  ts.flags = flags;
  ts.depth_limit = (flags & RX_SEARCH_DEPTH_MASK) >> RX_SEARCH_DEPTH_SHIFT;
  if (ts.depth_limit == 0) {
    ts.depth_limit = MAX_DEPTH;
  }
  ts.initial_buf_index = buf_index;
  ts.initial_edge_pos = edge_pos;
  ts.cookie = cookie;
  ts.cb = cb;
  return rx_traverse(&ts, buf, buf_index, edge_pos);
}

struct predict_info {
  void *original_cookie;
  const struct rx *r;
  int (*cb)(void *cookie, const char *s, int len, int id);
  int len;
  int flags;
};

static int predict_cb(void *cookie, const char *c, int len, int edge_id) {
  struct predict_info *pred = (struct predict_info *)cookie;
  if (len == pred->len) {
    char buf[MAX_DEPTH + 1];
    strcpy(buf, c);
    int pos = bv_select(pred->r->ev, edge_id + 1, 1);
    rx_start_traverse(pred->r, pred->flags,
                      pred->original_cookie, pred->cb,
                      (unsigned char *)buf, len, pos);
    return 1;
  }
  return 0;
}

void rx_search(const struct rx *r, int flags, const char *s,
	       int (*cb)(void *cookie, const char *s, int len, int id),
	       void *cookie) {
  rx_search_expand(r, flags, s, cb, cookie, 0, 0);
}

void rx_search_expand(const struct rx *r, int flags, const char *s,
                      int (*cb)(void *cookie, const char *s, int len, int id),
                      void *cookie,
                      const char *(*cb_expand_chars)(const void *expansion_data,
                                                     const char s),
                      const void *expansion_data) {
  struct predict_info pred;
  unsigned char buf[MAX_DEPTH + 1];
  if (!(flags & RX_SEARCH_PREDICTIVE)) {
    /* find prefix strings */
    rx_find(r, (const unsigned char *)s, buf, cookie, cb,
            cb_expand_chars, expansion_data, FIND_TERMINAL, 0, 2);
    return ;
  }
  pred.original_cookie = cookie;
  pred.r = r;
  pred.cb = cb;
  pred.len = strlen(s);
  pred.flags = flags;
  rx_find(r, (const unsigned char *)s, buf, &pred, predict_cb,
          cb_expand_chars, expansion_data, FIND_EDGE, 0, 2);
}

/* rbx */
struct rbx_builder {
  int min_element_len;
  int element_len_step;
  struct bit_stream len_marks;
  struct bit_stream blobs;
  struct bit_stream output;
};

struct rbx_builder *rbx_builder_create() {
  struct rbx_builder *builder = (struct rbx_builder *)malloc(sizeof(*builder));
  builder->min_element_len = 4;
  builder->element_len_step = 1;
  init_bit_stream(&builder->blobs);
  init_bit_stream(&builder->len_marks);
  init_bit_stream(&builder->output);

  return builder;
}

void rbx_builder_set_length_coding(struct rbx_builder *b,
				   int min, int step) {
  b->min_element_len = min;
  b->element_len_step = step;
}

void rbx_builder_push(struct rbx_builder *b,
		      const char *bytes, int len) {
  int i;
  int elm_len = len;
  int p = 0;
  int s;
  if (elm_len < b->min_element_len) {
    elm_len = b->min_element_len;
  }
  /* element length */
  ensure_bits(&b->len_marks, len * 8 + 32);
  push_bit(&b->len_marks, 0);
  s = 0;
  for (i = b->min_element_len; i < elm_len; i += b->element_len_step) {
    push_bit(&b->len_marks, 1);
    ++s;
  }
  /* blobs */
  ensure_bits(&b->blobs, (elm_len + b->element_len_step) * 8);
  push_bytes(&b->blobs, (const unsigned char *)bytes, len);
  for (i = s * b->element_len_step + b->min_element_len - len;
       i > 0; --i) {
    push_bytes(&b->blobs, (const unsigned char *)&p, 1);
  }
}

int rbx_builder_build(struct rbx_builder *b) {
  int len;
  int dummy = 0;
  /* termination */
  rbx_builder_push(b, "", 1);
  write_pad32(&b->len_marks);
  write_pad32(&b->blobs);
  /* write header */
  len = b->len_marks.nr_bits_in_array / 8;
  ensure_bits(&b->output, 128);
  push_int(&b->output, len);
  push_int(&b->output, b->min_element_len);
  push_int(&b->output, b->element_len_step);
  push_int(&b->output, dummy);
  /* writes stream */
  copy_stream(&b->output, &b->len_marks);
  copy_stream(&b->output, &b->blobs);
  return 0;
}

unsigned char *rbx_builder_get_image(struct rbx_builder *b) {
  return b->output.bits;
}

int rbx_builder_get_size(struct rbx_builder *b) {
  return b->output.nr_bits_in_array / 8;
}

void rbx_builder_release(struct rbx_builder *b) {
  release_bit_stream(&b->blobs);
  release_bit_stream(&b->len_marks);
  release_bit_stream(&b->output);
  free(b);
}

struct rbx {
  int base_len;
  int len_step;
  struct bv *lv;
  const unsigned char *body;
};

struct rbx *rbx_open(const unsigned char *bits) {
  struct rbx *r = (struct rbx *)malloc(sizeof(*r));
  const int *c = (const int *)bits;
  r->lv = bv_alloc((const unsigned char *)&c[4], read_int(c[0]));
  r->base_len = read_int(c[1]);
  r->len_step = read_int(c[2]);
  r->body = &bits[read_int(c[0]) + 16];
  return r;
}

void rbx_close(struct rbx *r) {
  bv_free(r->lv);
  free(r);
}

const unsigned char *rbx_get(struct rbx *r, int idx, int *len) {
  int mark_pos = bv_select(r->lv, idx, 0);
  int image_idx = idx * r->base_len +
    bv_rank(r->lv, mark_pos, 1) * r->len_step;
  int i, l;
  l = r->base_len;
  for (i = mark_pos + 1; bv_get(r->lv, i); ++i) {
    l += r->len_step;
  }
  /*printf("pos = %d image[%d], len = %d\n", mark_pos, image_idx, l);*/
  if (len) {
    *len = l;
  }
  return &r->body[image_idx];
}
