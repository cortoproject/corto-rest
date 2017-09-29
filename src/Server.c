/* This is a managed file. Do not delete this comment. */

#include <corto/rest/rest.h>

#include "driver/fmt/json/json.h"

void rest_Server_apiGet(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    corto_bool value = TRUE;
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
    httpserver_HTTP_Request_setHeader(
        r, "Content-Type", "application/json; charset=utf-8");

    httpserver_HTTP_Request_setHeader(
        r, "Access-Control-Allow-Origin", "*");

    /* Determine what to show */
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "value"), "false")) { value = FALSE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "type"), "true")) { type = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "parent"), "true")) { parent = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "name"), "true")) { name = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "owner"), "true")) { owner = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "leaf"), "true")) { leaf = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "td"), "true")) { descriptor = TRUE; }
    offset = atoi(httpserver_HTTP_Request_getVar(r, "offset"));
    limit = atoi(httpserver_HTTP_Request_getVar(r, "limit"));
    typeFilter = httpserver_HTTP_Request_getVar(r, "typefilter");
    select = httpserver_HTTP_Request_getVar(r, "select");
    corto_string contentType = value ? "text/json" : NULL;

    if (!*typeFilter) typeFilter = NULL;
    if (!*select) select = NULL;

    if (!select) {
        select = "*";
    }

    {
        corto_id uriWithRoot;
        if (this->from) {
            sprintf(uriWithRoot, "%s/%s", this->from, uri);
            corto_cleanpath(uriWithRoot, uriWithRoot);
        } else {
            strcpy(uriWithRoot, *uri ? uri : "/");
        }

        corto_trace("REST: select('%s').from('%s').limit(%d, %d).type('%s').contentType('%s')",
          select, uriWithRoot, offset, limit, typeFilter ? typeFilter : "*", contentType);

        ret = corto_select(select)
          .from(uriWithRoot)
          .limit(offset, limit)
          .type(typeFilter)
          .contentType(contentType)
          .iter(&iter);
        if (ret) {
            httpserver_HTTP_Request_setStatus(r, 400);
            httpserver_HTTP_Request_reply(r, "400: bad request\n");
            return;
        };
    }

    /* Add object to result list */
    corto_buffer_appendstr(&response, "[");

    /* Collect types if typedescriptors are requested */
    corto_ll types = NULL;
    if (descriptor) {
        types = corto_ll_new();
    }

    corto_resultIterForeach(iter, result) {
        if (count) {
            corto_buffer_append(&response, ",");
        }

        if (descriptor) {
            corto_iter it = corto_ll_iter(types);
            corto_bool found = FALSE;
            while (corto_iter_hasNext(&it)) {
                if (!strcmp(corto_iter_next(&it), result.type)) {
                    found = TRUE;
                }
            }
            if (!found) {
                corto_ll_append(types, corto_strdup(result.type));
            }
        }

        corto_buffer_append(&response, "{\"id\":\"%s\"", result.id);
        if (parent && strcmp(result.parent, ".")) corto_buffer_append(&response, ",\"parent\":\"%s\"", result.parent);
        if (type) corto_buffer_append(&response, ",\"type\":\"%s\"", result.type);
        if (name && result.name) corto_buffer_append(&response, ",\"name\":\"%s\"", result.name);
        if (leaf) corto_buffer_append(&response, ",\"leaf\":%s", result.flags & CORTO_RESULT_LEAF ? "true" : "false");
        if (owner && result.owner) {
            corto_id id;
            char *escaped = id;
            corto_fullpath(id, result.owner);
            if (!corto_checkAttr(result.owner, CORTO_ATTR_NAMED)) {
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
            corto_string msg = corto_asprintf("404: resource not found '%s'", uri);
            httpserver_HTTP_Request_setStatus(r, 404);
            httpserver_HTTP_Request_reply(r, msg);
            corto_dealloc(msg);
            corto_dealloc(responseStr);
            return;
        }
    }

    if (descriptor && corto_ll_size(types)) {
        corto_buffer tdbuffer = CORTO_BUFFER_INIT;
        corto_buffer_append(&tdbuffer, "{\"o\":%s,\"t\":{", responseStr);
        corto_dealloc(responseStr);

        corto_iter it = corto_ll_iter(types);
        corto_bool first = TRUE;
        while (corto_iter_hasNext(&it)) {
            corto_string typeId = corto_iter_next(&it);
            corto_type t = corto_resolve(NULL, typeId);
            if (t) {
                corto_string td = httpserver_typedescriptor(t);
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
        corto_ll_free(types);
    }

    httpserver_HTTP_Request_reply(r, responseStr);

    corto_dealloc(responseStr);
}

void rest_Server_apiPut(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    char *value = httpserver_HTTP_Request_getVar(r, "value");
    char *id = httpserver_HTTP_Request_getVar(r, "id");

    corto_id realId;
    sprintf(realId, "/%s/%s/%s", httpserver_Service(this)->prefix, uri, id);
    corto_cleanpath(realId, realId);

    corto_trace("REST: PUT uri='%s' id='%s' value = '%s' (computed id = %s)", uri, id, value, realId);

    if (corto_publish(CORTO_UPDATE, realId, NULL, "text/json", value)) {
        corto_string msg;
        msg = corto_asprintf("400: PUT failed: %s: id=%s, value=%s",
          corto_lasterr(), id, value);
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }
}

void rest_Server_apiPost(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    char *id = httpserver_HTTP_Request_getVar(r, "id");
    char *type = httpserver_HTTP_Request_getVar(r, "type");
    char *value = httpserver_HTTP_Request_getVar(r, "value");

    if (corto_publish(CORTO_DEFINE, id, type, "text/json", value)) {
        corto_string msg;
        msg = corto_asprintf("400: POST failed: %s: id=%s, type=%s, value=%s",
          corto_lasterr(), id, type, value);
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }
}

void rest_Server_apiDelete(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    corto_string select = httpserver_HTTP_Request_getVar(r, "select");
    if (corto_publish(CORTO_DELETE, select, NULL, NULL, NULL)) {
        corto_string msg;
        msg = corto_asprintf("400: DELETE failed: %s", corto_lasterr());
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }
}


int16_t rest_Server_construct(
    rest_Server this)
{
    return httpserver_Service_construct(this);
}

int16_t rest_Server_onDelete(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    rest_Server_apiDelete(this, c, r, uri);
    return 1;
}

int16_t rest_Server_onGet(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    rest_Server_apiGet(this, c, r, uri);
    return 1;
}

int16_t rest_Server_onPost(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    rest_Server_apiPost(this, c, r, uri);
    return 1;
}

int16_t rest_Server_onPut(
    rest_Server this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    corto_string uri)
{
    rest_Server_apiPut(this, c, r, uri);
    return 1;
}

