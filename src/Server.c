/* $CORTO_GENERATED
 *
 * Server.c
 *
 * Only code written between the begin and end tags will be preserved
 * when the file is regenerated.
 */

#include <corto/rest/rest.h>

/* $header() */
#include "corto/fmt/json/json.h"

void rest_Server_apiGet(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
    corto_bool value = FALSE;
    corto_bool type = FALSE;
    corto_bool parent = FALSE;
    corto_bool name = FALSE;
    corto_bool owner = FALSE;
    corto_bool leaf = FALSE;
    corto_bool descriptor = FALSE;
    corto_uint64 offset = 0;
    corto_uint64 limit = 0;
    corto_string typeFilter = NULL;
    corto_string select = NULL;
    corto_int32 count = 0;
    corto_int16 ret;
    corto_iter iter;

    corto_buffer response = CORTO_BUFFER_INIT;

    /* Set correct content type */
    server_HTTP_Request_setHeader(
        r, "Content-Type", "application/json; charset=utf-8");

    /* Determine what to show */
    if (!strcmp(server_HTTP_Request_getVar(r, "value"), "true")) { value = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "type"), "true")) { type = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "parent"), "true")) { parent = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "name"), "true")) { name = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "owner"), "true")) { owner = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "leaf"), "true")) { leaf = TRUE; }
    if (!strcmp(server_HTTP_Request_getVar(r, "td"), "true")) { descriptor = TRUE; }
    offset = atoi(server_HTTP_Request_getVar(r, "offset"));
    limit = atoi(server_HTTP_Request_getVar(r, "limit"));
    typeFilter = server_HTTP_Request_getVar(r, "typeof");
    select = server_HTTP_Request_getVar(r, "select");
    corto_string contentType = value ? "text/corto" : NULL;

    if (!*typeFilter) typeFilter = NULL;
    if (!*select) select = NULL;

    {
        corto_id uriWithRoot;
        if (this->root) {
            sprintf(uriWithRoot, "%s/%s", this->root, uri);
            corto_cleanpath(uriWithRoot, uriWithRoot);
        } else {
            strcpy(uriWithRoot, *uri ? uri : "/");
        }

        corto_trace("REST: select('%s', '%s').limit(%d, %d).type('%s').contentType('%s')",
          uriWithRoot, select, offset, limit, typeFilter ? typeFilter : "*", contentType);

        ret = corto_select(uriWithRoot, select)
          .limit(offset, limit)
          .type(typeFilter)
          .contentType(contentType)
          .iter(&iter);
        if (ret) {
            server_HTTP_Request_setStatus(r, 400);
            server_HTTP_Request_reply(r, "400: bad request\n");
            return;
        };
    }

    /* Add object to result list */
    corto_buffer_appendstr(&response, "[");

    /* Collect types if typedescriptors are requested */
    corto_ll types = NULL;
    if (descriptor) {
        types = corto_llNew();
    }

    corto_resultIterForeach(iter, result) {
        if (count) {
            corto_buffer_append(&response, ",");
        }

        if (descriptor) {
            corto_iter it = corto_llIter(types);
            corto_bool found = FALSE;
            while (corto_iterHasNext(&it)) {
                if (!strcmp(corto_iterNext(&it), result.type)) {
                    found = TRUE;
                }
            }
            if (!found) {
                corto_llAppend(types, corto_strdup(result.type));
            }
        }

        corto_buffer_append(&response, "{\"id\":\"%s\"", result.id);
        if (parent && strcmp(result.parent, ".")) corto_buffer_append(&response, ",\"parent\":\"%s\"", result.parent);
        if (type) corto_buffer_append(&response, ",\"type\":\"%s\"", result.type);
        if (name && result.name) corto_buffer_append(&response, ",\"name\":\"%s\"", result.name);
        if (leaf) corto_buffer_append(&response, ",\"leaf\":%s", result.leaf ? "true" : "false");
        if (owner && result.owner) {
            corto_id id;
            char *escaped = id;
            corto_fullpath(id, result.owner);
            if (!corto_checkAttr(result.owner, CORTO_ATTR_SCOPED)) {
                int length = stresc(NULL, 0, id);
                escaped = corto_alloc(length + 1);
                stresc(escaped, length + 1, id);
            }
            corto_buffer_append(&response , ",\"owner\":\"%s\"", escaped);
            if (escaped != id) {
                corto_dealloc(escaped);
            }
        }

        if (value) {
            corto_string valueTxt = corto_result_getText(&result);
            if (valueTxt) {
                corto_buffer_append(&response, ",\"value\":%s", valueTxt);
            }
        }
        corto_buffer_append(&response, "}");
        count ++;
    }

    corto_buffer_append(&response, "]");
    corto_string responseStr = corto_buffer_str(&response);

    if (!count) {
        if (!select || (!strchr(select, '/') && !strchr(select, '*'))) {
            corto_string msg;
            corto_asprintf(&msg, "404: resource not found '%s'", uri);
            server_HTTP_Request_setStatus(r, 404);
            server_HTTP_Request_reply(r, msg);
            corto_dealloc(msg);
            corto_dealloc(responseStr);
            return;
        }
    }

    if (descriptor && corto_llSize(types)) {
        corto_buffer tdbuffer = CORTO_BUFFER_INIT;
        corto_buffer_append(&tdbuffer, "{\"o\":%s,\"t\":{", responseStr);
        corto_dealloc(responseStr);

        corto_iter it = corto_llIter(types);
        corto_bool first = TRUE;
        while (corto_iterHasNext(&it)) {
            corto_string typeId = corto_iterNext(&it);
            corto_type t = corto_resolve(NULL, typeId);
            if (t) {
                corto_string td = server_typedescriptor(t);
                if (!first) {
                    corto_buffer_appendstr(&tdbuffer, ",");
                } else {
                    first = FALSE;
                }
                corto_buffer_append(&tdbuffer, "\"%s\":%s", typeId, td);
                corto_dealloc(typeId);
            }
        }
        corto_buffer_appendstr(&tdbuffer, "}}");
        responseStr = corto_buffer_str(&tdbuffer);
        corto_llFree(types);
    }

    server_HTTP_Request_reply(r, responseStr);

    corto_dealloc(responseStr);
}

