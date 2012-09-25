/*
 *  SSL server demonstration program
 *
 *  Copyright (C) 2006-2011, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/certs.h"
#include "polarssl/x509.h"
#include "polarssl/ssl.h"
#include "polarssl/net.h"
#include "polarssl/error.h"

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>PolarSSL Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"

/*
 * Computing a "safe" DH prime can take a very
 * long time. RFC 5114 provides precomputed and standardized
 * values.
 */
char *my_dhm_P = POLARSSL_DHM_RFC5114_MODP_1024_P;
char *my_dhm_G = POLARSSL_DHM_RFC5114_MODP_1024_G;

/*
 * Sorted by order of preference
 */
int my_ciphersuites[] =
{
#if defined(POLARSSL_DHM_C)
#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    SSL_EDH_RSA_AES_256_SHA256,
    SSL_EDH_RSA_AES_128_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_EDH_RSA_AES_256_SHA,
    SSL_EDH_RSA_AES_128_SHA,
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA4_C)
    SSL_EDH_RSA_AES_256_GCM_SHA384,
#endif
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    SSL_EDH_RSA_AES_128_GCM_SHA256,
#endif
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    SSL_EDH_RSA_CAMELLIA_256_SHA256,
    SSL_EDH_RSA_CAMELLIA_128_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_EDH_RSA_CAMELLIA_256_SHA,
    SSL_EDH_RSA_CAMELLIA_128_SHA,
#endif
#if defined(POLARSSL_DES_C)
    SSL_EDH_RSA_DES_168_SHA,
#endif
#endif

#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    SSL_RSA_AES_256_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_RSA_AES_256_SHA,
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    SSL_RSA_CAMELLIA_256_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_RSA_CAMELLIA_256_SHA,
#endif
#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    SSL_RSA_AES_128_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_RSA_AES_128_SHA,
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA4_C)
    SSL_RSA_AES_256_GCM_SHA384,
#endif
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    SSL_RSA_AES_128_GCM_SHA256,
#endif
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    SSL_RSA_CAMELLIA_128_SHA256,
#endif /* POLARSSL_SHA2_C */
    SSL_RSA_CAMELLIA_128_SHA,
#endif
#if defined(POLARSSL_DES_C)
    SSL_RSA_DES_168_SHA,
#endif
#if defined(POLARSSL_ARC4_C)
    SSL_RSA_RC4_128_SHA,
    SSL_RSA_RC4_128_MD5,
#endif
#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
#if defined(POLARSSL_DES_C)
    SSL_EDH_RSA_DES_SHA,
    SSL_RSA_DES_SHA,
#endif
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
    SSL_RSA_NULL_MD5,
    SSL_RSA_NULL_SHA,
    SSL_RSA_NULL_SHA256,
#endif
#endif
    0
};

#define DEBUG_LEVEL 0

