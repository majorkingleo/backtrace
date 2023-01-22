#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *bla;

#define PUBLIC

void bar()
{
	malloc( 10 );

	if( 1 > 0 )
	{
		bla = malloc( 200 );
	}
	bla = 0;
}

void klo()
{
	if( 1 < 2 )
	{
		bar();
	}
}

#ifdef _LINUX_
    void ctSetup();
#endif


PUBLIC void main(int argc, char **argv)
{
    int rv;
	char *foo = malloc( 10 );

	sprintf( foo, "hello" );

	klo();

	strcmp( bla, "hello" ); 

	/* this is important at program exit */
	leak_check_exit();

	return 0;
}


