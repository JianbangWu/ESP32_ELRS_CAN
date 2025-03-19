#include "nvs_component.hpp"
#include <cstring>
#include <cstdlib>
#include <cinttypes>
#include <cerrno>

const CmdNVS::TypeStrPair CmdNVS::typeStrPair[] = {
    {NVS_TYPE_I8, "i8"},
    {NVS_TYPE_U8, "u8"},
    {NVS_TYPE_U16, "u16"},
    {NVS_TYPE_I16, "i16"},
    {NVS_TYPE_U32, "u32"},
    {NVS_TYPE_I32, "i32"},
    {NVS_TYPE_U64, "u64"},
    {NVS_TYPE_I64, "i64"},
    {NVS_TYPE_STR, "str"},
    {NVS_TYPE_BLOB, "blob"},
    {NVS_TYPE_ANY, "any"}};

char CmdNVS::current_namespace[16] = "storage";

nvs_type_t CmdNVS::strToType(const char *type)
{
    for (size_t i = 0; i < TYPE_STR_PAIR_SIZE; i++)
    {
        if (strcmp(type, typeStrPair[i].str) == 0)
        {
            return typeStrPair[i].type;
        }
    }
    return NVS_TYPE_ANY;
}

const char *CmdNVS::typeToStr(nvs_type_t type)
{
    for (size_t i = 0; i < TYPE_STR_PAIR_SIZE; i++)
    {
        if (typeStrPair[i].type == type)
        {
            return typeStrPair[i].str;
        }
    }
    return "Unknown";
}

esp_err_t CmdNVS::storeBlob(nvs_handle_t nvs, const char *key, const char *str_values)
{
    size_t str_len = strlen(str_values);
    size_t blob_len = str_len / 2;

    if (str_len % 2)
    {
        ESP_LOGE(TAG, "Blob data must contain even number of characters");
        return ESP_ERR_NVS_TYPE_MISMATCH;
    }

    char *blob = (char *)malloc(blob_len);
    if (blob == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0, j = 0; i < str_len; i++)
    {
        char ch = str_values[i];
        uint8_t value = 0;
        if (ch >= '0' && ch <= '9')
        {
            value = ch - '0';
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            value = ch - 'A' + 10;
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            value = ch - 'a' + 10;
        }
        else
        {
            ESP_LOGE(TAG, "Blob data contains invalid character");
            free(blob);
            return ESP_ERR_NVS_TYPE_MISMATCH;
        }

        if (i & 1)
        {
            blob[j++] += value;
        }
        else
        {
            blob[j] = value << 4;
        }
    }

    esp_err_t err = nvs_set_blob(nvs, key, blob, blob_len);
    free(blob);

    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }

    return err;
}

void CmdNVS::printBlob(const char *blob, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        printf("%02x", blob[i]);
    }
    printf("\n");
}

