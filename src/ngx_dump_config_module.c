
#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include <ngx_core.h>
#include <ngx_http.h>
#include <lauxlib.h>
#include "ngx_http_lua_api.h"


ngx_module_t ngx_dump_config_module;


static ngx_int_t ngx_dump_config_upstream_init(ngx_conf_t *cf);
static int ngx_dump_config_upstream_create_module(lua_State * L); 
static int ngx_dump_config_upstreams(lua_State * L); 
static int ngx_dump_config_servers(lua_State * L, ngx_str_t *upstream, ngx_fd_t fd);
static ngx_http_upstream_main_conf_t *
    ngx_dump_config_get_upstream_main_conf(lua_State *L);
static int ngx_dump_config_create_dir(const char *path);
static int ngx_dump_config_remove_conf(const char *filepath);
static ngx_http_upstream_srv_conf_t *
    ngx_dump_config_find_upstream(lua_State *L, ngx_str_t *host);

static ngx_http_module_t ngx_dump_config_ctx = {
    NULL,                           /* preconfiguration */
    ngx_dump_config_upstream_init,  /* postconfiguration */
    NULL,                           /* create main configuration */
    NULL,                           /* init main configuration */
    NULL,                           /* create server configuration */
    NULL,                           /* merge server configuration */
    NULL,                           /* create location configuration */
    NULL                            /* merge location configuration */
};


