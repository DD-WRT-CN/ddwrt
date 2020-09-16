/* Dropbear SSH
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved. See LICENSE for the license. */

#ifndef DROPBEAR_OPTIONS_H_
#define DROPBEAR_OPTIONS_H_

/* Define compile-time options below - the "#ifndef DROPBEAR_XXX .... #endif"
 * parts are to allow for commandline -DDROPBEAR_XXX options etc. */

/* IMPORTANT: Many options will require "make clean" after changes */

#ifndef DROPBEAR_DEFPORT
#define DROPBEAR_DEFPORT "22"
#endif

#ifndef DROPBEAR_DEFADDRESS
/* Listen on all interfaces */
#define DROPBEAR_DEFADDRESS ""
#endif

/* Default hostkey paths - these can be specified on the command line */
#ifndef DSS_PRIV_FILENAME
#define DSS_PRIV_FILENAME "/tmp/root/.ssh/ssh_host_dss_key"
#endif
#ifndef RSA_PRIV_FILENAME
#define RSA_PRIV_FILENAME "/tmp/root/.ssh/ssh_host_rsa_key"
#endif
#ifndef ECDSA_PRIV_FILENAME
#define ECDSA_PRIV_FILENAME "/etc/dropbear/dropbear_ecdsa_host_key"
#endif
#ifndef ED25519_PRIV_FILENAME
#define ED25519_PRIV_FILENAME "/etc/dropbear/dropbear_ed25519_host_key"
#endif

/* Set NON_INETD_MODE if you require daemon functionality (ie Dropbear listens
 * on chosen ports and keeps accepting connections. This is the default.
 *
 * Set INETD_MODE if you want to be able to run Dropbear with inetd (or
 * similar), where it will use stdin/stdout for connections, and each process
 * lasts for a single connection. Dropbear should be invoked with the -i flag
 * for inetd, and can only accept IPv4 connections.
 *
 * Both of these flags can be defined at once, don't compile without at least
 * one of them. */
#define NON_INETD_MODE 1
//#define INETD_MODE

/* Setting this disables the fast exptmod bignum code. It saves ~5kB, but is
 * perhaps 20% slower for pubkey operations (it is probably worth experimenting
 * if you want to use this) */
/*#define NO_FAST_EXPTMOD*/

/* Set this if you want to use the DROPBEAR_SMALL_CODE option. This can save
several kB in binary size however will make the symmetrical ciphers and hashes
slower, perhaps by 50%. Recommended for small systems that aren't doing
much traffic. */
#define DROPBEAR_SMALL_CODE 1

/* Enable X11 Forwarding - server only */
#define DROPBEAR_X11FWD 0

/* Enable TCP Fowarding */
/* 'Local' is "-L" style (client listening port forwarded via server)
 * 'Remote' is "-R" style (server listening port forwarded via client) */

#define DROPBEAR_CLI_LOCALTCPFWD 1
#define DROPBEAR_CLI_REMOTETCPFWD 1

#define DROPBEAR_SVR_LOCALTCPFWD 1
#define DROPBEAR_SVR_REMOTETCPFWD 1

/* Enable Authentication Agent Forwarding */
#define DROPBEAR_SVR_AGENTFWD 1
#define DROPBEAR_CLI_AGENTFWD 1


/* Note: Both DROPBEAR_CLI_PROXYCMD and DROPBEAR_CLI_NETCAT must be set to
 * allow multihop dbclient connections */

/* Allow using -J <proxycommand> to run the connection through a 
   pipe to a program, rather the normal TCP connection */
#define DROPBEAR_CLI_PROXYCMD 1

/* Enable "Netcat mode" option. This will forward standard input/output
 * to a remote TCP-forwarded connection */
#define DROPBEAR_CLI_NETCAT 0

/* Whether to support "-c" and "-m" flags to choose ciphers/MACs at runtime */
#define DROPBEAR_USER_ALGO_LIST 0

/* Encryption - at least one required.
 * Protocol RFC requires 3DES and recommends AES128 for interoperability.
 * Including multiple keysize variants the same cipher 
 * (eg AES256 as well as AES128) will result in a minimal size increase.*/
