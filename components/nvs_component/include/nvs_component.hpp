/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_console.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"

   class NVS_DEV
   {
   public:
      static void registerNVS();
      static char current_namespace[16];
      NVS_DEV();
      ~NVS_DEV();

   private:
      static int setValue(int argc, char **argv);
      static int getValue(int argc, char **argv);
      static int eraseValue(int argc, char **argv);
      static int eraseNamespace(int argc, char **argv);
      static int setNamespace(int argc, char **argv);
      static int listEntries(int argc, char **argv);

      static esp_err_t setValueInNVS(const char *key, const char *str_type, const char *str_value);
      static esp_err_t getValueFromNVS(const char *key, const char *str_type);
      static esp_err_t eraseKey(const char *key);
      static esp_err_t eraseAll(const char *name);
      static int list(const char *part, const char *name, const char *str_type);

      static nvs_type_t strToType(const char *type);
      static const char *typeToStr(nvs_type_t type);
      static esp_err_t storeBlob(nvs_handle_t nvs, const char *key, const char *str_values);
      static void printBlob(const char *blob, size_t len);

      static constexpr size_t TYPE_STR_PAIR_SIZE = 11;
      static constexpr const char *ARG_TYPE_STR = "type can be: i8, u8, i16, u16 i32, u32 i64, u64, str, blob";

      static constexpr const char *TAG = "NVS_DEV";

      struct TypeStrPair
      {
         nvs_type_t type;
         const char *str;
      };

      static const TypeStrPair typeStrPair[TYPE_STR_PAIR_SIZE];
   };

#ifdef __cplusplus
}
#endif
