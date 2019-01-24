/* This is a managed file. Do not delete this comment. */

#include <corto.rest>

void rest_service_apiGet(
    rest_service this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    const char *uri)
{
    corto_bool value = TRUE;
    corto_bool parent = FALSE;
    corto_bool name = FALSE;
    corto_bool owner = FALSE;
    corto_bool leaf = FALSE;
    corto_bool descriptor = FALSE;
    corto_uint64 offset = 0;
    corto_uint64 limit = 0;
    corto_string type = FALSE;
    corto_string select = NULL;
    corto_int32 count = 0;
    corto_int16 ret;
    ut_iter iter;

    ut_strbuf response = UT_STRBUF_INIT;

    /* Set correct content type */
    httpserver_HTTP_Request_setHeader(
        r, "Content-Type", "application/json; charset=utf-8");

    httpserver_HTTP_Request_setHeader(
        r, "Access-Control-Allow-Origin", "*");

    /* Determine what to show */
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "value"), "false")) { value = FALSE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "parent"), "true")) { parent = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "name"), "true")) { name = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "owner"), "true")) { owner = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "leaf"), "true")) { leaf = TRUE; }
    if (!strcmp(httpserver_HTTP_Request_getVar(r, "td"), "true")) { descriptor = TRUE; }

    offset = atoi(httpserver_HTTP_Request_getVar(r, "offset"));
    limit = atoi(httpserver_HTTP_Request_getVar(r, "limit"));
    type = httpserver_HTTP_Request_getVar(r, "type");
    select = httpserver_HTTP_Request_getVar(r, "select");
    corto_string format = value ? "text/json" : NULL;

    if (!*type) type = NULL;
    if (!*select) select = NULL;
    if (!select) {
        select = "*";
    }

    {
        corto_id uriWithRoot;
        if (this->from) {
            sprintf(uriWithRoot, "%s/%s", this->from, uri);
            ut_path_clean(uriWithRoot, uriWithRoot);
        } else {
            strcpy(uriWithRoot, *uri ? uri : "/");
        }

        ut_trace(
    "REST: select('%s').from('%s').limit(%d, %d).type('%s').format('%s')",
          select, uriWithRoot, offset, limit, type ? type : "*", format);

        ret = corto_select(select)
          .from(uriWithRoot)
          .offset(offset)
          .limit(limit)
          .type(type)
          .format(format)
          .iter(&iter);
        if (ret) {
            httpserver_HTTP_Request_setStatus(r, 400);
            httpserver_HTTP_Request_reply(r, "400: bad request\n");
            return;
        };
    }

    /* Add object to result list */
    ut_strbuf_appendstr(&response, "[");

    /* Collect types if typedescriptors are requested */
    ut_ll types = NULL;
    if (descriptor) {
        types = ut_ll_new();
    }

    while (ut_iter_hasNext(&iter)) {
        corto_record *result = ut_iter_next(&iter);
        if (count) {
            ut_strbuf_append(&response, ",");
        }

        if (descriptor) {
            ut_iter it = ut_ll_iter(types);
            corto_bool found = FALSE;
            while (ut_iter_hasNext(&it)) {
                if (!strcmp(ut_iter_next(&it), result->type)) {
                    found = TRUE;
                }
            }

            if (!found) {
                ut_ll_append(types, ut_strdup(result->type));
            }
        }

        if (strcmp(result->parent, ".")) {
            if (!parent) {
                ut_strbuf_append(
                    &response, "{\"id\":\"%s/%s\"", result->parent, result->id);
            } else {
                ut_strbuf_append(&response, "{\"id\":\"%s\"", result->id);
                ut_strbuf_append(
                    &response, ",\"parent\":\"%s\"", result->parent);
            }
        } else {
            ut_strbuf_append(&response, "{\"id\":\"%s\"", result->id);
        }
        ut_strbuf_append(&response, ",\"type\":\"%s\"", result->type);
        if (name && result->name) ut_strbuf_append(&response, ",\"name\":\"%s\"", result->name);
        if (leaf) ut_strbuf_append(&response, ",\"leaf\":%s", result->flags & CORTO_RECORD_LEAF ? "true" : "false");
        if (owner && result->owner) {
            corto_id id;
            char *escaped = id;
            corto_fullpath(id, result->owner);
            if (!corto_check_attr(result->owner, CORTO_ATTR_NAMED)) {
                int length = stresc(NULL, 0, '"', id);
                escaped = corto_alloc(length + 1);
                stresc(escaped, length + 1, '"', id);
            }

            ut_strbuf_append(&response , ",\"owner\":\"%s\"", escaped);
            if (escaped != id) {
                corto_dealloc(escaped);
            }
        }

        if (value) {
            corto_string valueTxt = corto_record_get_text(result);
            if (valueTxt) {
                ut_strbuf_append(&response, ",\"value\":%s", valueTxt);
            }
        }

        ut_strbuf_append(&response, "}");
        count ++;
    }

    ut_strbuf_append(&response, "]");
    corto_string responseStr = ut_strbuf_get(&response);
    if (!count) {
        if (!select || (!strchr(select, '/') && !strchr(select, '*'))) {
            corto_string msg = ut_asprintf("404: resource not found '%s'", uri);
            httpserver_HTTP_Request_setStatus(r, 404);
            httpserver_HTTP_Request_reply(r, msg);
            corto_dealloc(msg);
            corto_dealloc(responseStr);
            return;
        }
    }

    if (descriptor && ut_ll_count(types)) {
        ut_strbuf tdbuffer = UT_STRBUF_INIT;
        ut_strbuf_append(&tdbuffer, "{\"o\":%s,\"t\":{", responseStr);
        corto_dealloc(responseStr);
        ut_iter it = ut_ll_iter(types);
        corto_bool first = TRUE;
        while (ut_iter_hasNext(&it)) {
            corto_string typeId = ut_iter_next(&it);
            corto_type t = corto_resolve(NULL, typeId);
            if (t) {
                corto_string td = httpserver_typedescriptor(t);
                if (!first) {
                    ut_strbuf_appendstr(&tdbuffer, ",");
                } else {
                    first = FALSE;
                }

                ut_strbuf_append(&tdbuffer, "\"%s\":%s", typeId, td);
                corto_dealloc(typeId);
            }
        }

        ut_strbuf_appendstr(&tdbuffer, "}}");
        responseStr = ut_strbuf_get(&tdbuffer);
        ut_ll_free(types);
    }

    httpserver_HTTP_Request_reply(r, responseStr);
    corto_dealloc(responseStr);
}

