#ifndef _MYSQL_PROTOCOL_H
#define _MYSQL_PROTOCOL_H
/*
 * This file is distributed as part of the SkySQL Gateway.  It is free
 * software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright SkySQL Ab 2013
 */

/*
 * Revision History
 *
 * Date         Who                     Description
 * 01-06-2013   Mark Riddoch            Initial implementation
 * 14-06-2013   Massimiliano Pinto      Added specific data
 *                                      for MySQL session
 * 04-07-2013	Massimiliano Pinto	Added new MySQL protocol status for asynchronous connection
 *					Added authentication reply status
 * 12-07-2013	Massimiliano Pinto	Added routines for change_user
 * 14-02-2014	Massimiliano Pinto	setipaddress returns int
 * 25-02-2014	Massimiliano Pinto	Added dcb parameter to gw_find_mysql_user_password_sha1()
 * 					and repository to gw_check_mysql_scramble_data()
 * 					It's now possible to specify a different users' table than
 * 					dcb->service->users default
 * 26-02-2014	Massimiliano Pinto	Removed previouvsly added parameters to gw_check_mysql_scramble_data() and
 * 					gw_find_mysql_user_password_sha1()
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <service.h>
#include <router.h>
#include <poll.h>
#include <users.h>
#include <dbusers.h>
#include <version.h>

#define GW_MYSQL_VERSION "MaxScale " MAXSCALE_VERSION
#define GW_MYSQL_LOOP_TIMEOUT 300000000
#define GW_MYSQL_READ 0
#define GW_MYSQL_WRITE 1

#define GW_MYSQL_PROTOCOL_VERSION 10 // version is 10
#define GW_MYSQL_HANDSHAKE_FILLER 0x00
#define GW_MYSQL_SERVER_CAPABILITIES_BYTE1 0xff
#define GW_MYSQL_SERVER_CAPABILITIES_BYTE2 0xf7
#define GW_MYSQL_SERVER_LANGUAGE 0x08
#define GW_MYSQL_MAX_PACKET_LEN 0xffffffL;
#define GW_MYSQL_SCRAMBLE_SIZE 20
#define GW_SCRAMBLE_LENGTH_323 8

#ifndef MYSQL_SCRAMBLE_LEN
#define MYSQL_SCRAMBLE_LEN GW_MYSQL_SCRAMBLE_SIZE
#endif

#define GW_NOINTR_CALL(A)       do { errno = 0; A; } while (errno == EINTR)
// network buffer is 32K
#define MAX_BUFFER_SIZE 32768
// socket send buffer for backend
#define GW_BACKEND_SO_SNDBUF 1024
#define SMALL_CHUNK 1024
#define MAX_CHUNK SMALL_CHUNK * 8 * 4
#define ToHex(Y) (Y>='0'&&Y<='9'?Y-'0':Y-'A'+10)

struct dcb;

typedef enum {
    MYSQL_ALLOC,
    MYSQL_PENDING_CONNECT,
    MYSQL_CONNECTED,
    MYSQL_AUTH_SENT,
    MYSQL_AUTH_RECV,
    MYSQL_AUTH_FAILED,
    MYSQL_IDLE,
    MYSQL_ROUTING,
    MYSQL_WAITING_RESULT,
    MYSQL_SESSION_CHANGE
} mysql_pstate_t;


/*
 * MySQL Protocol specific state data
 */
typedef struct {
#if defined(SS_DEBUG)
        skygw_chk_t     protocol_chk_top;
#endif
	int		fd;                             /*< The socket descriptor */
 	struct dcb	*owner_dcb;                     /*< The DCB of the socket
                                                         * we are running on */
	mysql_pstate_t	state;                          /*< Current protocol state */
	uint8_t		scramble[MYSQL_SCRAMBLE_LEN];   /*< server scramble,
                                                         * created or received */
	uint32_t	server_capabilities;            /*< server capabilities,
                                                         * created or received */
	uint32_t	client_capabilities;            /*< client capabilities,
                                                         * created or received */
	unsigned	long tid;                       /*< MySQL Thread ID, in
                                                         * handshake */
#if defined(SS_DEBUG)
        skygw_chk_t     protocol_chk_tail;
#endif
} MySQLProtocol;

/*
 * MySQL session specific data
 *
 */
