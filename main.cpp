#if !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include "src/dirscaner.h"


int main(int argc, char** argv)
{
    int status;
    int pid;

    // если параметров командной строки меньше двух, то покажем как использовать демана
    if (argc != 2)
    {
        cout << "Usage: ./filedaemon filename.cfg" << endl;
        return -1;
    }

    DirScaner scaner;

    syslog(LOG_INFO, "[main] Start daemon\n");

    // загружаем файл конфигурации
    status = scaner.loadConfig(argv[1]);

    if (status) // если произошла ошибка загрузки конфига
    {
        cout << "Error: Load config failed" << endl;
        return -1;
    }

    // создаем потомка
    pid = fork();

    if (pid == -1) // если не удалось запустить потомка
    {
        // выведем на экран ошибку и её описание
        cout << "Start Daemon Error: " << strerror(errno) << endl;

        return -1;
    }
    else if (!pid) // если это потомок
    {
        syslog(LOG_INFO, "[WorkingLoop] Fork successful\n");
        // данный код уже выполняется в процессе потомка
        // разрешаем выставлять все биты прав на создаваемые файлы,
        // иначе у нас могут быть проблемы с правами доступа
        umask(0);

        // создаём новый сеанс, чтобы не зависеть от родителя
        setsid();

        signal(SIGHUP, DirScaner::signalHandler);
        signal(SIGTERM, DirScaner::signalHandler);
        // переходим в корень диска, если мы этого не сделаем, то могут быть проблемы.
        // к примеру с размантированием дисков
        chdir("/");

        // закрываем дискрипторы ввода/вывода/ошибок, так как нам они больше не понадобятся
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Данная функция будет осуществлять слежение за процессом
        status = scaner.run();

        return status;
    }
    else // если это родитель
    {
        // завершим процес, т.к. основную свою задачу (запуск демона) мы выполнили
        return 0;
    }
}

