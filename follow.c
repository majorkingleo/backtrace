#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static int is_c_file( char* str )
{
    char *ret = strstr( str, ".c" );

    return  ret != NULL;
}

static int cmp_keyword( char *keyword, char *str )
{
    int i;
    
    for( i = 0;; i++ )
    {
	if( keyword[i] == '\0' )
	{
	    if( str[i] == '\0' )
		return  1;
	    
	    
	    if( !isalpha( str[i] ) && str[i] != '_' )
		return  1;
	    
	}  
	
	if( str[i] == '\0' )
	    return  0;
	
	
	if( keyword[i] != str[i] )
	    return  0;
	
    }
    
    return  1;
}

static int keyword( char *str, int is_function )
{
    if( !isalpha( *str ) )
       return  0;

    if( cmp_keyword( "enum", str ) ) return 1;
    if( cmp_keyword( "if", str ) ) return 1;
    if( cmp_keyword( "while", str ) ) return 1;
    if( !is_function && cmp_keyword( "struct", str ) ) return 1;
    if( cmp_keyword( "union", str ) ) return 1;
    if( cmp_keyword( "switch", str ) ) return 1;
    if( cmp_keyword( "for", str ) ) return 1;
    if( cmp_keyword( "#include", str ) ) return 1;
    if( cmp_keyword( "do", str ) ) return 1;
    
    return  0;
}

static int is_condition( char *str, int is_function )
{
    if( !isalpha( *str ) )
       return  0;

    if( cmp_keyword( "if", str ) ) return 1;
    if( cmp_keyword( "while", str ) ) return 1;
    if( cmp_keyword( "for", str ) ) return 1;
    if( cmp_keyword( "do", str ) ) return 1;
    
    return  0;
}

static int is_keyword( char *str, int is_function )
{
    if( !isalpha( *str ) )
       return  0;

    if( cmp_keyword( "enum", str ) ) return 1;
    if( cmp_keyword( "struct", str ) ) return 1;
    if( cmp_keyword( "class", str ) ) return 1;
    if( cmp_keyword( "union", str ) ) return 1;
    if( cmp_keyword( "switch", str ) ) return 1;
    if( cmp_keyword( "#include", str ) ) return 1;
    if( cmp_keyword( "const", str ) ) return 1;
    if( cmp_keyword( "int", str ) ) return 1;
    
    return  0;
}


static int parse_file( FILE *in, FILE *out )
{
    char *file = NULL;
    long file_size = 0;
    char *pos;
    char *start;
    char *file_end;
    
    
    fprintf( out, "#include \"bt.h\"\n" );
    
    fseek( in, 0, SEEK_END );
    file_size = ftell( in );
    fseek( in, 0, SEEK_SET );
    
    file = malloc( file_size + 1 );
    
    fread( file, file_size, 1, in );
    file[file_size] = '\0';    
    
    pos = file;
    file_end = file;
    file_end += file_size;

    while( pos != NULL && !(pos >= file_end ) )
    {
	char *rpos;
	int valid = 0;

	/* add BT */
	{
	    int pause = 0;
	    int is_function = 0;
	    
	    start = pos;
	    pos = index( pos, '{' );
	    
	    if( pos == NULL )
	    {
		fwrite( start, file_end - start, 1, out );
		break;
	    }
	    
	    rpos = pos;
	    
	    pause = 0;
	    while( rpos > file )
	    {
		if( *rpos == ')' )
		{
		    is_function = 1;
		    pause++;		
		} 

		if( *rpos == '(' )
		    pause--;

		if( !pause )
		{
		    if( *rpos == ';' || *rpos == '}' || *rpos == '#' || *rpos == '=' )
		    {
			if( cmp_keyword( "#endif", rpos ) )
			{
				valid = 1;
				break;
			}
			else if( cmp_keyword( "#define", rpos ) )
			{
			    valid = 0;
			    break;
			} else {
			    valid = 1;
			    break;
			}
		    }

		    if( keyword( rpos, is_function ) )
		    {
			valid = 0;
			break;
		    }
		}
		
		rpos--;
	    }
	    
	    fwrite( start, pos - start + 1, 1, out );
	    
	    if( valid && is_function )
	    {
		fprintf( out, "BT_ENTER;" );
	    } else {
		valid = 0;
	    }
	}

	
	/* ad BT_END */
	if( valid ){
	    int count = 0;

	    rpos = ++pos;

	    for(;;)
	    {
		if( rpos >= file_end )
		    break;

		if( *rpos == '}' && count <= 0 )
		{
		    
		    /* end found */
		    fwrite( pos, rpos - pos, 1, out );
		    fprintf( out, "BT_LEAVE;}" );
		    pos = rpos;		    
		    break;
		} else if( *rpos == '{' ) {
		    count++;
		} else if( *rpos == '}' ) {
		    count--;
		}
		rpos++;
	    }
	}

	pos++;
    }

    free( file );

    return  0;
}

