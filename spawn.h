#ifndef SPAWN_H_INCLUDED
#define SPAWN_H_INCLUDED

#define RAR_SMALLSEEK (1024 * 1024)


void rar_expandlist ( rar_cache_t *cache, char *archive_name );
int  rar_despawn    ( rar_fdmap_t *fdmap );
int  rar_spawn      ( rar_fdmap_t *fdmap );
int  rar_openfile   ( rar_cache_t *cache, archive_t *archive, entry_t *entry );



#endif


