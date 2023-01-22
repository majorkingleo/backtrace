#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#ifdef linux
#include <execinfo.h>
#include <malloc.h>
#endif

/* #define DISABLE 1 * */

#ifndef DISABLE

static int setup_check_init=0;

typedef struct Slice
{
  void   *ptr;
  size_t  size;
  size_t  user_size;
  int     used;
  char    acBt[1024];
  struct Slice  *next;
} Slice;


static Slice * heap_start = NULL;
static Slice * slice_start = NULL;

static int heap_slice_size = 0xFFFF;
static int SLICE_NUM = 256;

void my_free( void * ptr );
void * my_malloc( size_t size );
void leak_check_init();

#ifdef linux
#  ifndef LIBFOLLOW
#    define LINUX_HOOK
#  endif
#endif

#ifdef LINUX_HOOK

void my_init_hook();

static void * (*old_malloc_hook)( size_t size, const void *ptr );
static void * (*old_free_hook)( void *ptr, const void * );
void (*__malloc_initialize_hook) (void) = my_init_hook;

void my_init_hook()
{
  leak_check_init();
}

static void * my_malloc_hook( size_t size, const void *ptr )
{
  return my_malloc( size );
}

static void my_free_hook( void *ptr, const void * foo )
{
  return my_free( ptr );
}

static void install_hooks()
{
  old_malloc_hook = __malloc_hook;
  old_free_hook = __free_hook;

  __malloc_hook = my_malloc_hook;
  __free_hook = my_free_hook;
}

static void remove_hooks()
{
  __malloc_hook = old_malloc_hook;
  __free_hook = old_free_hook;
}

void get_bt( char *buffer, size_t buf_size )
{
  void *array[200];
  size_t size;
  char **strings;
  size_t i;

  buffer[0] = '\0';

  remove_hooks();

  size = backtrace( array, 200 );
  strings = backtrace_symbols( array, size );  

  for( i = 0; i < size; i++ )
	{
	  if( buf_size - 1 < strlen( strings[i] ) )
		break;
	  
	  strcat( buffer, strings[i] );
	  strcat( buffer, "\n" );
	  buf_size -= strlen( strings[i] ) + 1;
	}

  free( strings );

  install_hooks();
}

void get_bt_fd( int fd )
{
  char acBuf[1024];

  get_bt( acBuf, sizeof( acBuf ) );

  write( fd, acBuf, strlen( acBuf ) );
}
#else
void free( void * ptr )
{
  return my_free( ptr );
}

void * malloc( size_t size )
{
  return my_malloc( size );
}

void get_bt_fd( int fd );
void get_bt( char *buffer, size_t buf_size );
#endif

static void * organice_mem( size_t size )
{
  void * addr = mmap( 0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 );

  if( addr == MAP_FAILED )
	return NULL;

  return addr;
}

static void unorganice_mem( void *adr, size_t size )
{
  munmap( adr, size );
}

static Slice * organice_heap_with_slice( size_t size )
{
  Slice * slice = (Slice*) organice_mem( sizeof( Slice ) );
  if( slice == NULL )
	return NULL;

  slice->ptr = organice_mem( size );

  if( slice->ptr == NULL )
	{
	  unorganice_mem( slice, sizeof(Slice) );
	  return NULL;
	}

  slice->size = size;
  slice->user_size = size;
  slice->used = 1;

  return slice;
}

static Slice * create_slice_chain()
{
  Slice *slice = organice_heap_with_slice( SLICE_NUM * sizeof( Slice ) + 100 );
  int i;
  Slice *mslice;

  if( slice == NULL )
	return NULL;

  memset( slice->ptr, 0, slice->size );
  
  mslice = slice->ptr;

  for( i = 0; i < SLICE_NUM-1; i++ )
	{
	  mslice[i].next = &mslice[i+1];
	}

/*
  Slice *s;

  for( s = (Slice*)slice->ptr; s; s = s->next )
	{
	  fprintf( stderr, "%p u: %d\n", s, s->used );
	}
  */
  return slice;
}