#define DROPBEAR_AES128 1
#define DROPBEAR_3DES 0
#define DROPBEAR_AES256 1
/* Compiling in Blowfish will add ~6kB to runtime heap memory usage */
#define DROPBEAR_BLOWFISH 0 
#define DROPBEAR_TWOFISH256 0
#define DROPBEAR_TWOFISH128 0

/* Enable CBC mode for ciphers. This has security issues though
 * is the most compatible with older SSH implementations */
#define DROPBEAR_ENABLE_CBC_MODE 1

/* Enable "Counter Mode" for ciphers. This is more secure than normal
 * CBC mode against certain attacks. It is recommended for security
 * and forwards compatibility */
#define DROPBEAR_ENABLE_CTR_MODE 1

/* Enable "Galois/Counter Mode" for ciphers. This authenticated
 * encryption mode is combination of CTR mode and GHASH. Recommended
 * for security and forwards compatibility, but slower than CTR on
 * CPU w/o dedicated AES/GHASH instructions.
 * Compiling in will add ~6kB to binary size on x86-64 */
#define DROPBEAR_ENABLE_GCM_MODE 0

/* Enable Chacha20-Poly1305 authenticated encryption mode. This is
 * generally faster than AES256 on CPU w/o dedicated AES instructions,
 * having the same key size.
 * Compiling in will add ~5,5kB to binary size on x86-64 */
#define DROPBEAR_CHACHA20POLY1305 0

/* Twofish counter mode is disabled by default because it 
has not been tested for interoperability with other SSH implementations.
If you test it please contact the Dropbear author */
#define DROPBEAR_TWOFISH_CTR 0

/* You can compile with no encryption if you want. In some circumstances
 * this could be safe security-wise, though make sure you know what
 * you're doing. Anyone can see everything that goes over the wire, so
 * the only safe auth method is public key. */
#define DROPBEAR_NONE_CIPHER 0

/* Message Integrity - at least one required.
 * Protocol RFC requires sha1 and recommends sha1-96.
 * sha1-96 is of use for slow links as it has a smaller overhead.
 *
 * There's no reason to disable sha1 or sha1-96 to save space since it's
 * used for the random number generator and public-key cryptography anyway.
 * Disabling it here will just stop it from being used as the integrity portion
 * of the ssh protocol.
 *
 * These hashes are also used for public key fingerprints in logs.
 * If you disable MD5, Dropbear will fall back to SHA1 fingerprints,
 * which are not the standard form. */
#define DROPBEAR_SHA1_HMAC 1
#define DROPBEAR_SHA1_96_HMAC 0
#define DROPBEAR_SHA2_256_HMAC 1
#define DROPBEAR_SHA2_512_HMAC 0
#define DROPBEAR_MD5_HMAC 1

/* You can also disable integrity. Don't bother disabling this if you're
 * still using a cipher, it's relatively cheap. If you disable this it's dead
 * simple for an attacker to run arbitrary commands on the remote host. Beware. */
#define DROPBEAR_NONE_INTEGRITY 0

/* Hostkey/public key algorithms - at least one required, these are used
 * for hostkey as well as for verifying signatures with pubkey auth.
 * Removing either of these won't save very much space.
 * SSH2 RFC Draft requires dss, recommends rsa */
#define DROPBEAR_RSA 1
#define DROPBEAR_DSS 0
/* ECDSA is significantly faster than RSA or DSS. Compiling in ECC
 * code (either ECDSA or ECDH) increases binary size - around 30kB
 * on x86-64 */
#define DROPBEAR_ECDSA 0

/* Ed25519 is faster than ECDSA. Compiling in Ed25519 code increases
   binary size - around 7,5kB on x86-64 */
#define DROPBEAR_ED25519 0

#define DROPBEAR_DEFAULT_RSA_SIZE 2048

/* Generate hostkeys as-needed when the first connection using that key type occurs.
   This avoids the need to otherwise run "dropbearkey" and avoids some problems
   with badly seeded /dev/urandom when systems first boot.
   This also requires a runtime flag "-R". This adds ~4kB to binary size (or hardly 
   anything if dropbearkey is linked in a "dropbearmulti" binary) */
#define DROPBEAR_DELAY_HOSTKEY 1

/* Enable Curve25519 for key exchange. This is another elliptic
 * curve method with good security properties. Increases binary size
 * by ~8kB on x86-64 */
