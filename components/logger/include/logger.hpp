#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <time.h>
#include <chrono>

#ifdef __cplusplus
extern "C"
{
#endif

    class LoggerBase
    {
    public:
        // 构造函数，传入挂载点路径
        explicit LoggerBase(const std::string &mount_point);

        // 析构函数
        virtual ~LoggerBase();

        // 初始化日志文件
        bool init(const std::string &file_name);

        // 生成文件头
        std::string generate_file_header();
        std::string generate_file_header_asc();

        void getFileExtension(const std::string &filename);

        // 记录文本信息
        void log(const std::string &message);
        void log(const std::string &message, int max_lines);

        bool is_string_group_exists(const std::vector<std::string> &string_group, const std::string &delimiter);

        bool log_string_group(const std::vector<std::string> &string_group, const std::string &delimiter);

        // 提供公共接口关闭文件
        void shutdown();

        // 获取当前文件的行数
        int get_line_count() const;

        // 获取当前文件的行数
        int get_init_state() const;

        // 获取当前文件的创建时间
        std::chrono::time_point<std::chrono::steady_clock> get_file_time() const;

    protected:
        const char *TAG{"Logger"};
        // 打开文件
        virtual bool open_file(const std::string &file_path);

        // 关闭文件
        virtual void close_file();

        // 写入文件
        virtual bool write_to_file(const std::string &message);

        // 从文件中读取行数
        int read_line_count(const std::string &file_path);

    private:
        std::string mount_point;                                              // 挂载点路径
        FILE *file;                                                           // 文件指针
        std::string _file_extention;                                          // 文件后缀名
        bool is_initialized;                                                  // 是否已初始化
        int line_count;                                                       // 当前文件的行数
        std::string current_file_path;                                        // 当前文件完整地址
        std::chrono::time_point<std::chrono::steady_clock> time_file_created; // 文件创建时间
    };

#ifdef __cplusplus
}
#endif
