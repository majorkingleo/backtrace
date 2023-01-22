#ifndef BACKTRACE_H
#define BACKTRACE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void back_trace_enter( char *file, int line );
void back_trace_leave( char *file, int line );
void back_trace_condition( char *file, int line );

#define BT_ENTER \
  back_trace_enter( __FILE__, __LINE__ )

#define BT_CONDITION \
  back_trace_condition( __FILE__, __LINE__ )

#define BT_LEAVE \
  back_trace_leave( __FILE__, __LINE__ )


void leak_check_exit();
void report_current_leaks_to_file( FILE *where );

#ifdef __cplusplus
}
#endif

#endif