void rest_Server_apiPut(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
    char *value = server_HTTP_Request_getVar(r, "value");
    char *id = server_HTTP_Request_getVar(r, "id");

    corto_id realId;
    sprintf(realId, "/%s/%s/%s", server_Service(this)->prefix, uri, id);
    corto_cleanpath(realId, realId);

    corto_trace("REST: PUT uri='%s' id='%s' value = '%s' (computed id = %s)", uri, id, value, realId);

    if (corto_publish(CORTO_ON_UPDATE, realId, NULL, "text/json", value)) {
        corto_string msg;
        corto_asprintf(&msg, "400: PUT failed: %s: id=%s, value=%s",
          corto_lasterr(), id, value);
        server_HTTP_Request_setStatus(r, 400);
        server_HTTP_Request_reply(r, msg);
    } else {
        server_HTTP_Request_setStatus(r, 200);
        server_HTTP_Request_reply(r, "Ok\n");
    }
}

void rest_Server_apiPost(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
    char *id = server_HTTP_Request_getVar(r, "id");
    char *type = server_HTTP_Request_getVar(r, "type");
    char *value = server_HTTP_Request_getVar(r, "value");

    if (corto_publish(CORTO_ON_DEFINE, id, type, "text/json", value)) {
        corto_string msg;
        corto_asprintf(&msg, "400: POST failed: %s: id=%s, type=%s, value=%s",
          corto_lasterr(), id, type, value);
        server_HTTP_Request_setStatus(r, 400);
        server_HTTP_Request_reply(r, msg);
    } else {
        server_HTTP_Request_setStatus(r, 200);
        server_HTTP_Request_reply(r, "Ok\n");
    }
}

void rest_Server_apiDelete(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
    corto_string select = server_HTTP_Request_getVar(r, "select");
    if (corto_publish(CORTO_ON_DELETE, select, NULL, NULL, NULL)) {
        corto_string msg;
        corto_asprintf(&msg, "400: DELETE failed: %s", corto_lasterr());
        server_HTTP_Request_setStatus(r, 400);
        server_HTTP_Request_reply(r, msg);
    } else {
        server_HTTP_Request_setStatus(r, 200);
        server_HTTP_Request_reply(r, "Ok\n");
    }
}

/* $end */

int16_t _rest_Server_construct(
    rest_Server this)
{
/* $begin(corto/rest/Server/construct) */
    return server_Service_construct(this);
/* $end */
}

int16_t _rest_Server_onDelete(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
/* $begin(corto/rest/Server/onDelete) */
    rest_Server_apiDelete(this, c, r, uri);
    return 1;
/* $end */
}

int16_t _rest_Server_onGet(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
/* $begin(corto/rest/Server/onGet) */
    rest_Server_apiGet(this, c, r, uri);
    return 1;
/* $end */
}

int16_t _rest_Server_onPost(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
/* $begin(corto/rest/Server/onPost) */
    rest_Server_apiPost(this, c, r, uri);
    return 1;
/* $end */
}

int16_t _rest_Server_onPut(
    rest_Server this,
    server_HTTP_Connection c,
    server_HTTP_Request *r,
    corto_string uri)
{
/* $begin(corto/rest/Server/onPut) */
    rest_Server_apiPut(this, c, r, uri);
    return 1;
/* $end */
}