esp_err_t CmdNVS::setValueInNVS(const char *key, const char *str_type, const char *str_value)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(current_namespace, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    nvs_type_t type = strToType(str_type);
    if (type == NVS_TYPE_ANY)
    {
        ESP_LOGE(TAG, "Type '%s' is undefined", str_type);
        nvs_close(nvs);
        return ESP_ERR_NVS_TYPE_MISMATCH;
    }

    bool range_error = false;
    if (type == NVS_TYPE_I8)
    {
        int32_t value = strtol(str_value, nullptr, 0);
        if (value < INT8_MIN || value > INT8_MAX || errno == ERANGE)
        {
            range_error = true;
        }
        else
        {
            err = nvs_set_i8(nvs, key, static_cast<int8_t>(value));
        }
    }
    else if (type == NVS_TYPE_U8)
    {
        uint32_t value = strtoul(str_value, nullptr, 0);
        if (value > UINT8_MAX || errno == ERANGE)
        {
            range_error = true;
        }
        else
        {
            err = nvs_set_u8(nvs, key, static_cast<uint8_t>(value));
        }
    }
    else if (type == NVS_TYPE_I16)
    {
        int32_t value = strtol(str_value, nullptr, 0);
        if (value < INT16_MIN || value > INT16_MAX || errno == ERANGE)
        {
            range_error = true;
        }
        else
        {
            err = nvs_set_i16(nvs, key, static_cast<int16_t>(value));
        }
    }
    else if (type == NVS_TYPE_U16)
    {
        uint32_t value = strtoul(str_value, nullptr, 0);
        if (value > UINT16_MAX || errno == ERANGE)
        {
            range_error = true;
        }
        else
        {
            err = nvs_set_u16(nvs, key, static_cast<uint16_t>(value));
        }
    }
    else if (type == NVS_TYPE_I32)
    {
        int32_t value = strtol(str_value, nullptr, 0);
        if (errno != ERANGE)
        {
            err = nvs_set_i32(nvs, key, value);
        }
    }
    else if (type == NVS_TYPE_U32)
    {
        uint32_t value = strtoul(str_value, nullptr, 0);
        if (errno != ERANGE)
        {
            err = nvs_set_u32(nvs, key, value);
        }
    }
    else if (type == NVS_TYPE_I64)
    {
        int64_t value = strtoll(str_value, nullptr, 0);
        if (errno != ERANGE)
        {
            err = nvs_set_i64(nvs, key, value);
        }
    }
    else if (type == NVS_TYPE_U64)
    {
        uint64_t value = strtoull(str_value, nullptr, 0);
        if (errno != ERANGE)
        {
            err = nvs_set_u64(nvs, key, value);
        }
    }
    else if (type == NVS_TYPE_STR)
    {
        err = nvs_set_str(nvs, key, str_value);
    }
    else if (type == NVS_TYPE_BLOB)
    {
        err = storeBlob(nvs, key, str_value);
    }

    if (range_error || errno == ERANGE)
    {
        nvs_close(nvs);
        return ESP_ERR_NVS_VALUE_TOO_LONG;
    }

    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Value stored under key '%s'", key);
        }
    }

    nvs_close(nvs);
    return err;
}

esp_err_t CmdNVS::getValueFromNVS(const char *key, const char *str_type)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(current_namespace, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    nvs_type_t type = strToType(str_type);
    if (type == NVS_TYPE_ANY)
    {
        ESP_LOGE(TAG, "Type '%s' is undefined", str_type);
        nvs_close(nvs);
        return ESP_ERR_NVS_TYPE_MISMATCH;
    }

    if (type == NVS_TYPE_I8)
    {
        int8_t value;
        err = nvs_get_i8(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%d\n", value);
        }
    }
    else if (type == NVS_TYPE_U8)
    {
        uint8_t value;
        err = nvs_get_u8(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%u\n", value);
        }
    }
    else if (type == NVS_TYPE_I16)
    {
        int16_t value;
        err = nvs_get_i16(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%d\n", value);
        }
    }
    else if (type == NVS_TYPE_U16)
    {
        uint16_t value;
        err = nvs_get_u16(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%u\n", value);
        }
    }
    else if (type == NVS_TYPE_I32)
    {
        int32_t value;
        err = nvs_get_i32(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%" PRIi32 "\n", value);
        }
    }
    else if (type == NVS_TYPE_U32)
    {
        uint32_t value;
        err = nvs_get_u32(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%" PRIu32 "\n", value);
        }
    }
    else if (type == NVS_TYPE_I64)
    {
        int64_t value;
        err = nvs_get_i64(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%lld\n", value);
        }
    }
    else if (type == NVS_TYPE_U64)
    {
        uint64_t value;
        err = nvs_get_u64(nvs, key, &value);
        if (err == ESP_OK)
        {
            printf("%llu\n", value);
        }
    }
    else if (type == NVS_TYPE_STR)
    {
        size_t len;
        err = nvs_get_str(nvs, key, nullptr, &len);
        if (err == ESP_OK)
        {
            char *str = (char *)malloc(len);
            if (str != nullptr)
            {
                err = nvs_get_str(nvs, key, str, &len);
                if (err == ESP_OK)
                {
                    printf("%s\n", str);
                }
                free(str);
            }
            else
            {
                err = ESP_ERR_NO_MEM;
            }
        }
    }
    else if (type == NVS_TYPE_BLOB)
    {
        size_t len;
        err = nvs_get_blob(nvs, key, nullptr, &len);
        if (err == ESP_OK)
        {
            char *blob = (char *)malloc(len);
            if (blob != nullptr)
            {
                err = nvs_get_blob(nvs, key, blob, &len);
                if (err == ESP_OK)
                {
                    printBlob(blob, len);
                }
                free(blob);
            }
            else
            {
                err = ESP_ERR_NO_MEM;
            }
        }
    }

    nvs_close(nvs);
    return err;
}

