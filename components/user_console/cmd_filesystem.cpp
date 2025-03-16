#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "user_console.hpp"
#include "argtable3/argtable3.h"

#define MOUNT_POINT "/sdcard"

extern USER_CONSOLE *console_obj;

static std::string current_path = MOUNT_POINT; // 维护当前工作目录

// 拼接路径
static std::string join_path(std::string_view base, std::string_view sub)
{
    return std::string(base) + "/" + std::string(sub);
}

// 定义 ls 命令的参数结构
static struct
{
    struct arg_str *dirname;
    struct arg_end *end;
} ls_args;

// `ls` 命令
static int cmd_ls(int argc, char **argv)
{
    // 解析命令行参数
    int nerrors = arg_parse(argc, argv, (void **)&ls_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, ls_args.end, argv[0]);
        return 1;
    }

    // 获取路径参数
    std::string input_path{ls_args.dirname->sval[0]};
    printf("input_path: %s\n", input_path.c_str());

    std::string path = (input_path.compare(".") != 0) ? join_path(current_path, input_path) : current_path;

    printf("path: %s\n", path.c_str());

    DIR *dir = opendir(path.c_str());
    if (!dir)
    {
        printf("Cannot open directory: %s\n", path.c_str());
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        printf("%s\n", entry->d_name);
    }
    closedir(dir);
    return 0;
}

// 定义 cd 命令的参数结构
static struct
{
    struct arg_str *dirname;
    struct arg_end *end;
} cd_args;

// `cd` 命令
static int cmd_cd(int argc, char **argv)
{
    // 解析命令行参数
    int nerrors = arg_parse(argc, argv, (void **)&cd_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, cd_args.end, argv[0]);
        return 1;
    }

    // 获取路径参数
    std::string input_path{cd_args.dirname->sval[0]};
    printf("input_path: %s\n", input_path.c_str());

    // 处理 cd 命令
    std::string new_path;
    if (input_path.compare(".") == 0)
    {
        size_t last_slash = current_path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash != 0)
        {
            current_path = current_path.substr(0, last_slash);
        }
        else
        {
            current_path = MOUNT_POINT;
        }
    }
    else if (input_path.compare("HOME") == 0)
    {
        current_path = MOUNT_POINT;
    }
    else
    {
        new_path = join_path(current_path, input_path);
        struct stat st;
        if (stat(new_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            current_path = new_path;
        }
        else
        {
            printf("Failed to change directory: %s\n", input_path.c_str());
            return 1;
        }
    }

    // 更新当前工作目录和 prompt
    console_obj->set_path(current_path);

    return 0;
}

static struct
{
    struct arg_str *dirname;
    struct arg_end *end;
} mkdir_args;

// `mkdir` 命令
static int cmd_mkdir(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mkdir_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, mkdir_args.end, argv[0]);
        return 1;
    }

    const char *dirname = mkdir_args.dirname->sval[0];
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", dirname);

    // 创建目录
    if (mkdir(full_path, 0755) != 0)
    {
        ESP_LOGE("mkdir", "Failed to create directory: %s", full_path);
        return 1;
    }

    ESP_LOGI("mkdir", "Directory created: %s", full_path);
    return 0;
}

// 注册命令
void register_commands(USER_CONSOLE *console)
{

    cd_args.dirname = arg_str1(NULL, NULL, "<dirname>", "Directory to change to");
    cd_args.end = arg_end(2);

    esp_console_cmd_t cd_cmd = {
        .command = "cd",
        .help = "Change the current directory",
        .hint = "<path>",
        .func = &cmd_cd,
        .argtable = &cd_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cd_cmd));

    mkdir_args.dirname = arg_str1(NULL, NULL, "<dirname>", "Directory name to create");
    mkdir_args.end = arg_end(2);

    esp_console_cmd_t mkdir_cmd = {
        .command = "mkdir",
        .help = "Create a directory",
        .hint = "<directory>",
        .func = &cmd_mkdir,
        .argtable = &mkdir_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&mkdir_cmd));

    ls_args.dirname = arg_str1(NULL, NULL, "<path>", "Directory path");
    ls_args.end = arg_end(1); // 最多允许 1 个错误

    esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "List files in the current directory",
        .hint = "[path]",
        .func = &cmd_ls,
        .argtable = &ls_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&ls_cmd));
}