ngx_module_t ngx_dump_config_module = {
    NGX_MODULE_V1,
    &ngx_dump_config_ctx,        /* module context */
    NULL,                        /* module directives */
    NGX_HTTP_MODULE,             /* module type */
    NULL,                        /* init master */
    NULL,                        /* init module */
    NULL,                        /* init process */
    NULL,                        /* init thread */
    NULL,                        /* exit thread */
    NULL,                        /* exit process */
    NULL,                        /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_dump_config_upstream_init(ngx_conf_t *cf)
{
    if (ngx_http_lua_add_package_preload(cf, "ngx.dump",
                                         ngx_dump_config_upstream_create_module)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static int
ngx_dump_config_upstream_create_module(lua_State * L)
{
    lua_createtable(L, 0, 1);

    lua_pushcfunction(L, ngx_dump_config_upstreams);
    lua_setfield(L, -2, "dump_upstream");

    return 1;
}

static int ngx_dump_config_create_dir(const char *path)
{
   int i, len;

   len = strlen(path);
   char dir_path[len + 1];
   dir_path[len] = '\0';

   ngx_memcpy(dir_path, path, len);

   for (i = 0; i < len; i++)
   {   
      if (dir_path[i] == '/' && i > 0)
      {   
         dir_path[i]='\0';
         if (access(dir_path, F_OK) < 0)
         {
            if (ngx_create_dir(dir_path, 0755) < 0)
            {
               return -1; 
            }
         }
         dir_path[i]='/';
      }   
   }   

   return 0;
}

static int
ngx_dump_config_remove_conf(const char *filepath)
{
    if (access(filepath, F_OK) ==  0) {
         return unlink(filepath);
    }
    
    return 0;
}

static int
ngx_dump_config_upstreams(lua_State * L)
{
    ngx_uint_t                            i;
    ngx_str_t                             file;
    ngx_http_upstream_srv_conf_t        **uscfp, *uscf;
    ngx_http_upstream_main_conf_t        *umcf;
    ngx_fd_t                              fd;
    char                                  buf[1024] = "";
    int                                   n;

    if (lua_gettop(L) != 1) {
        return luaL_error(L, "it's require one parameter");
    }

    file.data = (u_char *)luaL_checklstring(L, 1, &file.len); 
    if (file.len > sizeof(buf)) {
        lua_pushnil(L);
        lua_pushliteral(L, "conf file name too long");
        return 2;
    }
    
    ngx_memcpy(buf, file.data, file.len);

    umcf = ngx_dump_config_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;
    n = ngx_dump_config_create_dir(buf);  
    if (n == -1) {
        lua_pushnil(L);
        lua_pushliteral(L, "create dir fail");
        return 2;
    }

    n = ngx_dump_config_remove_conf(buf);
    if (n == -1) {
        lua_pushnil(L);
        lua_pushliteral(L, "remove conf fail");
        return 2;
    }

    fd = ngx_open_file(buf, NGX_FILE_WRONLY, NGX_FILE_CREATE_OR_OPEN, NGX_FILE_DEFAULT_ACCESS);
    if (fd == NGX_INVALID_FILE) {
        lua_pushnil(L);
        lua_pushliteral(L, "open file fail");
        return 2;
    }

    ngx_memset(buf, 0, sizeof(buf));
    lua_createtable(L, umcf->upstreams.nelts, 0);

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        uscf = uscfp[i];

        if (!uscf->port) {
            ngx_snprintf((u_char*)buf, sizeof(buf), "upstream  %V  {\n", &uscf->host);
            buf[sizeof(buf) - 1] = '\0';
            n = ngx_write_fd(fd, buf, ngx_strlen(buf));
            if (n == -1) {
                lua_pushnil(L);
                lua_pushliteral(L, "ngx_write file err");
                return 2;
            }
        
            ngx_memset(buf, 0, sizeof(buf));        
            ngx_dump_config_servers(L, &uscf->host, fd);
        }

    }

    close(fd);

    return 1;
}


static int
ngx_dump_config_servers(lua_State * L, ngx_str_t *upstream, ngx_fd_t fd)
{
    ngx_str_t                             host;
    ngx_uint_t                            i;
    ngx_http_upstream_server_t           *server;
    ngx_http_upstream_srv_conf_t         *us;
    u_char                                buf[1024];
    ssize_t                               n;

    host.data = upstream->data;
    host.len = upstream->len;

    us = ngx_dump_config_find_upstream(L, &host);
    if (us == NULL) {
        lua_pushnil(L);
        lua_pushliteral(L, "upstream not found");
        return 2;
    }

    if (us->servers == NULL || us->servers->nelts == 0) {
        n = ngx_write_fd(fd,"}\n\n",ngx_strlen("}\n\n"));
        if (n == -1) {
            lua_pushnil(L);
            lua_pushliteral(L, "ngx_write file err"); 
            return 2;
        }
        lua_newtable(L);
        return 1;
    }

    server = us->servers->elts;

    lua_createtable(L, us->servers->nelts, 0);

    ngx_memset(buf, 0, sizeof(buf));

    for (i = 0; i < us->servers->nelts; i++) {
        ngx_snprintf(buf, sizeof(buf), "    server    %V    weight=%d fail_timeout=%d max_fails=%d%s;\n", ngx_inet_addr(server[i].host.data, server[i].host.len) != INADDR_NONE ? (&server[i].addrs[0].name) : (&server[i].host), server[i].weight, server[i].fail_timeout, server[i].max_fails, server[i].backup ? " backup":"");
        buf[sizeof(buf) - 1] = '\0';
        n = ngx_write_fd(fd, buf, ngx_strlen(buf));
        if (n == -1) {
            lua_pushnil(L);
            lua_pushliteral(L, "ngx_write file err"); 
            return 2;
        }

        ngx_memset(buf, 0, sizeof(buf));
    }
    
    n = ngx_write_fd(fd,"}\n\n",ngx_strlen("}\n\n"));
    if (n == -1) {
        lua_pushnil(L);
        lua_pushliteral(L, "ngx_write file err"); 
        return 2;
    }

    return 1;
}


static ngx_http_upstream_main_conf_t *
ngx_dump_config_get_upstream_main_conf(lua_State *L)
{
    ngx_http_request_t                   *r;

    r = ngx_http_lua_get_request(L);

    if (r == NULL) {
        return ngx_http_cycle_get_module_main_conf(ngx_cycle,
                                                   ngx_http_upstream_module);
    }

    return ngx_http_get_module_main_conf(r, ngx_http_upstream_module);
}


static ngx_http_upstream_srv_conf_t *
ngx_dump_config_find_upstream(lua_State *L, ngx_str_t *host)
{
    u_char                               *port;
    size_t                                len;
    ngx_int_t                             n;
    ngx_uint_t                            i;
    ngx_http_upstream_srv_conf_t        **uscfp, *uscf;
    ngx_http_upstream_main_conf_t        *umcf;

    umcf = ngx_dump_config_get_upstream_main_conf(L);
    uscfp = umcf->upstreams.elts;

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        uscf = uscfp[i];

        if (uscf->host.len == host->len
            && ngx_memcmp(uscf->host.data, host->data, host->len) == 0) {
            return uscf;
        }
    }

    port = ngx_strlchr(host->data, host->data + host->len, ':');
    if (port) {
        port++;
        n = ngx_atoi(port, host->data + host->len - port);
        if (n < 1 || n > 65535) {
            return NULL;
        }

        /* try harder with port */

        len = port - host->data - 1;

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            uscf = uscfp[i];

            if (uscf->port
                && uscf->port == n
                && uscf->host.len == len
                && ngx_memcmp(uscf->host.data, host->data, len) == 0) {
                return uscf;
            }
        }
    }

    return NULL;
}


