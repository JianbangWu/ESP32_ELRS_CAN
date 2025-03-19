#pragma once

#include <string>
#include <string.h>
#include "logger.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "esp_console.h"

   class WiFiComponent
   {
   public:
      WiFiComponent(EventGroupHandle_t &wifi_event, LoggerBase &logger);
      ~WiFiComponent();

      struct
      {
         struct arg_str *ssid;
         struct arg_str *password;
         struct arg_int *timeout;
         struct arg_end *end;
      } join_args;

      bool connect(const std::string &ssid, const std::string &password, int timeout_ms = 10000);
      bool disconnect();
      bool isConnected();

      void registerConsoleCommands();

   private:
      EventGroupHandle_t &_wifi_event;
      LoggerBase &_wifikey_logger;

      static void eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
      void initializeWiFi();

      // 修改：调整函数签名以匹配 esp_console_cmd_func_with_context_t
      static int connectCommand(void *context, int argc, char **argv);
      static int disconnectCommand(void *context, int argc, char **argv);

      // 修改：将 TAG 声明为类的静态成员变量
      static const char *TAG;

      const int CONNECTED_BIT = BIT0;
      bool initialized;
   };
#ifdef __cplusplus
}
#endif
