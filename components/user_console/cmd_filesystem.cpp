#include <stdio.h>
#include <cstring>
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

// `pwd` 命令
static int cmd_pwd(int argc, char **argv)
{
    printf("%s\n", current_path.c_str());
    return 0;
}

// 定义 rm 命令的参数结构
static struct
{
    struct arg_lit *recursive; // -r 参数
    struct arg_lit *force;     // -f 参数
    struct arg_str *path;      // 文件或目录路径
    struct arg_end *end;       // 结束标记
} rm_args;

// 递归删除目录
static int remove_directory(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        std::string full_path = join_path(path, entry->d_name);
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                if (remove_directory(full_path.c_str()) != 0)
                {
                    closedir(dir);
                    return -1;
                }
            }
            else
            {
                if (unlink(full_path.c_str()) != 0)
                {
                    closedir(dir);
                    return -1;
                }
            }
        }
    }

    closedir(dir);
    return rmdir(path);
}

// `rm` 命令
static int cmd_rm(int argc, char **argv)
{
    // 解析命令行参数
    int nerrors = arg_parse(argc, argv, (void **)&rm_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, rm_args.end, argv[0]);
        return 1;
    }

    const char *path = rm_args.path->sval[0];
    std::string full_path = join_path(current_path, path);

    struct stat st;
    if (stat(full_path.c_str(), &st) != 0)
    {
        if (!rm_args.force->count) // 如果文件不存在且没有 -f 参数，报错
        {
            printf("rm: cannot remove '%s': No such file or directory\n", path);
            return 1;
        }
        return 0; // 如果文件不存在但有 -f 参数，静默返回
    }

    if (S_ISDIR(st.st_mode))
    {
        if (!rm_args.recursive->count) // 如果是目录但没有 -r 参数，报错
        {
            printf("rm: cannot remove '%s': Is a directory\n", path);
            return 1;
        }

        // 递归删除目录
        if (remove_directory(full_path.c_str()) != 0)
        {
            printf("rm: failed to remove '%s'\n", path);
            return 1;
        }
    }
    else
    {
        // 删除文件
        if (unlink(full_path.c_str()) != 0)
        {
            printf("rm: failed to remove '%s'\n", path);
            return 1;
        }
    }

    printf("Removed: %s\n", path);
    return 0;
}

// 定义 cat 命令的参数结构
static struct
{
    struct arg_str *filename;
    struct arg_end *end;
} cat_args;

// `cat` 命令
static int cmd_cat(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&cat_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, cat_args.end, argv[0]);
        return 1;
    }

    const char *filename = cat_args.filename->sval[0];
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", filename);

    FILE *file = fopen(full_path, "r");
    if (!file)
    {
        ESP_LOGE("cat", "Failed to open file: %s", full_path);
        return 1;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), file))
    {
        printf("%s", buffer);
    }

    fclose(file);
    return 0;
}

// 定义 tree 命令的参数结构
static struct
{
    struct arg_str *dirname; // 目录路径
    struct arg_end *end;     // 结束标记
} tree_args;

// 递归打印目录树
static void print_tree(const std::string &path, const std::string &prefix)
{
    DIR *dir = opendir(path.c_str());
    if (!dir)
    {
        printf("Cannot open directory: %s\n", path.c_str());
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // 忽略 . 和 ..
        }

        std::string full_path = join_path(path, entry->d_name);
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0)
        {
            continue; // 忽略无法访问的文件或目录
        }

        printf("%s%s\n", prefix.c_str(), entry->d_name);

        if (S_ISDIR(st.st_mode))
        {
            // 如果是目录，递归打印
            print_tree(full_path, prefix + "    ");
        }
    }

    closedir(dir);
}

// `tree` 命令
static int cmd_tree(int argc, char **argv)
{
    // 解析命令行参数
    int nerrors = arg_parse(argc, argv, (void **)&tree_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, tree_args.end, argv[0]);
        return 1;
    }

    // 获取路径参数
    std::string input_path{tree_args.dirname->sval[0]};
    std::string path = (input_path.compare(".") != 0) ? join_path(current_path, input_path) : current_path;

    // printf("%s\n", path.c_str()); // 打印根目录
    print_tree(path, ""); // 递归打印目录树

    return 0;
}

// 注册命令
void register_commands(void)
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
        .argtable = &mkdir_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&mkdir_cmd));

    ls_args.dirname = arg_str1(NULL, NULL, "<path>", "Directory path");
    ls_args.end = arg_end(1); // 最多允许 1 个错误

    esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "List files in the current directory",
        .hint = "[path]",
        .func = &cmd_ls,
        .argtable = &ls_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ls_cmd));

    // 注册 pwd 命令
    esp_console_cmd_t pwd_cmd = {
        .command = "pwd",
        .help = "Print the current working directory",
        .hint = NULL,
        .func = &cmd_pwd,
        .argtable = NULL,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pwd_cmd));

    // 注册 rm 命令
    rm_args.recursive = arg_lit0("r", "recursive", "Remove directories and their contents recursively");
    rm_args.force = arg_lit0("f", "force", "Ignore nonexistent files and arguments, never prompt");
    rm_args.path = arg_str1(NULL, NULL, "<path>", "File or directory to remove");
    rm_args.end = arg_end(2);

    esp_console_cmd_t rm_cmd = {
        .command = "rm",
        .help = "Remove files or directories",
        .hint = "[-r] [-f] <path>",
        .func = &cmd_rm,
        .argtable = &rm_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rm_cmd));

    // 注册 cat 命令
    cat_args.filename = arg_str1(NULL, NULL, "<filename>", "File to display");
    cat_args.end = arg_end(2);

    esp_console_cmd_t cat_cmd = {
        .command = "cat",
        .help = "Display the contents of a file",
        .hint = "<file>",
        .func = &cmd_cat,
        .argtable = &cat_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cat_cmd));

    // 注册 tree 命令
    tree_args.dirname = arg_str0(NULL, NULL, "<path>", "Directory to list (default: current directory)");
    tree_args.end = arg_end(1);

    esp_console_cmd_t tree_cmd = {
        .command = "tree",
        .help = "List directory contents in a tree-like format",
        .hint = "[path]",
        .func = &cmd_tree,
        .argtable = &tree_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&tree_cmd));
}