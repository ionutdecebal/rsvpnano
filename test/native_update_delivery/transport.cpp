#include "network/VerifiedDownload.h"
#include "esp_http_client.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <cstdio>
uint32_t testMillis = 0;
struct Response {
    int status = 200;
    int64_t length = 6;
    std::string data = "abcdef", redirect;
    bool stall = false, tlsError = false;
};
std::map<std::string,Response> responses;
int opens = 0, alive = 0;
struct FakeClient {
    esp_http_client_config_t config;
    std::string url;
    Response response;
    size_t position = 0;
};
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* config) {
    assert(config->crt_bundle_attach); assert(config->disable_auto_redirect);
    assert(config->timeout_ms > 0 && config->buffer_size <= 4096);
    ++alive;
    return new FakeClient{*config, config->url, responses.at(config->url)};
}
int esp_http_client_cleanup(esp_http_client_handle_t c) { --alive; delete c; return 0; }
int esp_http_client_set_header(esp_http_client_handle_t, const char*, const char*) { return 0; }
int esp_http_client_open(esp_http_client_handle_t c,int) { ++opens; return c->response.tlsError ? -1 : 0; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c) {
    if (!c->response.redirect.empty()) {
        char key[]="Location"; esp_http_client_event_t event{
            HTTP_EVENT_ON_HEADER,key,c->response.redirect.data(),c->config.user_data};
        c->config.event_handler(&event);
    }
    return c->response.length;
}
int esp_http_client_get_status_code(esp_http_client_handle_t c) { return c->response.status; }
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) {
    return !c->response.stall && c->position == c->response.data.size();
}
int esp_http_client_read(esp_http_client_handle_t c,char* buffer,int size) {
    if(c->response.stall) { testMillis+=1000; return 0; }
    const size_t n=std::min({size_t(size),size_t(2),c->response.data.size()-c->position});
    memcpy(buffer,c->response.data.data()+c->position,n); c->position+=n; return n;
}
int main() {
    const std::string url="https://example.org/pack";
    std::string output;
    auto sink=[&](const uint8_t* data,size_t n){output.append(reinterpret_cast<const char*>(data),n);return true;};
    auto reset=[&]{responses={{url,{}}};output.clear();testMillis=0;opens=0;assert(alive==0);};
    reset(); assert(net::download(url,6,6,sink)); assert(output=="abcdef");
    reset(); responses[url].length=0; assert(net::download(url,6,6,sink));
    reset(); responses[url].length=7; assert(!net::download(url,6,6,sink)); assert(output.empty());
    reset(); responses[url].length=0; responses[url].data="abcdefgh"; assert(!net::download(url,6,6,sink)); assert(output.size()<=6);
    reset(); responses[url].data="ab"; assert(!net::download(url,6,6,sink));
    reset(); responses[url].length=0; responses[url].data=""; assert(!net::download(url,6,0,sink));
    reset(); responses[url].status=404; assert(!net::download(url,6,6,sink)); assert(output.empty());
    reset(); responses[url].tlsError=true; assert(!net::download(url,6,6,sink)); assert(output.empty());
    reset(); responses[url].stall=true; assert(!net::download(url,6,6,sink)); assert(testMillis<=21001);
    reset(); assert(!net::download(url,6,6,sink,{[]{return true;},{}})); assert(opens==0);
    reset(); assert(!net::download(url,6,6,sink,{[&]{return output.size()>=2;},{}})); assert(output.size()==2);
    reset(); assert(!net::download(url,6,6,[](const uint8_t*,size_t){return false;}));
    reset(); responses[url].status=302;responses[url].redirect="http://example.org/pack";
    assert(!net::download(url,6,6,sink));assert(opens==1);
    reset(); responses[url].status=302;responses[url].redirect=url;
    assert(!net::download(url,6,6,sink));assert(opens==6);
    reset(); responses[url].status=302;responses[url].redirect="https://assets.example.org/file";
    responses[responses[url].redirect]={};assert(net::download(url,6,6,sink));assert(output=="abcdef"&&opens==2);
    reset(); assert(!net::download("https://name@host/file",6,6,sink));assert(opens==0);
    reset(); assert(!net::download(url,5,6,sink));assert(opens==0);
    assert(alive==0);
    puts("HTTPS streaming: 17 success, redirect, downgrade, TLS, short/oversize, timeout, cancellation and sink-failure cases passed.");
}