esp_err_t CmdNVS::eraseKey(const char *key)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(current_namespace, NVS_READWRITE, &nvs);
    if (err == ESP_OK)
    {
        err = nvs_erase_key(nvs, key);
        if (err == ESP_OK)
        {
            err = nvs_commit(nvs);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "Value with key '%s' erased", key);
            }
        }
        nvs_close(nvs);
    }
    return err;
}

esp_err_t CmdNVS::eraseAll(const char *name)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(name, NVS_READWRITE, &nvs);
    if (err == ESP_OK)
    {
        err = nvs_erase_all(nvs);
        if (err == ESP_OK)
        {
            err = nvs_commit(nvs);
        }
    }
    ESP_LOGI(TAG, "_namespace '%s' was %s erased", name, (err == ESP_OK) ? "" : "not");
    nvs_close(nvs);
    return err;
}

int CmdNVS::list(const char *part, const char *name, const char *str_type)
{
    nvs_type_t type = strToType(str_type);
    nvs_iterator_t it = nullptr;
    esp_err_t result = nvs_entry_find(part, nullptr, type, &it);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "No such entry was found");
        return 1;
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS error: %s", esp_err_to_name(result));
        return 1;
    }

    do
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        result = nvs_entry_next(&it);
        printf("_namespace '%s', key '%s', type '%s'\n",
               info.namespace_name, info.key, typeToStr(info.type));
    } while (result == ESP_OK);

    if (result != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "NVS error %s at current iteration, stopping.", esp_err_to_name(result));
        return 1;
    }

    return 0;
}

int CmdNVS::setValue(int argc, char **argv)
{
    static struct
    {
        struct arg_str *key;
        struct arg_str *type;
        struct arg_str *value;
        struct arg_end *_end;
    } set_args;

    set_args.key = arg_str1(nullptr, nullptr, "<key>", "key of the value to be set");
    set_args.type = arg_str1(nullptr, nullptr, "<type>", ARG_TYPE_STR);
    set_args.value = arg_str1("v", "value", "<value>", "value to be stored");
    set_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&set_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_args._end, argv[0]);
        return 1;
    }

    const char *key = set_args.key->sval[0];
    const char *type = set_args.type->sval[0];
    const char *value = set_args.value->sval[0];

    esp_err_t err = setValueInNVS(key, type, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return 1;
    }

    return 0;
}

int CmdNVS::getValue(int argc, char **argv)
{
    static struct
    {
        struct arg_str *key;
        struct arg_str *type;
        struct arg_end *_end;
    } get_args;

    get_args.key = arg_str1(nullptr, nullptr, "<key>", "key of the value to be read");
    get_args.type = arg_str1(nullptr, nullptr, "<type>", ARG_TYPE_STR);
    get_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&get_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, get_args._end, argv[0]);
        return 1;
    }

    const char *key = get_args.key->sval[0];
    const char *type = get_args.type->sval[0];

    esp_err_t err = getValueFromNVS(key, type);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return 1;
    }

    return 0;
}

int CmdNVS::eraseValue(int argc, char **argv)
{
    static struct
    {
        struct arg_str *key;
        struct arg_end *_end;
    } erase_args;

    erase_args.key = arg_str1(nullptr, nullptr, "<key>", "key of the value to be erased");
    erase_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&erase_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, erase_args._end, argv[0]);
        return 1;
    }

    const char *key = erase_args.key->sval[0];

    esp_err_t err = eraseKey(key);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return 1;
    }

    return 0;
}