#define DROPBEAR_CURVE25519 0

/* Enable elliptic curve Diffie Hellman key exchange, see note about
 * ECDSA above */
#define DROPBEAR_ECDH 0

/* Group14 (2048 bit) is recommended. Group1 is less secure (1024 bit) though
   is the only option for interoperability with some older SSH programs */
#define DROPBEAR_DH_GROUP1 0
#define DROPBEAR_DH_GROUP16 0
#define DROPBEAR_DH_GROUP14_SHA1 1
#define DROPBEAR_DH_GROUP14_SHA256 1



/* Control the memory/performance/compression tradeoff for zlib.
 * Set windowBits=8 for least memory usage, see your system's
 * zlib.h for full details.
 * Default settings (windowBits=15) will use 256kB for compression
 * windowBits=8 will use 129kB for compression.
 * Both modes will use ~35kB for decompression (using windowBits=15 for
 * interoperability) */
#ifndef DROPBEAR_ZLIB_WINDOW_BITS
#define DROPBEAR_ZLIB_WINDOW_BITS 15 
#endif

/* Server won't allow zlib compression until after authentication. Prevents
   flaws in the zlib library being unauthenticated exploitable flaws.
   Some old ssh clients may not support the alternative zlib@openssh.com method */
#define DROPBEAR_SERVER_DELAY_ZLIB 1

/* Whether to do reverse DNS lookups. */
#define DO_HOST_LOOKUP 0

/* Whether to print the message of the day (MOTD). This doesn't add much code
 * size */
#define DO_MOTD 1
 
/* The MOTD file path */
#ifndef MOTD_FILENAME
#define MOTD_FILENAME "/etc/motd"
#endif

/* Authentication Types - at least one required.
   RFC Draft requires pubkey auth, and recommends password */

/* Note: PAM auth is quite simple and only works for PAM modules which just do
 * a simple "Login: " "Password: " (you can edit the strings in svr-authpam.c).
 * It's useful for systems like OS X where standard password crypts don't work
 * but there's an interface via a PAM module. It won't work for more complex
 * PAM challenge/response.
 * You can't enable both PASSWORD and PAM. */

/* Set this to 0 if your system does not have multiple user support.
   (Linux kernel CONFIG_MULTIUSER option)
   The resulting binary will not run on a normal system. */
#define DROPBEAR_SVR_MULTIUSER 1

/* This requires crypt() */
#define DROPBEAR_SVR_PASSWORD_AUTH 1
/* PAM requires ./configure --enable-pam */
#define DROPBEAR_SVR_PAM_AUTH 0
#define DROPBEAR_SVR_PUBKEY_AUTH 1

/* Whether to take public key options in 
 * authorized_keys file into account */
#ifdef DROPBEAR_SVR_PUBKEY_AUTH
#define DROPBEAR_SVR_PUBKEY_OPTIONS 1
#endif

/* This requires getpass. */
#define DROPBEAR_CLI_PASSWORD_AUTH 1
#define DROPBEAR_CLI_PUBKEY_AUTH 1

/* A default argument for dbclient -i <privatekey>. 
Homedir is prepended unless path begins with / */
#define DROPBEAR_DEFAULT_CLI_AUTHKEY ".ssh/id_dropbear"

/* This variable can be used to set a password for client
 * authentication on the commandline. Beware of platforms
 * that don't protect environment variables of processes etc. Also
 * note that it will be provided for all "hidden" client-interactive
 * style prompts - if you want something more sophisticated, use 
 * SSH_ASKPASS instead. Comment out this var to remove this functionality.*/
/*#define DROPBEAR_PASSWORD_ENV "DROPBEAR_PASSWORD"*/

/* Define this (as well as DROPBEAR_CLI_PASSWORD_AUTH) to allow the use of
 * a helper program for the ssh client. The helper program should be
 * specified in the SSH_ASKPASS environment variable, and dbclient
 * should be run with DISPLAY set and no tty. The program should
 * return the password on standard output */
/*#define DROPBEAR_CLI_ASKPASS_HELPER*/

/* Source for randomness. This must be able to provide hundreds of bytes per SSH
 * connection without blocking. In addition /dev/random is used for seeding
 * rsa/dss key generation */
