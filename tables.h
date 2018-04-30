#ifndef __TABLES_H__
 #define __TABLES_H__

#include <stdint.h>

#define SIZE(z, fileno)	((z)->ft[fileno].ve - (z)->ft[fileno].vs)


typedef struct
{
    int *ft_idx; /*-1 = unresolved, -2 = ignored (0), else taken literally. */
    int (*get_offsets)( void *, int idx, void *t );
    int (*set_offsets)( void *, int idx, void *t );
    int count;
    int file_no;
    int offset;
    int inc;
}
table;

typedef struct
{
    uint32_t vs;
    uint32_t ve;
    uint32_t ps;
    uint32_t pe;
    uint8_t *data;
}
ft_entry;

#define OOT	0
#define MM	1

typedef struct
{
    int game;
    int file_start;
    int st_off;	int st_max;
    int at_off;	int at_max;
    int objt_off;	int objt_max;
    int ovlt0_off;	int ovlt0_max;
    int ovlt1_off;	int ovlt1_max;
    int mmkd_off;	int mmkd_max;
    int efft_off;	int efft_max;
    int skyt_off;	int skyt_max;
}
game_config;

typedef struct
{
    table *tables;
    ft_entry *ft;
    uint8_t *rom_data;
    int n_tables;
    int n_files;
    int code_fn;
    int rom_size;
    game_config conf;
}
z_handle;

typedef int (read_tbl)( z_handle *z, int idx, table *t );
typedef int (write_tbl)( z_handle *z, int idx, table *t );

z_handle * new_handle( void *, int );
int read_files( z_handle * );
int init_tables( z_handle * );
table *add_table( z_handle *, read_tbl *, write_tbl *, int, int, int, int );
void read_tables( z_handle * );
int read_st( z_handle *z, table *t );
void resolve_errors( z_handle *);
int decide_unkn_ptr( z_handle *z, uint32_t start, uint32_t end );
int decide_size( ft_entry *f, int siz1 );
void sort_files( z_handle * );
void write_tables( z_handle * );
void get_rom( z_handle *, void **, int * );
int is_code( ft_entry * );
int set_game_config( z_handle *, int);
int filetable_get_entry(z_handle *, int, int);
char * get_filename( z_handle *z, int fileno );
void destroy_handle( z_handle *z );
void rom_compress_lzo( z_handle *z, void **out, int *out_size);
void rom_compress_yaz0( z_handle *z, void **out, int *out_size);

/* Table readers */
int read_std_table( z_handle *z, int idx, table *t );
int read_map( z_handle *z, int idx, table *t );
int read_scene( z_handle *z, int idx, table *t );
/* Table writers */
int write_std_table( z_handle *z, int idx, table *t );
int write_map( z_handle *z, int idx, table *t );
int write_scene( z_handle *z, int idx, table *t );

/* Yaz0 */
int yaz0_get_size( uint8_t * src );
void yaz0_decode( uint8_t *src, uint8_t *dst, int maxsize );
void yaz0_encode( uint8_t * src, int src_size, uint8_t *dst, int *dst_size );

/* Global variables */
extern int hflag, vflag, iflag, rflag, fflag,dflag;
#endif