static void init_leack_checker( size_t heap_size )
{
  fprintf( stderr, "\n\n =========== MOBBI LEAK CHECKER ============= \n" );

#ifdef LINUX_HOOK
  install_hooks();
#endif

  heap_start = organice_heap_with_slice( heap_size );

  slice_start = create_slice_chain();

  if( slice_start == NULL || heap_start == NULL )
	{
	  fprintf( stderr, "no initial alloc space! heap: %p slice: %p errno: %s\n", 
			   heap_start, slice_start, strerror(errno) );
	}

  ((Slice*)slice_start->ptr)[0].size = heap_size;
  ((Slice*)slice_start->ptr)[0].ptr = heap_start->ptr;

  setup_check_init=1;
}

void leak_check_init()
{
  if( !setup_check_init )
	init_leack_checker( heap_slice_size ); 
}

static void report_leaks( Slice *master, FILE *where )
{
  int i;

  for( i = 0; i < SLICE_NUM; i++ )
	{
	  if( ((Slice*)master->ptr)[i].used )
		{
		  fprintf( where, "***** leak detected: %p size: %d user_size: %d from:\n%s\n", 
				   ((Slice*)master->ptr)[i].ptr,
				   ((Slice*)master->ptr)[i].size,
				   ((Slice*)master->ptr)[i].user_size,
				   ((Slice*)master->ptr)[i].acBt );
		}
	}
  if( master->next )
	report_leaks( master->next, where );
}

void report_current_leaks_to_file( FILE *where )
{
	report_leaks( slice_start, where );
}

static void free_slices( Slice *master )
{
  Slice *next = master->next;

  unorganice_mem( master, sizeof( Slice ) );

  if( next )
	free_slices( next );
}

void leak_check_exit()
{
  leak_check_init();

  report_leaks( slice_start, stderr );

  free_slices( heap_start );
  free_slices( slice_start );

#ifdef LINUX_HOOK
  remove_hooks();
#endif
}

static void attach_next_heap( Slice *master, Slice *next )
{
  if( master->next )
   {
	 attach_next_heap( master->next, next );
	 return;
   }

  master->next = next;
}

static Slice * find_free_slice( size_t min_size, Slice * master )
{
  Slice * ret = NULL, *next = NULL;
  int i;
  int cont=0;

  do {
	
	cont = 0;
#if 1
	for( i = 0; i < SLICE_NUM; i++ )
	  {
		if( !((Slice*)master->ptr)[i].used && ((Slice*)master->ptr)[i].size >= min_size )
		  {
			ret = &((Slice*)master->ptr)[i];
			
			if( i + 1 < SLICE_NUM )
			  next = &((Slice*)master->ptr)[i+1];

			/* nächsten slice entsprechend anpassen */
			if( next != NULL && next->ptr == NULL )
			  {
				next->ptr = ((char*)ret->ptr) + min_size;
				next->size = ret->size - min_size;
				ret->size = min_size;
				ret->user_size = min_size;
			  }
			
			ret->used = 1;
			
			return ret;
		  }	  
      }
#else
	Slice *s;

	for( s = (Slice*)master->ptr; s; s = s->next )
	  {
		if( !s->used && s->size >= min_size )
		  {
			if( s->next != NULL && s->next->ptr == NULL )
			  {
				s->next->ptr = ((char*)s->ptr) + min_size;
				s->next->size = s->size - min_size;
				s->size = min_size;
				s->user_size = min_size;
			  }

			s->used = 1;

			return s;
		  }
	  }

#endif



	if( master->next )
	  {
		master=master->next;
		cont = 1;
	  }

  } while( cont );

  /* fixme neuen slice und heap organisieren */

  size_t new_size = heap_slice_size;

  if( min_size > new_size )
	new_size = min_size * 2;

  Slice *new_heap = organice_heap_with_slice( new_size );

  if( new_heap == NULL )
	return NULL;

  Slice *new_slice = create_slice_chain(); 

  if( new_slice == NULL )
	return NULL;

  ((Slice*)new_slice->ptr)[0].size = new_size;
  ((Slice*)new_slice->ptr)[0].ptr = new_heap->ptr;

  master->next = new_slice;

  attach_next_heap( heap_start, new_heap );

  return find_free_slice( min_size, new_slice );
}

void * my_malloc( size_t size )
{
  leak_check_init();

  if( size == 0 )
	{
	  fprintf( stderr, "****** 0 allocated\n" );
	  get_bt_fd( 1 );
	  size = 1;
	}

  /* size += 10; *//* sicherheitsbuffer */

  Slice *slice = find_free_slice( size, slice_start );

  if( slice == NULL )
	{
	  fprintf( stderr, "****** out of mem!\n" );
	  get_bt_fd( 1 );
	  return NULL;
	}

  get_bt( slice->acBt, sizeof( slice->acBt ) );
  
  return slice->ptr;
}