static
void rest_service_apiPut(
    rest_service this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    const char *uri)
{
    char *value = httpserver_HTTP_Request_getVar(r, "value");
    char *id = httpserver_HTTP_Request_getVar(r, "id");

    corto_id realId;
    sprintf(realId, "/%s/%s", uri, id);
    ut_path_clean(realId, realId);

    ut_trace(
      "REST: PUT uri='%s' id='%s' value = '%s' (computed id = %s)",
      uri, id, value, realId);

    if (corto_publish(CORTO_UPDATE, this->from, realId, NULL, "text/json", value)) {
        corto_string msg;
        msg = ut_asprintf("400: PUT failed: %s: id=%s, value=%s",
          ut_lasterr(), id, value);
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }

}

static
void rest_service_apiPost(
    rest_service this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    const char *uri)
{
    char *id = httpserver_HTTP_Request_getVar(r, "id");
    char *type = httpserver_HTTP_Request_getVar(r, "type");
    char *value = httpserver_HTTP_Request_getVar(r, "value");

    corto_id realId;
    sprintf(realId, "/%s/%s", uri, id);
    ut_path_clean(realId, realId);

    ut_trace(
      "REST: POST uri='%s' id='%s' value = '%s' (computed id = %s)",
      uri, id, value, realId);

    if (corto_publish(CORTO_DEFINE, this->from, realId, type, "text/json", value)) {
        corto_string msg;
        msg = ut_asprintf("400: POST failed: %s: id=%s, type=%s, value=%s",
          ut_lasterr(), id, type, value);
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }

}

static
void rest_service_apiDelete(
    rest_service this,
    httpserver_HTTP_Connection c,
    httpserver_HTTP_Request *r,
    const char *uri)
{
    char *id = httpserver_HTTP_Request_getVar(r, "id");

    corto_id realId;
    sprintf(realId, "/%s/%s", uri, id);
    ut_path_clean(realId, realId);

    ut_trace(
      "REST: DELETE uri='%s' id='%s' (computed id = %s)",
      uri, id, realId);

    if (corto_publish(CORTO_DELETE, this->from, realId, NULL, NULL, NULL)) {
        corto_string msg;
        msg = ut_asprintf("400: DELETE failed: %s", ut_lasterr());
        httpserver_HTTP_Request_setStatus(r, 400);
        httpserver_HTTP_Request_reply(r, msg);
    } else {
        httpserver_HTTP_Request_setStatus(r, 200);
        httpserver_HTTP_Request_reply(r, "Ok\n");
    }

}

int16_t rest_service_construct(
    rest_service this)
{
    return httpserver_Service_construct(this);
}

int16_t rest_service_on_delete(
    rest_service this,
    corto_httpserver_HTTP_Connection c,
    corto_httpserver_HTTP_Request *r,
    const char *uri)
{
    rest_service_apiDelete(this, c, r, uri);
    return 1;
}

int16_t rest_service_on_get(
    rest_service this,
    corto_httpserver_HTTP_Connection c,
    corto_httpserver_HTTP_Request *r,
    const char *uri)
{
    rest_service_apiGet(this, c, r, uri);
    return 1;
}

int16_t rest_service_on_post(
    rest_service this,
    corto_httpserver_HTTP_Connection c,
    corto_httpserver_HTTP_Request *r,
    const char *uri)
{
    rest_service_apiPost(this, c, r, uri);
    return 1;
}

int16_t rest_service_on_put(
    rest_service this,
    corto_httpserver_HTTP_Connection c,
    corto_httpserver_HTTP_Request *r,
    const char *uri)
{
    rest_service_apiPut(this, c, r, uri);
    return 1;
}
