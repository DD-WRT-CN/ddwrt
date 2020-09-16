#ifndef NDPI_STRCOL_H
#define NDPI_STRCOL_H
/* simple strings collestion */

typedef struct str_collect {
	size_t	 max,last;
	char	 s[0];
} str_collect_t;

typedef struct hosts_str {
        str_collect_t *p[NDPI_NUM_BITS+1];
} hosts_str_t;

static inline hosts_str_t *str_hosts_alloc(void) {
    return (hosts_str_t *)kcalloc(1,sizeof(hosts_str_t),GFP_KERNEL);
}
static void str_hosts_done(hosts_str_t *h);
static str_collect_t *str_collect_init(size_t num_start);
static str_collect_t *str_collect_copy(str_collect_t *c,int add_size);
static hosts_str_t *str_collect_clone( hosts_str_t *h);
static int str_collect_look(str_collect_t *c,char *str,size_t slen);
static char *str_collect_add(str_collect_t **pc,char *str,size_t slen);
static void str_collect_del(str_collect_t *c,char *str, size_t slen);

#endif