#include "filesystem_cmd.hpp"
#include <stdio.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <unistd.h>

#include "esp_log.h"

#define MOUNT_POINT "/sdcard"

/* STATIC */
char *CmdFilesystem::TAG = {"FILESYS"};
SemaphoreHandle_t CmdFilesystem::_path_change = xSemaphoreCreateBinary();
std::string CmdFilesystem::current_path = MOUNT_POINT;

/* FUNCTION */
std::string CmdFilesystem::getFileType(const struct stat &st)
{
    if (S_ISDIR(st.st_mode))
    {
        return LOG_ANSI_COLOR(LOG_ANSI_COLOR_BLUE) "DIR" LOG_ANSI_COLOR_RESET;
    }
    else
    {
        return LOG_ANSI_COLOR(LOG_ANSI_COLOR_GREEN) "FILE" LOG_ANSI_COLOR_RESET;
    }
}

std::string CmdFilesystem::joinPath(std::string_view base, std::string_view sub)
{
    if (!sub.empty() && sub[0] == '/')
    {
        sub.remove_prefix(1); // 移除开头的 '/'
    }
    return std::string(base) + "/" + std::string(sub);
}

int CmdFilesystem::cmdLs(int argc, char **argv)
{
    static struct
    {
        struct arg_str *dirname;
        struct arg_end *end;
    } ls_args;

    ls_args.dirname = arg_str0(nullptr, nullptr, "<path>", "Directory path");
    ls_args.end = arg_end(1);

    int nerrors = arg_parse(argc, argv, (void **)&ls_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, ls_args.end, argv[0]);
        return 1;
    }

    std::string path = joinPath(current_path, ls_args.dirname->sval[0]);

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

int CmdFilesystem::cmdCd(int argc, char **argv)
{
    static struct
    {
        struct arg_str *dirname;
        struct arg_end *end;
    } cd_args;

    cd_args.dirname = arg_str1(nullptr, nullptr, "<dirname>", "Directory to change to");
    cd_args.end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&cd_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, cd_args.end, argv[0]);
        return 1;
    }

    std::string input_path{cd_args.dirname->sval[0]};
    std::string new_path;

    if (input_path.compare("..") == 0)
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
        new_path = joinPath(current_path, input_path);
        struct stat st;
        if (stat(new_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            current_path = new_path;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to change directory: %s\n", input_path.c_str());
            return 1;
        }
    }

    xSemaphoreGive(_path_change);
    return 0;
}

int CmdFilesystem::cmdMkdir(int argc, char **argv)
{
    static struct
    {
        struct arg_str *dirname;
        struct arg_end *end;
    } mkdir_args;

    mkdir_args.dirname = arg_str1(nullptr, nullptr, "<dirname>", "Directory name to create");
    mkdir_args.end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&mkdir_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, mkdir_args.end, argv[0]);
        return 1;
    }

    const char *dirname = mkdir_args.dirname->sval[0];
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s", dirname);

    if (mkdir(full_path, 0755) != 0)
    {
        ESP_LOGE("mkdir", "Failed to create directory: %s", full_path);
        return 1;
    }

    ESP_LOGI("mkdir", "Directory created: %s", full_path);
    return 0;
}

int CmdFilesystem::cmdPwd(int argc, char **argv)
{
    ESP_LOGI(TAG, "%s\n", current_path.c_str());
    return 0;
}

int CmdFilesystem::removeDirectory(const char *path)
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

        std::string full_path = joinPath(path, entry->d_name);
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                if (removeDirectory(full_path.c_str()) != 0)
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

int CmdFilesystem::cmdRm(int argc, char **argv)
{
    static struct
    {
        struct arg_lit *recursive;
        struct arg_lit *force;
        struct arg_str *path;
        struct arg_end *end;
    } rm_args;

    rm_args.recursive = arg_lit0("r", "recursive", "Remove directories and their contents recursively");
    rm_args.force = arg_lit0("f", "force", "Ignore nonexistent files and arguments, never prompt");
    rm_args.path = arg_str1(nullptr, nullptr, "<path>", "File or directory to remove");
    rm_args.end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&rm_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, rm_args.end, argv[0]);
        return 1;
    }

    const char *path = rm_args.path->sval[0];
    std::string full_path = joinPath(current_path, path);

    struct stat st;
    if (stat(full_path.c_str(), &st) != 0)
    {
        if (!rm_args.force->count)
        {
            ESP_LOGE(TAG, "rm: cannot remove '%s': No such file or directory\n", path);
            return 1;
        }
        return 0;
    }

    if (S_ISDIR(st.st_mode))
    {
        if (!rm_args.recursive->count)
        {
            ESP_LOGE(TAG, "rm: cannot remove '%s': Is a directory\n", path);
            return 1;
        }

        if (removeDirectory(full_path.c_str()) != 0)
        {
            ESP_LOGE(TAG, "rm: failed to remove '%s'\n", path);
            return 1;
        }
    }
    else
    {
        if (unlink(full_path.c_str()) != 0)
        {
            ESP_LOGE(TAG, "rm: failed to remove '%s'\n", path);
            return 1;
        }
    }

    ESP_LOGI(TAG, "Removed: %s\n", path);
    return 0;
}

