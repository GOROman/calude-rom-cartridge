// WiFi softAP + ROM アップロード/選択 Web UI

#include "web_server.h"
#include "cart_loader.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "web";

#define FS_BASE   "/littlefs"
#define AP_SSID   "FC-CART"
#define AP_PASS   "famicom1983"

static const char INDEX_HTML[] =
    "<!doctype html><meta charset=utf-8>"
    "<title>FC-CART</title>"
    "<h1>Famicom Cartridge</h1>"
    "<h2>Upload .nes</h2>"
    "<input type=file id=f accept=.nes>"
    "<button onclick=\"up()\">Upload</button>"
    "<h2>Games</h2><ul id=list></ul>"
    "<script>"
    "async function refresh(){const r=await fetch('/list');const j=await r.json();"
    "list.innerHTML=j.map(n=>`<li>${n} <button onclick=\"sel('${n}')\">Play</button></li>`).join('')}"
    "async function up(){const file=f.files[0];if(!file)return;"
    "await fetch('/upload',{method:'POST',headers:{'X-Filename':file.name},body:file});refresh()}"
    "async function sel(n){await fetch('/select',{method:'POST',body:n});"
    "alert('Loaded. Press console RESET!')}"
    "refresh();"
    "</script>";

static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t list_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char buf[512] = "[";
    DIR *dir = opendir(FS_BASE);
    if (dir) {
        struct dirent *e;
        bool first = true;
        while ((e = readdir(dir)) != NULL) {
            if (!strstr(e->d_name, ".nes")) continue;
            if (strlen(buf) + strlen(e->d_name) + 4 >= sizeof(buf)) break;
            if (!first) strlcat(buf, ",", sizeof(buf));
            strlcat(buf, "\"", sizeof(buf));
            strlcat(buf, e->d_name, sizeof(buf));
            strlcat(buf, "\"", sizeof(buf));
            first = false;
        }
        closedir(dir);
    }
    strlcat(buf, "]", sizeof(buf));
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static bool safe_name(const char *name)
{
    return name[0] && !strchr(name, '/') && !strchr(name, '\\') &&
           strcmp(name, ".") && strcmp(name, "..") && strstr(name, ".nes");
}

static esp_err_t upload_post(httpd_req_t *req)
{
    char name[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-Filename", name, sizeof(name)) != ESP_OK ||
        !safe_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad filename");
        return ESP_FAIL;
    }
    char path[96];
    snprintf(path, sizeof(path), FS_BASE "/%s", name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open failed");
        return ESP_FAIL;
    }
    char buf[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf, remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf));
        if (n <= 0) { fclose(f); return ESP_FAIL; }
        fwrite(buf, 1, n, f);
        remaining -= n;
    }
    fclose(f);
    ESP_LOGI(TAG, "uploaded %s (%d bytes)", name, req->content_len);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t select_post(httpd_req_t *req)
{
    char name[64] = {0};
    int n = httpd_req_recv(req, name, sizeof(name) - 1);
    if (n <= 0 || !safe_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad filename");
        return ESP_FAIL;
    }
    char path[96];
    snprintf(path, sizeof(path), FS_BASE "/%s", name);
    if (!cart_load_game(path)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "load failed");
        return ESP_FAIL;
    }
    // 次回起動時に自動ロードするゲームとして記憶
    FILE *f = fopen(FS_BASE "/autoload.txt", "w");
    if (f) { fputs(name, f); fclose(f); }
    return httpd_resp_sendstr(req, "ok");
}

void web_server_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t ap = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASS,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "softAP up: %s / http://192.168.4.1/", AP_SSID);

    httpd_handle_t server = NULL;
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &hcfg));
    const httpd_uri_t routes[] = {
        { .uri = "/",       .method = HTTP_GET,  .handler = index_get },
        { .uri = "/list",   .method = HTTP_GET,  .handler = list_get },
        { .uri = "/upload", .method = HTTP_POST, .handler = upload_post },
        { .uri = "/select", .method = HTTP_POST, .handler = select_post },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        httpd_register_uri_handler(server, &routes[i]);
}
