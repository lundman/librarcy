
#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#define MIN(X,Y) ((X) <= (Y) ? (X) : (Y))

#define SAFE_FREE(X) { if ((X)) { free((X)); (X) = NULL; } }




void        xdebugf         ( const char const *fmt, ... );
char       *strip           ( char *s );
char       *misc_digtoken   ( char **string,char *match );
char       *misc_strjoin    ( char *a, char *b );
const char *my_basename     ( const char *s );
int         filename_is_rar ( const char *filename );
char       *rar_getpath     ( void );
int         skip_directory  ( __const char *__name );

#endif
