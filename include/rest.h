/* rest.h
 * This is the main package file. Include this file in other projects.
 * Only modify inside the header-end and body-end sections.
 */

#ifndef CORTO_REST_H
#define CORTO_REST_H

#include "bake_config.h"

#define CORTO_REST_ETC ut_locate("corto.rest", NULL, UT_LOCATE_ETC)

/* $header() */
/* Enter additional code here. */
/* $end */

#include "_type.h"
#include "_interface.h"
#include "_load.h"
#include "_binding.h"

#include <corto.rest.c>

/* $body() */
/* Enter code that requires types here */
/* $end */

#endif

