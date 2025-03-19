#include "wifi_component.hpp"
#include "esp_log.h"
#include "argtable3/argtable3.h"

const char *WiFiComponent::TAG = "WiFiComponent";

WiFiComponent::WiFiComponent(EventGroupHandle_t &wifi_event, LoggerBase &logger) : _wifi_event(wifi_event), _wifikey_logger(logger), initialized(false)
{
}

WiFiComponent::~WiFiComponent()
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
}

void WiFiComponent::initializeWiFi()
{
    if (initialized)
    {
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &WiFiComponent::eventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiComponent::eventHandler, this));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    initialized = true;
}

void WiFiComponent::eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WiFiComponent *instance = static_cast<WiFiComponent *>(arg);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        xEventGroupClearBits(instance->_wifi_event, instance->CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(instance->_wifi_event, instance->CONNECTED_BIT);
    }
}

bool WiFiComponent::connect(const std::string &ssid, const std::string &password, int timeout_ms)
{
    initializeWiFi();
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    if (!password.empty())
    {
        strlcpy((char *)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret == ESP_OK)
    {
        if (_wifikey_logger.init("wifi_key.txt"))
        {
            _wifikey_logger.log(ssid + ":" + password);
            _wifikey_logger.shutdown();
        }
    }

    int bits = xEventGroupWaitBits(_wifi_event, CONNECTED_BIT, pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);

    return (bits & CONNECTED_BIT) != 0;
}

bool WiFiComponent::disconnect()
{
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to disconnect from WiFi: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool WiFiComponent::isConnected()
{
    return (xEventGroupGetBits(_wifi_event) & CONNECTED_BIT);
}

void WiFiComponent::registerConsoleCommands()
{
    join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    join_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "join",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = NULL, // Not used
        .argtable = &join_args,
        .func_w_context = &WiFiComponent::connectCommand, // Use context-aware function
        .context = this                                   // Pass the instance pointer as context
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));

    // Register 'disconnect' command
    const esp_console_cmd_t disconnect_cmd = {
        .command = "disconnect",
        .help = "Disconnect from current WiFi network",
        .hint = NULL,
        .func = NULL, // Not used
        .argtable = NULL,
        .func_w_context = &WiFiComponent::disconnectCommand, // Use context-aware function
        .context = this                                      // Pass the instance pointer as context
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&disconnect_cmd));
}

// 修改：调整函数签名以匹配 esp_console_cmd_func_with_context_t
int WiFiComponent::connectCommand(void *context, int argc, char **argv)
{
    // Get the instance from context
    WiFiComponent *instance = static_cast<WiFiComponent *>(context);

    instance->join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    instance->join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    instance->join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    instance->join_args.end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&instance->join_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, instance->join_args.end, argv[0]);
        return 1;
    }

    const char *ssid = instance->join_args.ssid->sval[0];
    const char *password = instance->join_args.password->sval[0];
    int timeout_ms = instance->join_args.timeout->count > 0 ? instance->join_args.timeout->ival[0] : 10000;

    if (instance->connect(ssid, password, timeout_ms))
    {
        ESP_LOGI(TAG, "Connected to WiFi");
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return 1;
    }
}

// 修改：调整函数签名以匹配 esp_console_cmd_func_with_context_t
int WiFiComponent::disconnectCommand(void *context, int argc, char **argv)
{
    // Get the instance from context
    WiFiComponent *instance = static_cast<WiFiComponent *>(context);

    if (instance->disconnect())
    {
        ESP_LOGI(TAG, "Disconnected from WiFi");
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to disconnect from WiFi");
        return 1;
    }
}