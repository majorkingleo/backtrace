#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int handle = 0;

#ifdef LEAKCHECK
#define STACKSIZE 200
#define STACKBUFLEN 200
static int cond_handle = 0;
static char acStack[STACKSIZE][STACKBUFLEN];
#endif

static char * get_real_file( char *s )
{
    static int offset = strlen( "/tmp/follow_" );

    return s + offset;
}

#ifdef LEAKCHECK
void get_bt( char *buf, size_t len )
{
	int i;

	for( i = 0; i < handle + cond_handle; i++ )
	{
		int l = strlen( acStack[i] );
		if(  l + 1 > len )
			break;

		strcat( buf, acStack[i] );
		strcat( buf, "\n" );
		len -= l+1;
	}
}

void get_bt_fd( FILE *file )
{
	int i;

	for( i = 0; i < handle + cond_handle; i++ )
	{
		fprintf( file,  "%s\n", acStack[i] );
	}
}
#endif

static void log( char *file, int line, int what )
{
#ifdef LEAKCHECK
	if( handle + cond_handle >= STACKSIZE )
		return;

	if( what == 1 )
	{
		snprintf( acStack[handle + cond_handle], STACKBUFLEN-1, "%s%s:%d", "+", get_real_file( file ), line - 1 );
		acStack[handle + cond_handle][STACKBUFLEN-1] = '\0';
	} else if ( what == 2 ) {
		snprintf( acStack[handle + cond_handle], STACKBUFLEN-1, "%s%s:%d", "*", get_real_file( file ), line - 1 );
		acStack[handle + cond_handle][STACKBUFLEN-1] = '\0';
	}
#else
    static char buffer[1024] = {0};
    static int pid = 0;
    FILE *f = NULL;
    int h = 0;

    if( pid == 0 )
    {
	pid = getpid();
	sprintf( buffer, "/tmp/bt-%d.log", pid );
    }

    f = fopen( buffer, "a" );

    if( !f )
	return;

    if( what == 1 )
    {
	fprintf( f, "+" );
    } else if ( what == 0 ) {
	fprintf( f, "-" );
    } else {
	fprintf( f, "*" );
    }

    for( h = 0; h < handle; h++ )
    {
	fprintf( f, "\t" );
    }

    fprintf( f, "%s:%d\n", get_real_file( file ), line - 1 );

    fclose( f );
#endif
}

void back_trace_enter( char *file, int line )
{
#ifdef LEAKCHECK
	cond_handle=0;
#endif

    log( file, line, 1 );

    handle++;
}


void back_trace_leave( char *file, int line )
{
#ifdef LEAKCHECK
	cond_handle = 0;
#endif

    handle--;

    if( handle <= 0 )
	handle = 0;

    log( file, line, 0 );
}


void back_trace_condition( char *file, int line )
{
#ifdef LEAKCHECK
	cond_handle++;
#endif

    log( file, line, 2 );
}
