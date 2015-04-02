#include "h2o_http2.h"

VALUE rb_mH2oHttp2;

void
Init_h2o_http2(void)
{
  rb_mH2oHttp2 = rb_define_module("H2oHttp2");
}