int CmdFilesystem::cmdCat(int argc, char **argv)
{
    static struct
    {
        struct arg_str *filename;
        struct arg_end *end;
    } cat_args;

    cat_args.filename = arg_str1(nullptr, nullptr, "<filename>", "File to display");
    cat_args.end = arg_end(2);

    int nerrors = arg_parse(argc, argv, (void **)&cat_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, cat_args.end, argv[0]);
        return 1;
    }

    std::string path = joinPath(current_path, cat_args.filename->sval[0]);

    FILE *file = fopen(path.c_str(), "r");
    if (!file)
    {
        ESP_LOGE("cat", "Failed to open file: %s", path.c_str());
        return 1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), file))
    {
        printf("%s", buffer);
    }
    printf("\r\n");
    fclose(file);
    return 0;
}

void CmdFilesystem::printTree(const std::string &path, const std::string &prefix)
{
    DIR *dir = opendir(path.c_str());
    if (!dir)
    {
        printf("Cannot open directory: %s\n", path.c_str());
        return;
    }

    struct dirent *entry;
    std::vector<std::string> entries;

    // Read directory entries
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue; // Skip "." and ".."
        }
        entries.push_back(entry->d_name);
    }

    closedir(dir);

    // Sort entries for consistent output
    std::sort(entries.begin(), entries.end());

    for (size_t i = 0; i < entries.size(); ++i)
    {
        std::string full_path = joinPath(path, entries[i]);
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0)
        {
            continue; // Skip if stat fails
        }

        std::string type = getFileType(st);
        std::string name = entries[i];

        // Print current entry
        printf("%s", prefix.c_str());
        if (i == entries.size() - 1)
        {
            printf("└── ");
        }
        else
        {
            printf("├── ");
        }
        printf("%s %s\n", type.c_str(), name.c_str());

        // Recursively print subdirectories
        if (S_ISDIR(st.st_mode))
        {
            printTree(full_path, prefix + (i == entries.size() - 1 ? "    " : "│   "));
        }
    }
}

int CmdFilesystem::cmdTree(int argc, char **argv)
{
    static struct
    {
        struct arg_str *dirname;
        struct arg_end *end;
    } tree_args;

    tree_args.dirname = arg_str0(nullptr, nullptr, "<path>", "Directory to list (default: current directory)");
    tree_args.end = arg_end(1);

    int nerrors = arg_parse(argc, argv, (void **)&tree_args);
    if (nerrors > 0)
    {
        arg_print_errors(stderr, tree_args.end, argv[0]);
        return 1;
    }

    std::string input_path{tree_args.dirname->sval[0]};
    std::string path = (input_path.compare(".") != 0) ? joinPath(current_path, input_path) : current_path;

    printTree(path, "");
    return 0;
}

void CmdFilesystem::registerCommands()
{
    esp_console_cmd_t cd_cmd = {
        .command = "cd",
        .help = "Change the current directory",
        .hint = "<path>",
        .func = &cmdCd,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cd_cmd));

    esp_console_cmd_t mkdir_cmd = {
        .command = "mkdir",
        .help = "Create a directory",
        .hint = "<directory>",
        .func = &cmdMkdir,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&mkdir_cmd));

    esp_console_cmd_t ls_cmd = {
        .command = "ls",
        .help = "List files in the current directory",
        .hint = "[path]",
        .func = &cmdLs,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&ls_cmd));

    esp_console_cmd_t pwd_cmd = {
        .command = "pwd",
        .help = "Print the current working directory",
        .hint = nullptr,
        .func = &cmdPwd,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&pwd_cmd));

    esp_console_cmd_t rm_cmd = {
        .command = "rm",
        .help = "Remove files or directories",
        .hint = "[-r] [-f] <path>",
        .func = &cmdRm,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&rm_cmd));

    esp_console_cmd_t cat_cmd = {
        .command = "cat",
        .help = "Display the contents of a file",
        .hint = "<file>",
        .func = &cmdCat,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cat_cmd));

    esp_console_cmd_t tree_cmd = {
        .command = "tree",
        .help = "List directory contents in a tree-like format",
        .hint = "[path]",
        .func = &cmdTree,
        .argtable = nullptr};
    ESP_ERROR_CHECK(esp_console_cmd_register(&tree_cmd));
}
