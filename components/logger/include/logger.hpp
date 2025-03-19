#pragma once

#include <string>
#include <vector>

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

        // 记录文本信息
        void log(const std::string &message);

        // 提供公共接口关闭文件
        void shutdown();

        protected:
        const char *TAG{"Logger"};
        // 打开文件
        virtual bool open_file(const std::string &file_path);

        // 关闭文件
        virtual void close_file();

        // 写入文件
        virtual bool write_to_file(const std::string &message);

    private:
        std::string mount_point; // 挂载点路径
        FILE *file;              // 文件指针
        bool is_initialized;     // 是否已初始化
    };

#ifdef __cplusplus
}
#endif