void my_debug( void *ctx, int level, const char *str )
{
    if( level < DEBUG_LEVEL )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

/*
 * These session callbacks use a simple chained list
 * to store and retrieve the session information.
 */
ssl_session *s_list_1st = NULL;
ssl_session *cur, *prv;

static int my_get_session( ssl_context *ssl )
{
    time_t t = time( NULL );

    if( ssl->resume == 0 )
        return( 1 );

    cur = s_list_1st;
    prv = NULL;

    while( cur != NULL )
    {
        prv = cur;
        cur = cur->next;

        if( ssl->timeout != 0 && (int) ( t - prv->start ) > ssl->timeout )
            continue;

        if( ssl->session->ciphersuite != prv->ciphersuite ||
            ssl->session->length != prv->length )
            continue;

        if( memcmp( ssl->session->id, prv->id, prv->length ) != 0 )
            continue;

        memcpy( ssl->session->master, prv->master, 48 );
        return( 0 );
    }

    return( 1 );
}

static int my_set_session( ssl_context *ssl )
{
    time_t t = time( NULL );

    cur = s_list_1st;
    prv = NULL;

    while( cur != NULL )
    {
        if( ssl->timeout != 0 && (int) ( t - cur->start ) > ssl->timeout )
            break; /* expired, reuse this slot */

        if( memcmp( ssl->session->id, cur->id, cur->length ) == 0 )
            break; /* client reconnected */

        prv = cur;
        cur = cur->next;
    }

    if( cur == NULL )
    {
        cur = (ssl_session *) malloc( sizeof( ssl_session ) );
        if( cur == NULL )
            return( 1 );

        if( prv == NULL )
              s_list_1st = cur;
        else  prv->next  = cur;
    }

    memcpy( cur, ssl->session, sizeof( ssl_session ) );

    return( 0 );
}

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_CERTS_C) ||    \
    !defined(POLARSSL_ENTROPY_C) || !defined(POLARSSL_SSL_TLS_C) || \
    !defined(POLARSSL_SSL_SRV_C) || !defined(POLARSSL_NET_C) ||   \
    !defined(POLARSSL_RSA_C) || !defined(POLARSSL_CTR_DRBG_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_CERTS_C and/or POLARSSL_ENTROPY_C "
           "and/or POLARSSL_SSL_TLS_C and/or POLARSSL_SSL_SRV_C and/or "
           "POLARSSL_NET_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret, len;
    int listen_fd;
    int client_fd = -1;
    unsigned char buf[1024];
    char *pers = "ssl_server";

    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    ssl_context ssl;
    ssl_session ssn;
    x509_cert srvcert;
    rsa_context rsa;

    ((void) argc);
    ((void) argv);

    memset( &ssl, 0, sizeof( ssl_context ) );

    /*
     * 1. Load the certificates and private RSA key
     */
    printf( "\n  . Loading the server cert. and key..." );
    fflush( stdout );

    memset( &srvcert, 0, sizeof( x509_cert ) );

    /*
     * This demonstration program uses embedded test certificates.
     * Instead, you may want to use x509parse_crtfile() to read the
     * server and CA certificates, as well as x509parse_keyfile().
     */
    ret = x509parse_crt( &srvcert, (unsigned char *) test_srv_crt,
                         strlen( test_srv_crt ) );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    ret = x509parse_crt( &srvcert, (unsigned char *) test_ca_crt,
                         strlen( test_ca_crt ) );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    rsa_init( &rsa, RSA_PKCS_V15, 0 );
    ret =  x509parse_key( &rsa, (unsigned char *) test_srv_key,
                          strlen( test_srv_key ), NULL, 0 );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_key returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 2. Setup the listening TCP socket
     */
    printf( "  . Bind on https://localhost:4433/ ..." );
    fflush( stdout );

    if( ( ret = net_bind( &listen_fd, NULL, 4433 ) ) != 0 )
    {
        printf( " failed\n  ! net_bind returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 3. Seed the RNG
     */
    printf( "  . Seeding the random number generator..." );
    fflush( stdout );

    entropy_init( &entropy );
    if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy,
                               (unsigned char *) pers, strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 4. Setup stuff
     */
    printf( "  . Setting up the SSL data...." );
    fflush( stdout );

    if( ( ret = ssl_init( &ssl ) ) != 0 )
    {
        printf( " failed\n  ! ssl_init returned %d\n\n", ret );
        goto exit;
    }

    ssl_set_endpoint( &ssl, SSL_IS_SERVER );
    ssl_set_authmode( &ssl, SSL_VERIFY_NONE );

    ssl_set_rng( &ssl, ctr_drbg_random, &ctr_drbg );
    ssl_set_dbg( &ssl, my_debug, stdout );

    ssl_set_scb( &ssl, my_get_session,
                       my_set_session );

    ssl_set_ciphersuites( &ssl, my_ciphersuites );
    ssl_set_session( &ssl, 1, 0, &ssn );

    memset( &ssn, 0, sizeof( ssl_session ) );

    ssl_set_ca_chain( &ssl, srvcert.next, NULL, NULL );
    ssl_set_own_cert( &ssl, &srvcert, &rsa );
#if defined(POLARSSL_DHM_C)
    ssl_set_dh_param( &ssl, my_dhm_P, my_dhm_G );
#endif

    printf( " ok\n" );

reset:
#ifdef POLARSSL_ERROR_C
    if( ret != 0 )
    {
        char error_buf[100];
        error_strerror( ret, error_buf, 100 );
        printf("Last error was: %d - %s\n\n", ret, error_buf );
    }
#endif

    if( client_fd != -1 )
        net_close( client_fd );

    ssl_session_reset( &ssl );

    /*
     * 3. Wait until a client connects
     */
#if defined(_WIN32_WCE)
    {
        SHELLEXECUTEINFO sei;

        ZeroMemory( &sei, sizeof( SHELLEXECUTEINFO ) );

        sei.cbSize = sizeof( SHELLEXECUTEINFO );
        sei.fMask = 0;
        sei.hwnd = 0;
        sei.lpVerb = _T( "open" );
        sei.lpFile = _T( "https://localhost:4433/" );
        sei.lpParameters = NULL;
        sei.lpDirectory = NULL;
        sei.nShow = SW_SHOWNORMAL;

        ShellExecuteEx( &sei );
    }
#elif defined(_WIN32)
    ShellExecute( NULL, "open", "https://localhost:4433/",
                  NULL, NULL, SW_SHOWNORMAL );
#endif

    client_fd = -1;

    printf( "  . Waiting for a remote connection ..." );
    fflush( stdout );

    if( ( ret = net_accept( listen_fd, &client_fd, NULL ) ) != 0 )
    {
        printf( " failed\n  ! net_accept returned %d\n\n", ret );
        goto exit;
    }

    ssl_set_bio( &ssl, net_recv, &client_fd,
                       net_send, &client_fd );

    printf( " ok\n" );

    /*
     * 5. Handshake
     */
    printf( "  . Performing the SSL/TLS handshake..." );
    fflush( stdout );

    while( ( ret = ssl_handshake( &ssl ) ) != 0 )
    {
        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
            printf( " failed\n  ! ssl_handshake returned %d\n\n", ret );
            goto reset;
        }
    }

    printf( " ok\n" );

    /*
     * 6. Read the HTTP Request
     */
    printf( "  < Read from client:" );
    fflush( stdout );

    do
    {
        len = sizeof( buf ) - 1;
        memset( buf, 0, sizeof( buf ) );
        ret = ssl_read( &ssl, buf, len );

        if( ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE )
            continue;

        if( ret <= 0 )
        {
            switch( ret )
            {
                case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY:
                    printf( " connection was closed gracefully\n" );
                    break;

                case POLARSSL_ERR_NET_CONN_RESET:
                    printf( " connection was reset by peer\n" );
                    break;

                default:
                    printf( " ssl_read returned -0x%x\n", -ret );
                    break;
            }

            break;
        }

        len = ret;
        printf( " %d bytes read\n\n%s", len, (char *) buf );

        if( ret > 0 )
            break;
    }
    while( 1 );

    /*
     * 7. Write the 200 Response
     */
    printf( "  > Write to client:" );
    fflush( stdout );

    len = sprintf( (char *) buf, HTTP_RESPONSE,
                   ssl_get_ciphersuite( &ssl ) );

    while( ( ret = ssl_write( &ssl, buf, len ) ) <= 0 )
    {
        if( ret == POLARSSL_ERR_NET_CONN_RESET )
        {
            printf( " failed\n  ! peer closed the connection\n\n" );
            goto reset;
        }

        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
            printf( " failed\n  ! ssl_write returned %d\n\n", ret );
            goto exit;
        }
    }

    len = ret;
    printf( " %d bytes written\n\n%s\n", len, (char *) buf );
    
    ret = 0;
    goto reset;

exit:

#ifdef POLARSSL_ERROR_C
    if( ret != 0 )
    {
        char error_buf[100];
        error_strerror( ret, error_buf, 100 );
        printf("Last error was: %d - %s\n\n", ret, error_buf );
    }
#endif

    net_close( client_fd );
    x509_free( &srvcert );
    rsa_free( &rsa );
    ssl_session_free( &ssn );
    ssl_session_free( s_list_1st );
    ssl_free( &ssl );

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_CERTS_C && POLARSSL_ENTROPY_C &&
          POLARSSL_SSL_TLS_C && POLARSSL_SSL_SRV_C && POLARSSL_NET_C &&
          POLARSSL_RSA_C && POLARSSL_CTR_DRBG_C */