static void handle_return( FILE *out )
{
    char *file = NULL;
    long file_size = 0;
    char *pos;
    char *start;
    char *file_end;
    int  skip = 0;

    fseek( out, 0, SEEK_END );
    file_size = ftell( out );
    fseek( out, 0, SEEK_SET );

    file = malloc( file_size + 1 );

    fread( file, file_size, 1, out );
    file[file_size] = '\0';

    fseek( out, 0, SEEK_SET );

    pos = file;
    file_end = file;
    file_end += file_size;

    while( pos != NULL && !(pos >= file_end ) )
    {
	char *rpos;

	if( !skip )
	    start = pos;

	pos = strstr( pos, "return" );

	if( pos == NULL )
	{
	    fwrite( start, file_end - start, 1, out );
	    break;
	}
	
	if( isalpha( *(pos-1) ) || *(pos-1) == '_' )
	{
	    pos += 6;
	    skip = 1;
	    continue;
	}


	if( isalpha( *(pos+6) ) || *(pos+6) == '_' )
	{
	    pos += 6;
	    skip = 1;
	    continue;
	}

	/* check if we are in a string */
	for( rpos = pos; *rpos != '\n'; rpos-- )
	{
	    if( *rpos == '"' )
	    {
		skip = 1;
		pos += 6;
		break;
	    }
	} 

	if( *rpos == '"' )
	    continue;

        /* check if we are in a comment */
	for( rpos = pos; rpos > file; rpos-- )
	{
	    if( *rpos == '/' && *(rpos + 1) == '*' )
	    {
		skip = 1;
		pos += 6;
		break;
	    }

	    if( *rpos == '*' && *(rpos + 1) == '/' )
	    {
		break;
	    }
	} 

	if( skip )
	{
	    pos++;
	    continue;
	}

	skip = 0;

	rpos = index( pos, ';' );

	if( rpos == NULL )
	{
	    fwrite( start, file_end - start, 1, out );
	    break;
	}

	rpos++;

	fwrite( start, pos - start, 1, out );

	fprintf( out, "{ BT_LEAVE; return " );
	fwrite( pos + 6, rpos - (pos + 6), 1, out ); 
	fprintf( out, "; }" );

	pos = rpos;
    }   

    free( file );
}

static int handle_condition( FILE *out )
{
    char *file = NULL;
    long file_size = 0;
    char *pos;
    char *start;
    char *file_end;
     
    fseek( out, 0, SEEK_END );
    file_size = ftell( out );
    fseek( out, 0, SEEK_SET );
    
    file = malloc( file_size + 1 );
    
    fread( file, file_size, 1, out );
    file[file_size] = '\0';    
    
    fseek( out, 0, SEEK_SET );

    pos = file;
    file_end = file;
    file_end += file_size;

    while( pos != NULL && !(pos >= file_end ) )
    {
	char *rpos;
	int valid = 0;
	int condition = 0;

	/* add BT */
	{
	    int pause = 0;
	    int is_function = 0;
	    
	    start = pos;
	    pos = index( pos, '{' );
	    
	    if( pos == NULL )
	    {
		fwrite( start, file_end - start, 1, out );
		break;
	    }
	    
	    rpos = pos;
	    
	    pause = 0;
	    while( rpos > file )
	    {
		if( *rpos == ')' )
		{
		    is_function = 1;
		    pause++;		
		} 

		if( *rpos == '(' )
		    pause--;

		if( !pause )
		{
		    if( *rpos == ';' || *rpos == '}' || *rpos == '#' || *rpos == '=' )
		    {
			if( cmp_keyword( "#define", rpos ) )
			{
			    valid = 0;
			    break;
			} else {
			    valid = 1;
			    break;
			}
		    }

		    if( is_keyword( rpos, is_function ) )
		    {
			valid = 0;
			break;
		    }

		    if( is_condition( rpos, is_function ) )
		    {
			condition = 1;
			valid = 1;
			break;
		    }
		}
		
		rpos--;
	    }
	    
	    fwrite( start, pos - start + 1, 1, out );
	    
	    if( valid && condition )
	    {
		fprintf( out, "BT_CONDITION;" );
	    } else {
		valid = 0;
	    }
	}

	pos++;
    }

    free( file );

    return 1;
}

