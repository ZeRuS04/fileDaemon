#if !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <dirent.h>
#include <sys/file.h>
#include <execinfo.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>

// лимит для установки максимально кол-во открытых дискрипторов
#define FD_LIMIT			1024*10

#define RMODE_DIR_NAME      0
#define RMODE_TIME          1
#define RMODE_EMPTY         2

#define MAX_BUFFER          1024

#define IDENT   "filecheckdaemon"

signal(SIGHUP, signal_handler);
signal(SIGTERM, signal_handler);

int g_childs_pid[MAX_BUFFER];
int g_childs_count = 0;

string g_dir[MAX_BUFFER];
long int g_time[MAX_BUFFER];

int g_count = 0;

// функция записи лога
void WriteLog(char* msg)
{
    syslog(LOG_INFO, msg);
}

// функция загрузки конфига
int LoadConfig(char* FileName)
{
    FILE* f;

    f = fopen(FileName, "r");
    if (!f)
    {
        syslog(LOG_ERR, "[MONITOR] Fork failed (%s)\n", strerror(errno));
        return -1;
    }

    bool new_line = false;

    int rmode = RMODE_DIR_NAME;
    string dir_str("");
    string time_str("");

    int c;
    while ((c = getchar()) != EOF) {
        if(g_count == MAX_BUFFER-1) {
            break;
        }

        switch(c) {
        case (int)" ":
        case (int)"\t":
        case (int)"\r":
            if(rmode == RMODE_DIR_NAME);
                rmode = RMODE_TIME;
            else
                rmode = RMODE_EMPTY;
            break;
        case (int)"\n":
            new_line = true;
            break;
        default:
            if(new_line){
                dir_count++;
                rmode = RMODE_DIR_NAME;
                new_line = false;

                if(dir_str != "") {
                    g_dir[g_count];
                    g_count++;
                    long int time = strtol(time_str.c_str(), NULL, 10);
                    if(time == 0)
                        time = 60;
                    g_time[g_count] = time;
                }
            }
            if(rmode != RMODE_EMPTY) {
                if(rmode == RMODE_DIR_NAME) {
                    dir_str += c;
                }
                if(rmode == RMODE_TIME)
                    time_str += c;
            }
            break;
        }
    }

    fclose(f);
    return 0;
}


// функция обработки сигналов
static void signal_handler(int sig)
{
    switch(sig) {
    case SIGHUP:
        syslog(LOG_FILE, "hangup signal catched");
        break;
    case SIGTERM:
        syslog(LOG_FILE, "terminate signal catched");
        // TODO destroy all childs
        exit(0);
        break;
    }
}

int WorkingLoop()
{
    int pid;
    int index = -1;
    for(int i = 0; i < g_count; i++) {
        pid = fork();

        if (pid == -1) // если произошла ошибка
        {
            // запишем в лог сообщение об этом
            WriteLog("[WorkingLoop] Fork failed (%s)\n", strerror(errno));
        }
        else if (!pid) // если мы потомок
        {
            index = i;
            break;
        }
        else // если мы родитель
        {
            g_childs_pid[g_childs_count] = pid;
            g_childs_count++;
        }
    }

    if (!pid) // если мы потомок
    {
        while() {
            DIR * dir;
            struct dirent *entry;
            dir = opendir(g_dir[index]);
            while(( entry = readdir(dir)) != NULL ) {

            }
            sleep(g_time[index]);
        }
        exit(status);
    }
    // бесконечный цикл работы
    for (;;)
    {

        /*
        // если необходимо создать потомка
        if (need_start)
        {
            // создаём потомка
            pid = fork();
        }

        need_start = 1;

        if (pid == -1) // если произошла ошибка
        {
            // запишем в лог сообщение об этом
            WriteLog("[MONITOR] Fork failed (%s)\n", strerror(errno));
        }
        else if (!pid) // если мы потомок
        {
            // данный код выполняется в потомке

            // запустим функцию отвечающую за работу демона

            // завершим процесс
            exit(status);
        }
        else // если мы родитель
        {
            // данный код выполняется в родителе

        }
        */
    }
}

int main(int argc, char** argv)
{
    int status;
    int pid;

    // если параметров командной строки меньше двух, то покажем как использовать демана
    if (argc != 2)
    {
        printf("Usage: ./filedaemon filename.cfg\n");
        return -1;
    }

    // загружаем файл конфигурации
    status = LoadConfig(argv[1]);

    // open syslog:
    openlog(IDENT, LOG_PID, LOG_DAEMON);

    if (!status) // если произошла ошибка загрузки конфига
    {
        printf("Error: Load config failed\n");
        return -1;
    }

    // создаем потомка
    pid = fork();

    if (pid == -1) // если не удалось запустить потомка
    {
        // выведем на экран ошибку и её описание
        printf("Start Daemon Error: %s\n", strerror(errno));

        return -1;
    }
    else if (!pid) // если это потомок
    {
        // данный код уже выполняется в процессе потомка
        // разрешаем выставлять все биты прав на создаваемые файлы,
        // иначе у нас могут быть проблемы с правами доступа
        umask(0);

        // создаём новый сеанс, чтобы не зависеть от родителя
        setsid();

        // переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
        // к примеру с размантированием дисков
        chdir("/");

        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Данная функция будет осуществлять слежение за процессом
        status = WorkingLoop();


        return status;
    }
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
}