typedef struct mysql_session {
        uint8_t client_sha1[MYSQL_SCRAMBLE_LEN];        /*< SHA1(passowrd) */
        char user[MYSQL_USER_MAXLEN+1];                 /*< username       */
        char db[MYSQL_DATABASE_MAXLEN+1];               /*< database       */
} MYSQL_session;



/** Protocol packing macros. */
#define gw_mysql_set_byte2(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); } while (0)
#define gw_mysql_set_byte3(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); \
  (__buffer)[2]= (uint8_t)(((__int) >> 16) & 0xFF); } while (0)
#define gw_mysql_set_byte4(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); \
  (__buffer)[2]= (uint8_t)(((__int) >> 16) & 0xFF); \
  (__buffer)[3]= (uint8_t)(((__int) >> 24) & 0xFF); } while (0)

/** Protocol unpacking macros. */
#define gw_mysql_get_byte2(__buffer) \
  (uint16_t)((__buffer)[0] | \
            ((__buffer)[1] << 8))
#define gw_mysql_get_byte3(__buffer) \
  (uint32_t)((__buffer)[0] | \
            ((__buffer)[1] << 8) | \
            ((__buffer)[2] << 16))
#define gw_mysql_get_byte4(__buffer) \
  (uint32_t)((__buffer)[0] | \
            ((__buffer)[1] << 8) | \
            ((__buffer)[2] << 16) | \
            ((__buffer)[3] << 24))
#define gw_mysql_get_byte8(__buffer) \
  ((uint64_t)(__buffer)[0] | \
  ((uint64_t)(__buffer)[1] << 8) | \
  ((uint64_t)(__buffer)[2] << 16) | \
  ((uint64_t)(__buffer)[3] << 24) | \
  ((uint64_t)(__buffer)[4] << 32) | \
  ((uint64_t)(__buffer)[5] << 40) | \
  ((uint64_t)(__buffer)[6] << 48) | \
  ((uint64_t)(__buffer)[7] << 56))

/** MySQL protocol constants */
typedef enum
{
  GW_MYSQL_CAPABILITIES_NONE=                   0,
  GW_MYSQL_CAPABILITIES_LONG_PASSWORD=          (1 << 0),
  GW_MYSQL_CAPABILITIES_FOUND_ROWS=             (1 << 1),
  GW_MYSQL_CAPABILITIES_LONG_FLAG=              (1 << 2),
  GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB=        (1 << 3),
  GW_MYSQL_CAPABILITIES_NO_SCHEMA=              (1 << 4),
  GW_MYSQL_CAPABILITIES_COMPRESS=               (1 << 5),
  GW_MYSQL_CAPABILITIES_ODBC=                   (1 << 6),
  GW_MYSQL_CAPABILITIES_LOCAL_FILES=            (1 << 7),
  GW_MYSQL_CAPABILITIES_IGNORE_SPACE=           (1 << 8),
  GW_MYSQL_CAPABILITIES_PROTOCOL_41=            (1 << 9),
  GW_MYSQL_CAPABILITIES_INTERACTIVE=            (1 << 10),
  GW_MYSQL_CAPABILITIES_SSL=                    (1 << 11),
  GW_MYSQL_CAPABILITIES_IGNORE_SIGPIPE=         (1 << 12),
  GW_MYSQL_CAPABILITIES_TRANSACTIONS=           (1 << 13),
  GW_MYSQL_CAPABILITIES_RESERVED=               (1 << 14),
  GW_MYSQL_CAPABILITIES_SECURE_CONNECTION=      (1 << 15),
  GW_MYSQL_CAPABILITIES_MULTI_STATEMENTS=       (1 << 16),
  GW_MYSQL_CAPABILITIES_MULTI_RESULTS=          (1 << 17),
  GW_MYSQL_CAPABILITIES_PS_MULTI_RESULTS=       (1 << 18),
  GW_MYSQL_CAPABILITIES_PLUGIN_AUTH=            (1 << 19),
  GW_MYSQL_CAPABILITIES_SSL_VERIFY_SERVER_CERT= (1 << 30),
  GW_MYSQL_CAPABILITIES_REMEMBER_OPTIONS=       (1 << 31),
  GW_MYSQL_CAPABILITIES_CLIENT= (GW_MYSQL_CAPABILITIES_LONG_PASSWORD |
                                GW_MYSQL_CAPABILITIES_FOUND_ROWS |
                                GW_MYSQL_CAPABILITIES_LONG_FLAG |
                                GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB |
                                GW_MYSQL_CAPABILITIES_LOCAL_FILES |
                                GW_MYSQL_CAPABILITIES_PLUGIN_AUTH |
                                GW_MYSQL_CAPABILITIES_TRANSACTIONS |
                                GW_MYSQL_CAPABILITIES_PROTOCOL_41 |
                                GW_MYSQL_CAPABILITIES_MULTI_STATEMENTS |
                                GW_MYSQL_CAPABILITIES_MULTI_RESULTS |
                                GW_MYSQL_CAPABILITIES_PS_MULTI_RESULTS |
                                GW_MYSQL_CAPABILITIES_SECURE_CONNECTION),
  GW_MYSQL_CAPABILITIES_CLIENT_COMPRESS= (GW_MYSQL_CAPABILITIES_LONG_PASSWORD |
                                GW_MYSQL_CAPABILITIES_FOUND_ROWS |
                                GW_MYSQL_CAPABILITIES_LONG_FLAG |
                                GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB |
                                GW_MYSQL_CAPABILITIES_LOCAL_FILES |
                                GW_MYSQL_CAPABILITIES_PLUGIN_AUTH |
                                GW_MYSQL_CAPABILITIES_TRANSACTIONS |
                                GW_MYSQL_CAPABILITIES_PROTOCOL_41 |
                                GW_MYSQL_CAPABILITIES_MULTI_STATEMENTS |
                                GW_MYSQL_CAPABILITIES_MULTI_RESULTS |
                                GW_MYSQL_CAPABILITIES_PS_MULTI_RESULTS |
                                GW_MYSQL_CAPABILITIES_COMPRESS
                                ),
} gw_mysql_capabilities_t;

