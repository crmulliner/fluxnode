/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/sens_periph.h"
#include "soc/rtc.h"
#include "freertos/queue.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include <esp_http_server.h>

#include "log.h"
#include "vfs_config.h"
#include "web_service.h"

//#define WEBSERV_DEBUG 1

/* jsondoc
{
"class": "Web Service",
"longtext": "
The Web Service provides a minimal HTTP server to implement a web application.
The web service includes a websocket server that allows for one connection. 
The websocket connection is transparently provided to the JavaScript 
runtime via the OnEvent() function.

The websocket server is listening on port `8888`.

`192.168.4.1` is the default IP for Fluxn0de in Wifi AP mode.

The `web_mode` parameter of Platform.wifiConfigure() allows to disable `/control` and 
POST to `/file`.

## API

The API will not allow to read and write files that start with `_`.
This allows the device to store `private` data.

### URL: /

- GET will read /index.html

### /file?\\<filename\\>

- GET to read \\<filename\\>
- POST to write to \\<filename\\>

### /control?\\<command\\>

- GET to execute the \\<command\\>

Commands:
- **reset** (reset the JavaScript runtime, same as Platform.reset())
- **setload=\\<filename\\>** (set the load file, same as Platform.setLoadFileName(filename))
- **reboot** (reboot the board, same as Platform.reboot())
- **deletefile=\\<filename\\>** (delete \\<filename\\>, same as FileSystem.unlink(filename))

Example:
```

Upload test42.js to the board.
$ http://192.168.4.1/file?test42.js --data-binary @test42.js
OK

Set /test42.js as the application to run
$ http://192.168.4.1/control?setload=/test42.js
OK

Reset JavaScript runtime and run test42.js
$ curl http://192.168.4.1/control?reset
OK

```

"
}
*/

#define FS_BASE "/"
#define BUF_SIZE 1024
#define FILE_URI "/file"
#define CONTROL_URI "/control"
#define PROTECTED_FILE '_'

struct control_name_t
{
    char name[16];
    control_cmd_t id;
};

static struct control_name_t control_names[] = {
    {"reset", CMD_RESET},
    {"setload", CMD_SET_LOAD},
    {"reboot", CMD_REBOOT},
    {"deletefile", CMD_DELETE_FILE},
    {"", CMD_NONE},
};

static int running = 0;
static httpd_handle_t server;
static webserver_control_function_t *control_func = NULL;

void webserver_set_control_func(webserver_control_function_t *func)
{
    control_func = func;
}

static esp_err_t post_handler(httpd_req_t *req)
{
    size_t url_len = httpd_req_get_url_query_len(req);
    char *url = malloc(url_len + 1);
    httpd_req_get_url_query_str(req, url, url_len + 1);
#ifdef WEBSERV_DEBUG
    logprintf("%s POST '%s'\n", __func__, url);
#endif

    if (url[0] == PROTECTED_FILE)
    {
        free(url);
        httpd_resp_sendstr(req, "Access Denied");
        return ESP_FAIL;
    }

    char *fname = malloc(sizeof(FS_BASE) + url_len + 1);
    sprintf(fname, "%s%s", FS_BASE, url);
    free(url);

    FILE *fp = fopen(fname, "w+");
    if (fp == NULL)
    {
#ifdef WEBSERV_DEBUG
        logprintf("%s: can't create file '%s'\n", __func__, fname);
#endif
        free(fname);
        httpd_resp_sendstr(req, "FileCreateError");
        return ESP_FAIL;
    }
    else
    {
#ifdef WEBSERV_DEBUG
        logprintf("%s: writing %s with %d bytes\n", __func__, fname, req->content_len);
#endif
    }
    free(fname);

    char *buf = malloc(BUF_SIZE);
    int bread = 0;
    for (;;)
    {
        int ret = httpd_req_recv(req, buf, BUF_SIZE);
        if (ret <= 0)
        { // 0 return value indicates connection closed
            // Check if timeout occurred
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }
            free(buf);
            fclose(fp);
            httpd_resp_sendstr(req, "Error");
            return ESP_FAIL;
        }
        fwrite(buf, ret, 1, fp);
        bread += ret;
        if (bread >= req->content_len)
        {
            // add a new line at the end of the file, for the Duktape parser
            fwrite("\n", 1, 1, fp);
            break;
        }
    }
    fclose(fp);
    free(buf);
    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

