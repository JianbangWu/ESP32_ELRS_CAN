#include "wifi_component.hpp"
#include "esp_log.h"
#include "argtable3/argtable3.h"

decltype(WiFiComponent::_wifi_state) WiFiComponent::_wifi_state{""};

WiFiComponent::WiFiComponent(SemaphoreHandle_t &prompt_change, EventGroupHandle_t &wifi_event) : initialized(false), _prompt_change_sem(prompt_change), _wifi_event(wifi_event), _wifikey_logger("/sdcard/wifi")
{
    initializeWiFi();
    if (false == _wifikey_logger.init("wifi_key.txt"))
    {
        ESP_LOGE(TAG, "Failed to Init WifiLogger");
    }
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
        xEventGroupClearBits(instance->_wifi_event, CONNECTED_BIT);
        _wifi_state = {""};
        xSemaphoreGive(instance->_prompt_change_sem);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(instance->_wifi_event, CONNECTED_BIT);
        _wifi_state = {"[WIFI]"};
        xSemaphoreGive(instance->_prompt_change_sem);
    }
}

bool WiFiComponent::connect(const std::string &ssid, const std::string &password, int timeout_ms)
{
    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    if (!password.empty())
    {
        strlcpy((char *)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();

    int bits = xEventGroupWaitBits(_wifi_event, CONNECTED_BIT, pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);

    if (ret == ESP_OK)
    {
        if (_wifikey_logger.get_init_state() == false)
        {
            _wifikey_logger.init("wifi_key.txt");
        }

        const std::vector<std::string> string_group{ssid, password};
        _wifikey_logger.log_string_group(string_group, ":");
        _wifikey_logger.shutdown();
    }

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

    // Register 'disconnect' command
    const esp_console_cmd_t wifi_scan = {
        .command = "wifi_scan",
        .help = "Scan current WiFi ap",
        .hint = NULL,
        .func = NULL, // Not used
        .argtable = NULL,
        .func_w_context = &WiFiComponent::scanCommand, // Use context-aware function
        .context = this                                // Pass the instance pointer as context
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_scan));
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
        ESP_LOGI(instance->TAG, "Connected to WiFi");
        return 0;
    }
    else
    {
        ESP_LOGE(instance->TAG, "Failed to connect to WiFi");
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
        ESP_LOGI(instance->TAG, "Disconnected from WiFi");
        return 0;
    }
    else
    {
        ESP_LOGE(instance->TAG, "Failed to disconnect from WiFi");
        return 1;
    }
}

// 修改：调整函数签名以匹配 esp_console_cmd_func_with_context_t
int WiFiComponent::scanCommand(void *context, int argc, char **argv)
{
    // Get the instance from context
    WiFiComponent *instance = static_cast<WiFiComponent *>(context);
    instance->wifi_scan();
    return 0;
}

/* Initialize Wi-Fi as sta and set scan method */
void WiFiComponent::wifi_scan()
{
    //     ESP_ERROR_CHECK(esp_netif_init());
    //     ESP_ERROR_CHECK(esp_event_loop_create_default());
    //     esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    //     assert(sta_netif);
    //
    //     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    //     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = 16;
    wifi_ap_record_t ap_info[16];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    //
    //     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //     ESP_ERROR_CHECK(esp_wifi_start());

#ifdef USE_CHANNEL_BITMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1, sizeof(wifi_scan_config_t));
    if (!scan_config)
    {
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);

#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/

    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++)
    {
        std::string ssid(reinterpret_cast<const char *>(ap_info[i].ssid));
        // ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        // ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        std::string key = _wifikey_logger.find_string_group_key(ssid);
        if ("" != key)
        {
            connect(ssid, key, 10000);
            break;
        }
    }
}