#pragma once

#include <string>
#include <string_view>

#ifdef __cplusplus
extern "C"
{
#endif
#include <sys/stat.h>
#include <dirent.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"

    class CmdFilesystem
    {
    public:
        static void registerCommands();
        static std::string _current_path;

        static SemaphoreHandle_t _prompt_change_sem;

    private:
        static char *TAG;

        static struct
        {
            struct arg_str *dirname;
            struct arg_end *end;
        } ls_args;

        static struct
        {
            struct arg_str *dirname;
            struct arg_end *end;
        } cd_args;

        static struct
        {
            struct arg_str *dirname;
            struct arg_end *end;
        } mkdir_args;

        static struct
        {
            struct arg_lit *recursive;
            struct arg_lit *force;
            struct arg_str *path;
            struct arg_end *end;
        } rm_args;

        static struct
        {
            struct arg_str *filename;
            struct arg_end *end;
        } cat_args;

        static struct
        {
            struct arg_str *dirname;
            struct arg_end *end;
        } tree_args;

        const std::string home_path{"/sdcard"};
        static std::string getFileType(const struct stat &st);
        static std::string joinPath(std::string_view base, std::string_view sub);

        static int cmdLs(int argc, char **argv);
        static int cmdCd(int argc, char **argv);
        static int cmdMkdir(int argc, char **argv);
        static int cmdPwd(int argc, char **argv);
        static int cmdRm(int argc, char **argv);
        static int cmdCat(int argc, char **argv);
        static int cmdTree(int argc, char **argv);

        static int removeDirectory(const char *path);
        static void printTree(const std::string &path, const std::string &prefix);

        static std::string removePrefixIfExists(const std::string &str, const char *prefix);
    };

#ifdef __cplusplus
}
#endif