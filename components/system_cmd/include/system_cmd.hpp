#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "argtable3/argtable3.h"

    class CmdSystem
    {
    public:
        static void registerSystem();

    private:
        static void registerSystemCommon();
        static void registerSystemDeepSleep();
        static void registerSystemLightSleep();

        static int getVersion(int argc, char **argv);
        static int restart(int argc, char **argv);
        static int freeMem(int argc, char **argv);
        static int heapSize(int argc, char **argv);
        static int tasksInfo(int argc, char **argv);
        static int logLevel(int argc, char **argv);
        static int deepSleep(int argc, char **argv);
        static int lightSleep(int argc, char **argv);

        static void registerVersion();
        static void registerRestart();
        static void registerFree();
        static void registerHeap();
        static void registerTasks();
        static void registerLogLevel();
        static void registerDeepSleep();
        static void registerLightSleep();
    };

#ifdef __cplusplus
}
#endif