#define DROPBEAR_URANDOM_DEV "/dev/urandom"

/* Set this to use PRNGD or EGD instead of /dev/urandom or /dev/random */
/*#define DROPBEAR_PRNGD_SOCKET "/var/run/dropbear-rng"*/


/* Specify the number of clients we will allow to be connected but
 * not yet authenticated. After this limit, connections are rejected */
/* The first setting is per-IP, to avoid denial of service */
#ifndef MAX_UNAUTH_PER_IP
#define MAX_UNAUTH_PER_IP 5
#endif

/* And then a global limit to avoid chewing memory if connections 
 * come from many IPs */
#ifndef MAX_UNAUTH_CLIENTS
#define MAX_UNAUTH_CLIENTS 30
#endif

/* Maximum number of failed authentication tries (server option) */
#ifndef MAX_AUTH_TRIES
#define MAX_AUTH_TRIES 3
#endif

/* The default file to store the daemon's process ID, for shutdown
   scripts etc. This can be overridden with the -P flag */
#ifndef DROPBEAR_PIDFILE
#define DROPBEAR_PIDFILE "/var/run/dropbear.pid"
#endif

/* The command to invoke for xauth when using X11 forwarding.
 * "-q" for quiet */
#ifndef XAUTH_COMMAND
#define XAUTH_COMMAND "/usr/bin/xauth -q"
#endif

/* if you want to enable running an sftp server (such as the one included with
 * OpenSSH), set the path below. If the path isn't defined, sftp will not
 * be enabled */
#ifndef SFTPSERVER_PATH
#define SFTPSERVER_PATH "/usr/libexec/sftp-server"
#endif

/* This is used by the scp binary when used as a client binary. If you're
 * not using the Dropbear client, you'll need to change it */
#define DROPBEAR_PATH_SSH_PROGRAM "/usr/bin/dbclient"

/* Whether to log commands executed by a client. This only logs the 
 * (single) command sent to the server, not what a user did in a 
 * shell/sftp session etc. */
#define LOG_COMMANDS 0

/* Window size limits. These tend to be a trade-off between memory
   usage and network performance: */
/* Size of the network receive window. This amount of memory is allocated
   as a per-channel receive buffer. Increasing this value can make a
   significant difference to network performance. 24kB was empirically
   chosen for a 100mbit ethernet network. The value can be altered at
   runtime with the -W argument. */
#ifndef DEFAULT_RECV_WINDOW
#define DEFAULT_RECV_WINDOW 24576
#endif
/* Maximum size of a received SSH data packet - this _MUST_ be >= 32768
   in order to interoperate with other implementations */
#ifndef RECV_MAX_PAYLOAD_LEN
#define RECV_MAX_PAYLOAD_LEN 32768
#endif
/* Maximum size of a transmitted data packet - this can be any value,
   though increasing it may not make a significant difference. */
#ifndef TRANS_MAX_PAYLOAD_LEN
#define TRANS_MAX_PAYLOAD_LEN 16384
#endif

/* Ensure that data is transmitted every KEEPALIVE seconds. This can
be overridden at runtime with -K. 0 disables keepalives */
#define DEFAULT_KEEPALIVE 0

/* If this many KEEPALIVES are sent with no packets received from the
other side, exit. Not run-time configurable - if you have a need
for runtime configuration please mail the Dropbear list */
#define DEFAULT_KEEPALIVE_LIMIT 3

/* Ensure that data is received within IDLE_TIMEOUT seconds. This can
be overridden at runtime with -I. 0 disables idle timeouts */
#define DEFAULT_IDLE_TIMEOUT 0

/* The default path. This will often get replaced by the shell */
#define DEFAULT_PATH "/sbin:/bin:/usr/sbin:/usr/bin:/jffs/sbin:/jffs/bin:/jffs/usr/sbin:/jffs/usr/bin:/mmc/sbin:/mmc/bin:/mmc/usr/sbin:/mmc/usr/bin:/opt/bin:/opt/sbin:/opt/usr/bin:/opt/usr/sbin"

/* Some other defines (that mostly should be left alone) are defined
 * in sysoptions.h */
#include "sysoptions.h"

#endif /* DROPBEAR_OPTIONS_H_ */
