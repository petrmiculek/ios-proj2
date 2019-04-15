/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */

#include "error_msg.h"
#include <stdarg.h> // variadic
#include <stdio.h>
#include <stdlib.h> // exit(n)


void warning_msg(const char *fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf (stderr, fmt, args);
    va_end (args);

}


void error_exit(const char *fmt, ...)
{

    va_list args;
    va_start (args, fmt);
    warning_msg(fmt, args);
    exit(1);
}

