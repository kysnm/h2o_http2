#include "h2o_http2.h"

#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "h2o.h"
#include "h2o/http2.h"

VALUE rb_mH2oHttp2;
VALUE cServer;

static h2o_globalconf_t config;
static h2o_context_t ctx;
static SSL_CTX *ssl_ctx;
static int port;

#if H2O_USE_LIBUV

static void on_accept(uv_stream_t *listener, int status)
{
  uv_tcp_t *conn;
  h2o_socket_t *sock;

  if (status != 0)
  return;

  conn = h2o_mem_alloc(sizeof(*conn));
  uv_tcp_init(listener->loop, conn);

  if (uv_accept(listener, (uv_stream_t *)conn) != 0) {
    uv_close((uv_handle_t *)conn, (uv_close_cb)free);
    return;
  }

  sock = h2o_uv_socket_create((uv_stream_t *)conn, NULL, 0, (uv_close_cb)free);
  if (ssl_ctx != NULL)
    h2o_accept_ssl(&ctx, ctx.globalconf->hosts, sock, ssl_ctx);
}

static int create_listener(int port)
{
  static uv_tcp_t listener;
  struct sockaddr_in addr;
  int r;

  uv_tcp_init(ctx.loop, &listener);
  uv_ip4_addr("127.0.0.1", port, &addr);
  if ((r = uv_tcp_bind(&listener, (struct sockaddr *)&addr, 0)) != 0) {
    fprintf(stderr, "uv_tcp_bind:%s\n", uv_strerror(r));
    goto Error;
  }
  if ((r = uv_listen((uv_stream_t *)&listener, 128, on_accept)) != 0) {
    fprintf(stderr, "uv_listen:%s\n", uv_strerror(r));
    goto Error;
  }

  return 0;
Error:
  uv_close((uv_handle_t *)&listener, NULL);
  return r;
}

#else

static void on_accept(h2o_socket_t *listener, int status)
{
  h2o_socket_t *sock;

  if (status == -1) {
    return;
  }

  if ((sock = h2o_evloop_socket_accept(listener)) == NULL) {
    return;
  }
  if (ssl_ctx != NULL)
    h2o_accept_ssl(&ctx, ctx.globalconf->hosts, sock, ssl_ctx);
}

static int create_listener(int port)
{
  struct sockaddr_in addr;
  int fd, reuseaddr_flag = 1;
  h2o_socket_t *sock;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ||
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_flag, sizeof(reuseaddr_flag)) != 0 ||
    bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, SOMAXCONN) != 0) {
    return -1;
  }

  sock = h2o_evloop_socket_create(ctx.loop, fd, (void *)&addr, sizeof(addr), H2O_SOCKET_FLAG_DONT_READ);
  h2o_socket_read_start(sock, on_accept);

  return 0;
}

#endif

static int setup_ssl(const char *cert_file, const char *key_file)
{
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();

  ssl_ctx = SSL_CTX_new(SSLv23_server_method());
  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);

  /* load certificate and private key */
  if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
    fprintf(stderr, "an error occurred while trying to load server certificate file:%s\n", cert_file);
    return -1;
  }
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
    fprintf(stderr, "an error occurred while trying to load private key file:%s\n", key_file);
    return -1;
  }

/* setup protocol negotiation methods */
#if H2O_USE_NPN
  h2o_ssl_register_npn_protocols(ssl_ctx, h2o_http2_npn_protocols);
#endif
#if H2O_USE_ALPN
  h2o_ssl_register_alpn_protocols(ssl_ctx, h2o_http2_alpn_protocols);
#endif

  return 0;
}

static void on_sigint(int signal)
{
#if H2O_USE_LIBUV
  uv_stop(ctx.loop);
#else
  h2o_evloop_destroy(ctx.loop);
#endif

  rb_interrupt();
}

static VALUE h2o_http2_init(int argc, VALUE *argv, VALUE self) {
  if (rb_scan_args(argc, argv, "01", &port) == 1) {
    port = FIX2INT(port);
  } else {
    port = 7890;
  }

  return self;
}

static VALUE h2o_http2_run(VALUE self)
{
  h2o_hostconf_t *hostconf;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, on_sigint);

  h2o_config_init(&config);
  hostconf = h2o_config_register_host(&config, "default");
  h2o_file_register(h2o_config_register_path(hostconf, "/"), "examples/doc_root", NULL, NULL, 0);

#if H2O_USE_LIBUV
  uv_loop_t loop;
  uv_loop_init(&loop);
  h2o_context_init(&ctx, &loop, &config);
#else
  h2o_context_init(&ctx, h2o_evloop_create(), &config);
#endif

  /* disabled by default: uncomment the block below to use HTTPS instead of HTTP */
  if (setup_ssl("server.crt", "server.key") != 0)
    goto Error;

  /* disabled by default: uncomment the line below to enable access logging */
  /* h2o_access_log_register(&config.default_host, "/dev/stdout", NULL); */

  if (create_listener(port) != 0) {
    fprintf(stderr, "failed to listen to 127.0.0.1:%i:%s\n", port, strerror(errno));
    goto Error;
  }

#if H2O_USE_LIBUV
  uv_run(ctx.loop, UV_RUN_DEFAULT);
#else
  while (h2o_evloop_run(ctx.loop) == 0)
    ;
#endif

Error:
  return 1;
}

void Init_h2o_http2(void)
{
  rb_mH2oHttp2 = rb_define_module("H2oHttp2");
  cServer = rb_define_class_under(rb_mH2oHttp2, "Server", rb_cObject);

  rb_define_method(cServer, "initialize", h2o_http2_init, -1);
  rb_define_method(cServer, "run", h2o_http2_run, 0);
}
