/*
  Copyright Yusuke Tabata 2007-2008 (tabata.yusuke@gmail.com)
*/
#ifndef _rx_h_
#define _rx_h_

#ifdef __cplusplus
extern "C" {
#endif


struct rx;
struct rx_builder;

struct rbx;
struct rbx_builder;

/* builder */
struct rx_builder *rx_builder_create();
void rx_builder_set_bits(struct rx_builder *builder, int bits);
void rx_builder_release(struct rx_builder *builder);
void rx_builder_add(struct rx_builder *builder, const char *word);
void rx_builder_dump(struct rx_builder *builder);
int rx_builder_build(struct rx_builder *builder);
unsigned char *rx_builder_get_image(struct rx_builder *builder);
int rx_builder_get_size(struct rx_builder *builder);
int rx_builder_get_key_index(struct rx_builder *builder, const char *key);

/* rx */
struct rx *rx_open(const unsigned char *bits);
void rx_close(struct rx *r);
void rx_search(const struct rx *r, int is_pred, const char *s,
	       int (*cb)(void *cookie, const char *s, int len, int id),
	       void *cookie);
char *rx_reverse(const struct rx *r, int n, char *buf, int len);

/* rbx builder */
struct rbx_builder *rbx_builder_create();
void rbx_builder_set_length_coding(struct rbx_builder *builder, int min, int step);
void rbx_builder_push(struct rbx_builder *builder, const char *bytes, int len);
int rbx_builder_build(struct rbx_builder *builder);
unsigned char *rbx_builder_get_image(struct rbx_builder *builder);
int rbx_builder_get_size(struct rbx_builder *builder);
void rbx_builder_release(struct rbx_builder *builder);

/* rbx */
struct rbx *rbx_open(const unsigned char *bits);
void rbx_close(struct rbx *r);
const unsigned char *rbx_get(struct rbx *r, int idx, int *len);

#ifdef __cplusplus
}
#endif

#endif
