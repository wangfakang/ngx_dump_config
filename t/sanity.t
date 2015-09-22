# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_on();
#workers(2);
#log_level('warn');

repeat_each(1);

plan tests => repeat_each() * (blocks() * 3);

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

$ENV{TEST_NGINX_MY_INIT_CONFIG} = <<_EOC_;
lua_package_path "t/lib/?.lua;;";
_EOC_

#no_diff();
no_long_string();
run_tests();

__DATA__

=== TEST 1: get upstream names
--- http_config
    upstream foo.com:1234 {
        server 127.0.0.1;
    }

    upstream bar {
        server 127.0.0.2;
        server www.baidu.com;
    }
--- config
    location /t {
        content_by_lua '
            local action = require "ngx.dump"
            local dump_upstream = action.dump_upstream()
            local ok ,err = dump_upstream("/usr/local/conf/dump_upstream.conf")
            if not ok then 
                ngx.say(err)
            end
        ';
    }
--- request
    GET /t
--- response_body
--- no_error_log
[error]