int CmdNVS::eraseNamespace(int argc, char **argv)
{
    static struct
    {
        struct arg_str *_namespace;
        struct arg_end *_end;
    } erase_all_args;

    erase_all_args._namespace = arg_str1(nullptr, nullptr, "<_namespace>", "_namespace to be erased");
    erase_all_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&erase_all_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, erase_all_args._end, argv[0]);
        return 1;
    }

    const char *name = erase_all_args._namespace->sval[0];

    esp_err_t err = eraseAll(name);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "%s", esp_err_to_name(err));
        return 1;
    }

    return 0;
}

int CmdNVS::setNamespace(int argc, char **argv)
{
    static struct
    {
        struct arg_str *_namespace;
        struct arg_end *_end;
    } namespace_args;

    namespace_args._namespace = arg_str1(nullptr, nullptr, "<_namespace>", "_namespace of the partition to be selected");
    namespace_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&namespace_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, namespace_args._end, argv[0]);
        return 1;
    }

    const char *_namespace = namespace_args._namespace->sval[0];
    strlcpy(current_namespace, _namespace, sizeof(current_namespace));
    ESP_LOGI(TAG, "_namespace set to '%s'", current_namespace);
    return 0;
}

int CmdNVS::listEntries(int argc, char **argv)
{
    static struct
    {
        struct arg_str *partition;
        struct arg_str *_namespace;
        struct arg_str *type;
        struct arg_end *_end;
    } list_args;

    list_args.partition = arg_str1(nullptr, nullptr, "<partition>", "partition name");
    list_args._namespace = arg_str0("n", "_namespace", "<_namespace>", "_namespace name");
    list_args.type = arg_str0("t", "type", "<type>", ARG_TYPE_STR);
    list_args._end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&list_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, list_args._end, argv[0]);
        return 1;
    }

    const char *part = list_args.partition->sval[0];
    const char *name = list_args._namespace->sval[0];
    const char *type = list_args.type->sval[0];

    return list(part, name, type);
}

void CmdNVS::registerNVS()
{
    const esp_console_cmd_t set_cmd = {
        .command = "nvs_set",
        .help = "Set key-value pair in selected _namespace.\n"
                "Examples:\n"
                " nvs_set VarName i32 -v 123 \n"
                " nvs_set VarName str -v YourString \n"
                " nvs_set VarName blob -v 0123456789abcdef \n",
        .hint = nullptr,
        .func = &setValue,
        .argtable = nullptr};

    const esp_console_cmd_t get_cmd = {
        .command = "nvs_get",
        .help = "Get key-value pair from selected _namespace. \n"
                "Example: nvs_get VarName i32",
        .hint = nullptr,
        .func = &getValue,
        .argtable = nullptr};

    const esp_console_cmd_t erase_cmd = {
        .command = "nvs_erase",
        .help = "Erase key-value pair from current _namespace",
        .hint = nullptr,
        .func = &eraseValue,
        .argtable = nullptr};

    const esp_console_cmd_t erase_namespace_cmd = {
        .command = "nvs_erase_namespace",
        .help = "Erases specified _namespace",
        .hint = nullptr,
        .func = &eraseNamespace,
        .argtable = nullptr};

    const esp_console_cmd_t namespace_cmd = {
        .command = "nvs_namespace",
        .help = "Set current _namespace",
        .hint = nullptr,
        .func = &setNamespace,
        .argtable = nullptr};

    const esp_console_cmd_t list_entries_cmd = {
        .command = "nvs_list",
        .help = "List stored key-value pairs stored in NVS."
                "_namespace and type can be specified to print only those key-value pairs.\n"
                "Following command list variables stored inside 'nvs' partition, under _namespace 'storage' with type uint32_t"
                "Example: nvs_list nvs -n storage -t u32 \n",
        .hint = nullptr,
        .func = &listEntries,
        .argtable = nullptr};

    ESP_ERROR_CHECK(esp_console_cmd_register(&set_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&get_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&erase_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&namespace_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_entries_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&erase_namespace_cmd));
}