static void free_slice_ptr( Slice *master, void * ptr )
{
  int i;
  int cont;

  do {

	cont = 0;

  for( i = 0; i < SLICE_NUM; i++ )
	{
	  if( ((Slice*)master->ptr)[i].ptr == ptr )
		{
		  if( ((Slice*)master->ptr)[i].used == 0 ) 
			{
			  fprintf( stderr, "******* double free detected!\n" );
			  get_bt_fd( 1 );
			  return;
			}
		  else
			{
			  ((Slice*)master->ptr)[i].used = 0;
			  return;
			}
		}
	}

  if( master->next )
	{
	  master = master->next;
	  cont = 1;
	}

  } while( cont );

  fprintf( stderr, "******* invalid free!\n" );
  get_bt_fd( 1 );	
}

void my_free( void * ptr )
{
  leak_check_init();

  if( ptr == NULL )
	return;

  free_slice_ptr( slice_start, ptr );
}

char *strdup( const char *str )
{
  size_t len;
  char *ret;

  leak_check_init();

  if( str == NULL )
	{
	  fprintf( stderr, "********* strdup with NULL pointer\n" );
	  get_bt_fd( 1 );
	  return NULL;
	}

  len = strlen( str ) + 1;
  ret = malloc( len );

  if( ret )
	strcpy( ret, str );

  return ret;
}

void * calloc( size_t nelem, size_t elsize )
{
  void * ret = malloc( nelem * elsize );

  if( ret )
	memset( ret, 0, nelem * elsize );

  return ret;
}

static void * leak_realloc( Slice *master, void *ptr, size_t new_size )
{
  int i;
  int cont;

  do {

	cont = 0;

	for( i = 0; i < SLICE_NUM; i++ )
	  {
		if( ((Slice*)master->ptr)[i].ptr == ptr )
		  {
			Slice *slice = &((Slice*)master->ptr)[i];
			
			if( slice->size >= new_size ) {
			  slice->user_size = new_size;
			  return ptr;
			 } else {
				Slice *new = find_free_slice( new_size, slice_start );
				memcpy( new->ptr, slice->ptr, slice->size );
				slice->used = 0;
				return new->ptr;
			  }
		  }
	  }

	if( master->next )
	  {
		master = master->next;
		cont = 1;
	  }

  } while( cont );
  
  return NULL;
}

void * realloc( void *ptr, size_t new_size )
{
  leak_check_init();

  if( ptr == NULL )
	return malloc( new_size );

  return leak_realloc( slice_start, ptr, new_size );
}

void *valloc( size_t size )
{
	leak_check_init();

	fprintf( stderr, "********* FIXME valloc *************\n" );

	return NULL;
}

void * memalign(size_t alignment, size_t size)
{
	leak_check_init();

	fprintf( stderr, "********* FIXME memalign *************\n" );

	return NULL;
}

size_t strlen( const char * ptr )
{
  int count = 0;

  if( ptr == NULL )
    {
      fprintf( stderr, "strlen with NULL Pointer\n" );
      get_bt_fd( 1 );
      return 0;
    }

  for( count=0; ptr[count] != '\0'; count++ );

  return count;
}

int strcmp( const char * a, const char * b )
{
  if( a == NULL || b == NULL )
    {
      fprintf( stderr, "strcmp with NULL Pointer strcmp(a,b) a: %p b: %p\n", a, b );
      get_bt_fd( 1 );
      return 1;
    }

  while (*a && *a == *b) {
        a++;
        b++;
  }
  return (*a - *b);
}

int strncmp( const char * a, const char * b, size_t len )
{
  const char* fini=a+len;

  if( a == NULL || b == NULL )
    {
      fprintf( stderr, "strncmp with NULL Pointer strcmp(a,b) a: %p b: %p\n", a, b );
      get_bt_fd( 1 );
      return 1;
    }

  while (a<fini) {
    int res=*a-*b;
    if (res) return res;
    if (!*a) return 0;
    ++a; ++b;
  }
  return 0;
}

#endif