static esp_err_t get_file_by_name(httpd_req_t *req, const char *name)
{
    char *fname = malloc(sizeof(FS_BASE) + strlen(name) + 1);
    sprintf(fname, "%s%s", FS_BASE, name);
#ifdef WEBSERV_DEBUG
    logprintf("%s read file '%s'\n", __func__, fname);
#endif
    FILE *fp = fopen(fname, "r");
    free(fname);
    if (fp == NULL)
    {
#ifdef WEBSERV_DEBUG
        logprintf("%s: file does not exist\n", __func__);
#endif
        httpd_resp_sendstr(req, "NoSuchFileError");
        return ESP_FAIL;
    }

    char *buf = malloc(BUF_SIZE);
    for (;;)
    {
        int bs = fread(buf, 1, BUF_SIZE, fp);
        int ret = httpd_resp_send_chunk(req, buf, bs);
        // 0 return value indicates connection closed
        if (ret <= 0)
        {
            if (ret != ESP_OK)
            {
#ifdef WEBSERV_DEBUG
                logprintf("%s: send error\n", __func__);
#endif
                free(buf);
                fclose(fp);
                httpd_resp_sendstr(req, "Error");
                return ESP_FAIL;
            }
        }
        if (bs == 0)
        {
            break;
        }
        else if (bs < BUF_SIZE)
        {
            httpd_resp_send_chunk(req, buf, 0);
            break;
        }
    }
    fclose(fp);
    free(buf);

    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req)
{
    size_t url_len = httpd_req_get_url_query_len(req);
    char *url = malloc(url_len + 1);
    httpd_req_get_url_query_str(req, url, url_len + 1);
#ifdef WEBSERV_DEBUG
    logprintf("%s GET '%s'\n", __func__, url);
#endif
    if (url[0] == PROTECTED_FILE)
    {
        free(url);
        httpd_resp_sendstr(req, "Access Denied");
        return ESP_FAIL;
    }

    esp_err_t res = get_file_by_name(req, url);
    free(url);
    return res;
}

static esp_err_t control_handler(httpd_req_t *req)
{
    size_t url_len = httpd_req_get_url_query_len(req);
    char *url = malloc(url_len + 1);
    httpd_req_get_url_query_str(req, url, url_len + 1);
#ifdef WEBSERV_DEBUG
    logprintf("%s GET '%s'\n", __func__, url);
#endif
    int n = 0;
    char *data = NULL;
    size_t data_len = 0;
    int res = 0;

    int idx = 0;
    for (;;)
    {
        if (control_names[idx].id == 0)
        {
            break;
        }
        if (strncmp(control_names[idx].name, url, strlen(control_names[idx].name)) == 0)
        {
            n = control_names[idx].id;
            break;
        }
        idx++;
    }
    if (n == CMD_SET_LOAD)
    {
        if (strlen(url) > strlen(control_names[idx].name) + 1)
        {
            data = url + strlen(control_names[idx].name) + 1;
#ifdef WEBSERV_DEBUG
            logprintf("%s: setload data = %s\n", __func__, data);
#endif
        }
    }
    else if (n == CMD_DELETE_FILE)
    {
        if (strlen(url) > strlen(control_names[idx].name) + 1)
        {
            data = url + strlen(control_names[idx].name) + 1;
#ifdef WEBSERV_DEBUG
            logprintf("%s: delete file data = %s\n", __func__, data);
#endif
            // delete file
            char *fname = malloc(strlen(data) + strlen(FS_BASE) + 1);
            sprintf(fname, "%s%s", FS_BASE, data);
            res = unlink(fname);
            free(fname);

            // set to none to skip callback
            n = CMD_NONE;
        }
    }
    if (n != CMD_NONE && control_func != NULL)
    {
        control_func(n, data, data_len);
    }
    free(url);
    if (res == 0)
        httpd_resp_sendstr(req, "OK");
    else
        httpd_resp_sendstr(req, "Error");
    return ESP_OK;
}

static esp_err_t get_index_handler(httpd_req_t *req)
{
    return get_file_by_name(req, "index.html");
}

static const httpd_uri_t post_file = {
    .uri = FILE_URI,
    .method = HTTP_POST,
    .handler = post_handler,
};

static const httpd_uri_t get_file = {
    .uri = FILE_URI,
    .method = HTTP_GET,
    .handler = get_handler,
};

static const httpd_uri_t index_file = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_index_handler,
};

static const httpd_uri_t control_file = {
    .uri = CONTROL_URI,
    .method = HTTP_GET,
    .handler = control_handler,
};

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
#ifdef WEBSERV_DEBUG
    logprintf("%s: not found: %s\n", __func__, req->uri);
#endif
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    return ESP_FAIL;
}

int webserver_start(int read_only)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

#ifdef WEBSERV_DEBUG
    logprintf("%s: Starting server on port: %d\n", __func__, config.server_port);
#endif
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        if (read_only == 0)
        {
            httpd_register_uri_handler(server, &post_file);
            httpd_register_uri_handler(server, &control_file);
        }
        httpd_register_uri_handler(server, &get_file);
        httpd_register_uri_handler(server, &index_file);
        running = 1;
        return 1;
    }
#ifdef WEBSERV_DEBUG
    logprintf("%s: Error starting server!\n", __func__);
#endif
    return 0;
}

void webserver_stop()
{
    if (running)
    {
        running = 0;
        httpd_stop(server);
    }
}
