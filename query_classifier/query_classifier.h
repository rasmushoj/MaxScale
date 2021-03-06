/*
This file is distributed as part of the SkySQL Gateway. It is free
software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation,
version 2.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51
Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

Copyright SkySQL Ab

*/

/** getpid */
#include <unistd.h>
#include "../utils/skygw_utils.h"

EXTERN_C_BLOCK_BEGIN

/**
 * Query type for skygateway.
 * The meaninful difference is where operation is done and whether master data
 * is modified
 */
typedef enum {
    QUERY_TYPE_UNKNOWN          = 0,      /*< Couln't find out or parse error */
    QUERY_TYPE_LOCAL_READ       = (1<<0), /*< Read non-database data, execute in MaxScale */
    QUERY_TYPE_READ             = (1<<1), /*< No updates */
    QUERY_TYPE_WRITE            = (1<<2), /*< Master data will be  modified */
    QUERY_TYPE_SESSION_WRITE    = (1<<3), /*< Session data will be modified */
    QUERY_TYPE_GLOBAL_WRITE     = (1<<4), /*< Global system variable modification */
    QUERY_TYPE_BEGIN_TRX        = (1<<5), /*< BEGIN or START TRANSACTION */
    QUERY_TYPE_ROLLBACK         = (1<<6), /*< ROLLBACK */
    QUERY_TYPE_COMMIT           = (1<<7), /*< COMMIT */
} skygw_query_type_t;

#define QUERY_IS_TYPE(mask,type) ((mask & type) == type)

skygw_query_type_t skygw_query_classifier_get_type(
        const char*   query_str,
        unsigned long client_flags);


EXTERN_C_BLOCK_END

