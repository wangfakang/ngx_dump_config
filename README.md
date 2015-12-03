Name
====

ngx_dump_config_upstream - Nginx C module to expose Lua API to ngx_lua for dump Nginx upstreams config

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Synopsis](#synopsis)
* [Functions](#functions)
    * [dump_upstream](#dump_upstreams)
* [TODO](#todo)
* [Compatibility](#compatibility)
* [Installation](#installation)
* [Statement](#statement)
* [See Also](#see-also)

Status
======

This module is still under active development and is considered production ready.

Synopsis
========

```nginx
http {
    upstream foo.com {
        server 127.0.0.1 fail_timeout=53 weight=4 max_fails=100;
        server agentzh.org:81;
    }

    upstream bar {
        server 127.0.0.2;
    }

    server {
        listen 8080;

        # sample output for the following /upstream interface:
        # upstream foo.com:
        #     addr = 127.0.0.1:80, weight = 4, fail_timeout = 53, max_fails = 100
        #     addr = 106.187.41.147:81, weight = 1, fail_timeout = 10, max_fails = 1
        # upstream bar:
        #     addr = 127.0.0.2:80, weight = 1, fail_timeout = 10, max_fails = 1

        location = /upstreams {
            default_type text/plain;
            content_by_lua '
                local action = require "ngx.dump"
                local dump_upstream = action.dump_upstream
                local ok, err = dump_upstream()
                if not ok then 
                    ngx.say(err)
                end
            ';
        }
    }


}
```

Functions
=========

[Back to TOC](#table-of-contents)

dump_upstream
-------------
`syntax: ok, err = action.dump_upstream(filepath)`

 Dump conf from running nginx process (`but only dump upstream {}` blocks).
 Note not dump load balancing instruction about upstream
[Back to TOC](#table-of-contents)       



TODO
====

* dump location
* dump load balancing instruction about upstream

[Back to TOC](#table-of-contents)

Compatibility
=============

The following versions of OpenResty should work with this module:

* **1.7.10.x**  (last tested: 1.7.10.2)
* **1.7.7.x**   (last tested: 1.7.7.2)
* **1.7.4.1**  
* **1.7.2.1**  

[Back to TOC](#table-of-contents)

Installation
============

This module is bundled and enabled by default in the [OpenResty](http://openresty.org) bundle. And you are recommended to use OpenResty.

1. Grab the nginx source code from [nginx.org](http://nginx.org/), for example,
the version 1.5.12 (see [nginx compatibility](#compatibility)),
2. then grab the source code of the [ngx_lua](https://github.com/openresty/lua-nginx-module#installation) as well as its dependencies like [LuaJIT](http://luajit.org/download.html).
3. and finally build the source with this module:

```bash
wget 'http://nginx.org/download/nginx-1.5.12.tar.gz'
tar -xzvf nginx-1.5.12.tar.gz
cd nginx-1.5.12/

# assuming your luajit is installed to /opt/luajit:
export LUAJIT_LIB=/opt/luajit/lib

# assuming you are using LuaJIT v2.1:
export LUAJIT_INC=/opt/luajit/include/luajit-2.1

# Here we assume you would install you nginx under /opt/nginx/.
./configure --prefix=/opt/nginx \
    --with-ld-opt="-Wl,-rpath,$LUAJIT_LIB" \
    --add-module=/path/to/lua-nginx-module \
    --add-module=/path/to/ngx_dump-config-module

make -j2
make install
```

If you are using [ngx_openresty](http://openresty.org), then you can just add this module to OpenResty like this:

```bash
./configure --add-module=/path/to/ngx_dump_config_module
make -j2
make install
```

And you are all set. This module will get bundled into OpenResty in the near future.

[Back to TOC](#table-of-contents)


Statement 
========
* This module some api reference from [lua_upstream_ngx_module](https://github.com/openresty/lua-upstream-nginx-module) thx agentzh.
[Back to TOC](#table-of-contents)

See Also
========
* the ngx_lua module: http://github.com/openresty/lua-nginx-module#readme
[Back to TOC](#table-of-contents)

