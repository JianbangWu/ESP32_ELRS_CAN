#include "logger.hpp"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "esp_log.h"

// 构造函数
LoggerBase::LoggerBase(const std::string &mount_point)
    : mount_point(mount_point), file(nullptr), is_initialized(false) {}

// 析构函数
LoggerBase::~LoggerBase()
{
    close_file();
}

// 初始化日志文件
bool LoggerBase::init(const std::string &file_name)
{
    std::string file_path = mount_point + "/" + file_name;
    ESP_LOGI(TAG, "filepath:=%s", file_path.c_str());

    if (!open_file(file_path))
    {
        return false;
    }

    // 生成并写入文件头
    std::string header = generate_file_header();
    ESP_LOGI(TAG, "%s", header.c_str());
    if (!write_to_file(header))
    {
        close_file();
        return false;
    }

    is_initialized = true;
    return true;
}

// 生成文件头
std::string LoggerBase::generate_file_header()
{
    // 获取当前时间
    std::time_t now = std::time(nullptr);
    std::tm *time_info = std::localtime(&now);

    // 格式化时间戳
    std::ostringstream oss;
    oss << "File Created: "
        << std::put_time(time_info, "%Y-%m-%d %H:%M:%S") << "\n"
        << "System Event: ESP32 Logging Initialized\n"
        << "-------------------------------------\n";

    return oss.str();
}

// 记录文本信息
void LoggerBase::log(const std::string &message)
{
    if (!is_initialized || !file)
    {
        ESP_LOGE(TAG, "ERROR");
        return;
    }

    if (false == write_to_file(message + "\n"))
    {
        ESP_LOGE(TAG, "CANT WRITE TO FILE!");
    }
}

void LoggerBase::shutdown()
{
    if (is_initialized)
    {
        close_file();
        is_initialized = false; // 标记为未初始化
        printf("Logger shutdown successfully.\n");
    }
}

// 打开文件
bool LoggerBase::open_file(const std::string &file_path)
{
    file = fopen(file_path.c_str(), "a");
    return file != nullptr;
}

// 关闭文件
void LoggerBase::close_file()
{
    if (file)
    {
        fclose(file);
        file = nullptr;
    }
}

// 写入文件
bool LoggerBase::write_to_file(const std::string &message)
{
    if (fprintf(file, "%s", message.c_str()) < 0)
    {
        return false;
    }
    fflush(file); // 确保数据写入文件
    return true;
}