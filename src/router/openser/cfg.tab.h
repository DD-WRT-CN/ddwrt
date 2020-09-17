/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_CFG_TAB_H_INCLUDED
# define YY_YY_CFG_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    FORWARD = 258,
    FORWARD_TCP = 259,
    FORWARD_TLS = 260,
    FORWARD_UDP = 261,
    SEND = 262,
    SEND_TCP = 263,
    DROP = 264,
    EXIT = 265,
    RETURN = 266,
    RETCODE = 267,
    LOG_TOK = 268,
    ERROR = 269,
    ROUTE = 270,
    ROUTE_FAILURE = 271,
    ROUTE_ONREPLY = 272,
    ROUTE_BRANCH = 273,
    EXEC = 274,
    SET_HOST = 275,
    SET_HOSTPORT = 276,
    PREFIX = 277,
    STRIP = 278,
    STRIP_TAIL = 279,
    APPEND_BRANCH = 280,
    SET_USER = 281,
    SET_USERPASS = 282,
    SET_PORT = 283,
    SET_URI = 284,
    REVERT_URI = 285,
    SET_DSTURI = 286,
    RESET_DSTURI = 287,
    ISDSTURISET = 288,
    FORCE_RPORT = 289,
    FORCE_LOCAL_RPORT = 290,
    FORCE_TCP_ALIAS = 291,
    IF = 292,
    ELSE = 293,
    SWITCH = 294,
    CASE = 295,
    DEFAULT = 296,
    SBREAK = 297,
    SET_ADV_ADDRESS = 298,
    SET_ADV_PORT = 299,
    FORCE_SEND_SOCKET = 300,
    URIHOST = 301,
    URIPORT = 302,
    MAX_LEN = 303,
    SETFLAG = 304,
    RESETFLAG = 305,
    ISFLAGSET = 306,
    METHOD = 307,
    URI = 308,
    FROM_URI = 309,
    TO_URI = 310,
    SRCIP = 311,
    SRCPORT = 312,
    DSTIP = 313,
    DSTPORT = 314,
    PROTO = 315,
    AF = 316,
    MYSELF = 317,
    MSGLEN = 318,
    UDP = 319,
    TCP = 320,
    TLS = 321,
    DEBUG = 322,
    FORK = 323,
    LOGSTDERROR = 324,
    LOGFACILITY = 325,
    LOGNAME = 326,
    LISTEN = 327,
    ALIAS = 328,
    DNS = 329,
    REV_DNS = 330,
    DNS_TRY_IPV6 = 331,
    DNS_RETR_TIME = 332,
    DNS_RETR_NO = 333,
    DNS_SERVERS_NO = 334,
    DNS_USE_SEARCH = 335,
    PORT = 336,
    STAT = 337,
    CHILDREN = 338,
    CHECK_VIA = 339,
    SYN_BRANCH = 340,
    MEMLOG = 341,
    SIP_WARNING = 342,
    FIFO = 343,
    FIFO_DIR = 344,
    SOCK_MODE = 345,
    SOCK_USER = 346,
    SOCK_GROUP = 347,
    FIFO_DB_URL = 348,
    UNIX_SOCK = 349,
    UNIX_SOCK_CHILDREN = 350,
    UNIX_TX_TIMEOUT = 351,
    SERVER_SIGNATURE = 352,
    REPLY_TO_VIA = 353,
    LOADMODULE = 354,
    MPATH = 355,
    MODPARAM = 356,
    MAXBUFFER = 357,
    USER = 358,
    GROUP = 359,
    CHROOT = 360,
    WDIR = 361,
    MHOMED = 362,
    DISABLE_TCP = 363,
    TCP_ACCEPT_ALIASES = 364,
    TCP_CHILDREN = 365,
    TCP_CONNECT_TIMEOUT = 366,
    TCP_SEND_TIMEOUT = 367,
    DISABLE_TLS = 368,
    TLSLOG = 369,
    TLS_PORT_NO = 370,
    TLS_METHOD = 371,
    TLS_HANDSHAKE_TIMEOUT = 372,
    TLS_SEND_TIMEOUT = 373,
    SSLv23 = 374,
    SSLv2 = 375,
    SSLv3 = 376,
    TLSv1 = 377,
    TLS_VERIFY = 378,
    TLS_REQUIRE_CERTIFICATE = 379,
    TLS_CERTIFICATE = 380,
    TLS_PRIVATE_KEY = 381,
    TLS_CA_LIST = 382,
    TLS_CIPHERS_LIST = 383,
    ADVERTISED_ADDRESS = 384,
    ADVERTISED_PORT = 385,
    DISABLE_CORE = 386,
    OPEN_FD_LIMIT = 387,
    MCAST_LOOPBACK = 388,
    MCAST_TTL = 389,
    TLS_DOMAIN = 390,
    EQUAL = 391,
    EQUAL_T = 392,
    GT = 393,
    LT = 394,
    GTE = 395,
    LTE = 396,
    DIFF = 397,
    MATCH = 398,
    OR = 399,
    AND = 400,
    NOT = 401,
    PLUS = 402,
    MINUS = 403,
    NUMBER = 404,
    ID = 405,
    STRING = 406,
    IPV6ADDR = 407,
    COMMA = 408,
    SEMICOLON = 409,
    RPAREN = 410,
    LPAREN = 411,
    LBRACE = 412,
    RBRACE = 413,
    LBRACK = 414,
    RBRACK = 415,
    SLASH = 416,
    DOT = 417,
    CR = 418,
    COLON = 419,
    STAR = 420
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 119 "cfg.y" /* yacc.c:1909  */

	long intval;
	unsigned long uval;
	char* strval;
	struct expr* expr;
	struct action* action;
	struct net* ipnet;
	struct ip_addr* ipaddr;
	struct socket_id* sockid;

#line 231 "cfg.tab.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_CFG_TAB_H_INCLUDED  */
