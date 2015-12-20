#ifndef DIRSCANER_H
#define DIRSCANER_H

#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include <map>

#define IDENT   "filecheckdaemon"

using namespace std;

typedef struct f_info{
    string dirName;
    long int mTime;
} f_info;

class DirScaner
{
public:
    DirScaner();
    ~DirScaner();

    vector<string> split(const string &s, char delim) {
        vector<string> elems;
        stringstream ss(s);
        string item;
        while (getline(ss, item, delim)) {
            elems.push_back(item);
        }
        return elems;
    }

    // функция загрузки конфига
    int loadConfig();

    // функция обработки сигналов
    static void signalHandler(int sig);

    int scanDir(const char *str_dir, bool isInit);

    int startProcesses();

    void destroyAllChildren();

    int run();

    int setConfig(const char* fileName);

private:
    vector<f_info> m_buffer;
    vector<int> m_childPID;

    map<string, long int> m_map;

    string m_config;
};

#endif // DIRSCANER_H