static char * get_o_file( char *s )
{
    static char buffer[1024];

    strcat( buffer, s );
    buffer[strlen( buffer ) - 1] = 'o';

    return buffer;
}

int main( int argc, char **argv )
{
    int i;
    char **args;
    char *compiler = getenv( "FOLLOW_COMPILER" );
    char *follow_dir = getenv( "FOLLOW_DIR" );
	char *leakcheck = getenv( "FOLLOW_LEAKCHECK" );
    int ret;
    char *exec_buffer;
    int exec_len = 0;
    int link_mode = 1;
    int object_spec = 0;
    char *c_file = NULL;
    
    if( compiler == NULL )
    {
	printf( "compiler not set!\nSet the FOLLOW_COMPILER environment variable.\n" );
	return  2;
    }

    if( follow_dir == NULL )
    {
	printf( "FOLLOW_DIR is not set!\nSet the FOLLOW_DIR environment variable.\n" );
	return  2;
    }


    args = malloc( argc * sizeof( char* ) );

    for( i = 0; i < argc; i++ )
	args[i] = malloc( 1024 );

    for( i = 0; i < argc; i++ )
    {
	strcpy( args[i], argv[i] );

	if( strcmp( args[i], "-c" ) == 0 )
	    link_mode = 0;

	if( strcmp( args[i], "-o" ) == 0 )
	    object_spec = 1;

	if( is_c_file( argv[i] ) )
	{
	    char file[1024];
	    FILE *in = NULL;
	    FILE *out = NULL;

	    if( strcmp( argv[i], "bt.c" ) != 0 )
	    {
		c_file = argv[i];
		sprintf( file,  "/tmp/follow_%s", argv[i] );
		
		if( (out = fopen( file, "w" ) ) == NULL )
		{
		    perror( "cannot open output file" );
		    return  1;
		}
				
		if( (in = fopen( argv[i], "r" ) ) == NULL )
		{
		    perror( "cannot open input file" );
		    return  1;
		}

		parse_file( in, out );		
		
		fclose( in );
		fclose( out );

		if( (out = fopen( file, "r+" ) ) == NULL )
		{
		    perror( "cannot open output file" );
		    return  1;
		}

		handle_condition( out );
		handle_return( out );

		fclose( out );

		strcpy( args[i], file );
	    }
	}

	exec_len += strlen( args[i] );
    }

    exec_buffer = malloc( exec_len + strlen( compiler ) + argc + 2 + 3 + argc * 2 + 
			  strlen( follow_dir ) * 2 + 1000 );    

    strcat( exec_buffer, compiler );
    strcat( exec_buffer, " " );    

    for( i = 1; i < argc; i++ )
    {
	strcat( exec_buffer, "\'" );
	strcat( exec_buffer, args[i] );
	strcat( exec_buffer, "\'" );
	strcat( exec_buffer, " " );
    }

    strcat( exec_buffer, "-I. " );

    strcat( exec_buffer, "-I" );
    strcat( exec_buffer, follow_dir ); 

    if( link_mode )
    {
	strcat( exec_buffer, " -L" );
	strcat( exec_buffer, follow_dir ); 
		
	if( leakcheck != NULL && (strcmp( leakcheck, "1" ) == 0) )
		strcat( exec_buffer, " -lfollowleakcheck" ); 
	else
		strcat( exec_buffer, " -lfollow" ); 
    }

    if( !link_mode && !object_spec && c_file )
    {
	strcat( exec_buffer, " -o " );
	strcat( exec_buffer, get_o_file( c_file ) ); 
    }

    printf( "%s\n", exec_buffer );

    ret = system( exec_buffer );

    free( exec_buffer );

    for( i = 0; i < argc; i++ )
	free( args[i] );

    free( args );

    return  ret;
}
