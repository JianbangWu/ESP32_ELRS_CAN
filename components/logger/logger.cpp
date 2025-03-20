#include "logger.hpp"
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "esp_log.h"

#include <sys/types.h>
#include <sys/stat.h>

// 构造函数
LoggerBase::LoggerBase(const std::string &mount_point)
    : mount_point(mount_point), file(nullptr), is_initialized(false), line_count(0)
{
    /* 检查目录是否存在 */
    struct stat st;
    if (0 != stat(mount_point.c_str(), &st))
    {
        ESP_LOGI(TAG, "Path not Exist");
        ESP_LOGI(TAG, "Creating Mount Path");
        if (mkdir(mount_point.c_str(), 0755) != 0)
        {
            ESP_LOGE(TAG, "Failed to create directory: %s", mount_point.c_str());
        }
    }
}

// 析构函数
LoggerBase::~LoggerBase()
{
    if (file)
    {
        printf("Closing file...\n");
        close_file();
    }
}

// 初始化日志文件
bool LoggerBase::init(const std::string &file_name)
{
    current_file_path = mount_point + "/" + file_name;

    getFileExtension(file_name);

    ESP_LOGI(TAG, "filepath:=%s", current_file_path.c_str());

    // 读取当前文件的行数
    line_count = read_line_count(current_file_path);

    if (!open_file(current_file_path))
    {
        return false;
    }

    // 生成并写入文件头
    // 如果文件是新创建的，写入文件头
    if (line_count == 0)
    {
        std::string header;
        if (std::string::npos == file_name.find(".asc"))
        {
            header = generate_file_header_asc();
        }
        else
        {
            header = generate_file_header();
        }

        if (!write_to_file(header))
        {
            ESP_LOGE(TAG, "Failed to write file header!");
            close_file();
            return false;
        }
        line_count++; // 文件头算作一行
    }
    is_initialized = true;
    ESP_LOGI(TAG, "Logger initialized successfully! Initial line count: %d\n", line_count);
    return true;
}

// 生成文件头
std::string LoggerBase::generate_file_header_asc()
{
    time_file_created = std::chrono::steady_clock::now();

    char buf[100];
    std::chrono::time_point<std::chrono::system_clock> file_header_time;
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(file_header_time);

    std::strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M:%S", std::localtime(&start_time_t));
    std::ostringstream oss;
    oss << "date " << buf << ".471" << std::endl;
    oss << "base hex timestamps absolute" << std::endl;
    oss << "// version 7.0.0" << std::endl;

    return oss.str();
}

std::string LoggerBase::generate_file_header()
{
    std::time_t now = std::time(nullptr);
    std::tm *time_info = std::localtime(&now);

    std::ostringstream oss;
    oss << "File Created At: "
        << std::put_time(time_info, "%Y-%m-%d %H:%M:%S") << "\n"
        << "-------------------------------------\n";

    return oss.str();
}

// 获取文件扩展名
void LoggerBase::getFileExtension(const std::string &filename)
{
    // 查找最后一个 '.' 的位置
    size_t dotPos = filename.find_last_of('.');

    // 如果找到 '.' 并且它不是文件名中的第一个字符
    if (dotPos != std::string::npos && dotPos > 0)
    {
        // 返回从 '.' 开始到字符串末尾的子串
        _file_extention = filename.substr(dotPos);
        return;
    }
    _file_extention = "";
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

void LoggerBase::log(const std::string &message, int max_lines)
{
    if (!is_initialized || !file)
    {
        ESP_LOGE(TAG, "Logger not initialized or file not open!\n");
        return;
    }

    // 检查行数，如果达到阈值则创建新文件
    if (line_count >= max_lines)
    {
        ESP_LOGE(TAG, "Reached max lines (%d). Creating new file...", max_lines);
        shutdown();
        const size_t bufferSize = 64;
        char buffer[bufferSize];
        std::time_t now = std::time(nullptr);
        std::strftime(buffer, bufferSize, "%Y_%m_%d-%H_%M_%S", std::localtime(&now));
        std::string new_file_name = std::string(buffer) + _file_extention;
        init(new_file_name);
    }

    // 写入日志
    if (write_to_file(message + "\n"))
    {
        line_count++; // 更新行数
        // ESP_LOGE(TAG, "Message written to file: %s", message.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "Failed to write message to file!");
    }
}

bool LoggerBase::is_string_group_exists(const std::vector<std::string> &string_group, const std::string &delimiter)
{
    std::string target_line;
    for (size_t i = 0; i < string_group.size(); i++)
    {
        target_line += string_group[i];
        if (i < string_group.size() - 1)
        {
            target_line += delimiter;
        }
    }

    FILE *temp_file = fopen(current_file_path.c_str(), "r");
    if (!temp_file)
    {
        printf("Failed to open file for reading.\n");
        return false;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), temp_file))
    {
        std::string line(buffer);
        line.erase(line.find_last_not_of("\n") + 1); // 去掉换行符
        if (line == target_line)
        {
            fclose(temp_file);
            return true;
        }
    }

    fclose(temp_file);
    return false;
}

bool LoggerBase::log_string_group(const std::vector<std::string> &string_group, const std::string &delimiter)
{
    if (!is_initialized || !file)
    {
        printf("Logger not initialized or file not open!\n");
        return false;
    }

    // 检查字符串组是否已存在
    if (is_string_group_exists(string_group, delimiter))
    {
        printf("String group already exists. Skipping write.\n");
        return false;
    }

    // 将字符串组拼接为一行
    std::string line;
    for (size_t i = 0; i < string_group.size(); i++)
    {
        line += string_group[i];
        if (i < string_group.size() - 1)
        {
            line += delimiter;
        }
    }

    // 写入文件
    if (write_to_file(line + "\n"))
    {
        line_count++; // 更新行数
        printf("String group written to file: %s\n", line.c_str());
        return true;
    }
    else
    {
        printf("Failed to write string group to file!\n");
        return false;
    }
}

void LoggerBase::shutdown()
{
    if (is_initialized)
    {
        close_file();
        is_initialized = false; // 标记为未初始化
        line_count = 0;         // 重置行数
        ESP_LOGI(TAG, "Logger shutdown successfully.");
    }
}

int LoggerBase::get_line_count() const
{
    return line_count;
}

int LoggerBase::get_init_state() const
{
    return is_initialized;
}

std::chrono::time_point<std::chrono::steady_clock> LoggerBase::get_file_time() const
{
    return time_file_created;
}

// 打开文件
bool LoggerBase::open_file(const std::string &file_path)
{
    file = fopen(file_path.c_str(), "a");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path.c_str());
        return false;
    }
    ESP_LOGI(TAG, "File opened successfully: %s", file_path.c_str());
    return true;
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

int LoggerBase::read_line_count(const std::string &file_path)
{
    FILE *temp_file = fopen(file_path.c_str(), "r");
    if (!temp_file)
    {
        ESP_LOGE(TAG, "File does not exist or cannot be opened. Starting with line count 0.");
        return 0;
    }

    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), temp_file))
    {
        count++;
    }

    fclose(temp_file);
    ESP_LOGI(TAG, "Read line count from file: %d", count);
    return count;
}