/** Basic mysql commands */
#define MYSQL_COM_CHANGE_USER 0x11
#define MYSQL_COM_QUIT        0x1
#define MYSQL_COM_INIT_DB     0x2
#define MYSQL_COM_QUERY       0x3

#define MYSQL_GET_COMMAND(payload) (payload[4])
#define MYSQL_GET_PACKET_NO(payload) (payload[3])
#define MYSQL_GET_PACKET_LEN(payload) (gw_mysql_get_byte3(payload))

#endif

void gw_mysql_close(MySQLProtocol **ptr);
MySQLProtocol* mysql_protocol_init(DCB* dcb, int fd);
MySQLProtocol *gw_mysql_init(MySQLProtocol *data);
void gw_mysql_close(MySQLProtocol **ptr);
int  gw_receive_backend_auth(MySQLProtocol *protocol);
int  gw_decode_mysql_server_handshake(MySQLProtocol *protocol, uint8_t *payload);
int  gw_read_backend_handshake(MySQLProtocol *protocol);
int  gw_send_authentication_to_backend(
        char *dbname,
        char *user,
        uint8_t *passwd,
        MySQLProtocol *protocol);
const char *gw_mysql_protocol_state2string(int state);
int gw_do_connect_to_backend(char *host, int port, int* fd);
int mysql_send_custom_error (
        DCB *dcb,
        int packet_number,
        int in_affected_rows,
        const char* mysql_message);
int gw_send_change_user_to_backend(
        char *dbname,
        char *user,
        uint8_t *passwd,
        MySQLProtocol *protocol);
int gw_find_mysql_user_password_sha1(
        char *username,
        uint8_t *gateway_password,
	DCB *dcb);
int gw_check_mysql_scramble_data(
        DCB *dcb,
        uint8_t *token,
        unsigned int token_len,
        uint8_t *scramble,
        unsigned int scramble_len,
        char *username,
        uint8_t *stage1_hash);
int mysql_send_auth_error (
        DCB *dcb,
        int packet_number,
        int in_affected_rows,
        const char* mysql_message);

void gw_sha1_str(const uint8_t *in, int in_len, uint8_t *out);
void gw_sha1_2_str(
        const uint8_t *in,
        int in_len,
        const uint8_t *in2,
        int in2_len,
        uint8_t *out);
void gw_str_xor(
        uint8_t       *output,
        const uint8_t *input1,
        const uint8_t *input2,
        unsigned int  len);
char	*gw_bin2hex(char *out, const uint8_t *in, unsigned int len);
int	gw_hex2bin(uint8_t *out, const char *in, unsigned int len);
int	gw_generate_random_str(char *output, int len);
char	*gw_strend(register const char *s);
int	setnonblocking(int fd);
int	setipaddress(struct in_addr *a, char *p);
int	gw_read_gwbuff(DCB *dcb, GWBUF **head, int b);
