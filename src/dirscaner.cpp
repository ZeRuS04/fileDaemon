#include "dirscaner.h"

DirScaner::DirScaner()
{
    // open syslog:
    openlog(IDENT, LOG_PID, LOG_DAEMON);
}

DirScaner::~DirScaner()
{

    syslog(LOG_NOTICE, "[~DirScaner] closing thread\n");
    closelog();
}

int DirScaner::loadConfig(const char *fileName)
{
    string line;
    ifstream cfgFile(fileName);
    if (cfgFile.is_open())
    {
        while (getline (cfgFile, line))
        {
            vector<string> sVec;
            sVec = split(line, ' ');
            string dirStr = sVec[0];
            string timeStr = sVec[1];

            long int time = strtol(timeStr.c_str(), NULL, 10);
            if(time == 0)
                time = 60;
            f_info i;
            i.dirName = dirStr;
            i.mTime = time;
            m_buffer.push_back(i);
        }
        cfgFile.close();
    }
    else {
        syslog(LOG_ERR, "[MONITOR] Open config failed (%s)\n", strerror(errno));
        return -1;
    }
    return 0;
}

void DirScaner::signalHandler(int sig)
{
    switch(sig) {
    case SIGHUP:
        syslog(LOG_NOTICE, "hangup signal catched");
        break;
    case SIGTERM:
        syslog(LOG_NOTICE, "terminate signal catched");
        // TODO destroy all childs
        exit(0);
        break;
    }
}

int DirScaner::scanDir(const char *str_dir, bool isInit)
{
    struct dirent *entry;
    DIR * dir;
    struct stat stbuf;

    if(isInit) {
        syslog(LOG_INFO, "[ScanDir] Entering to %s...\n", str_dir);
    }

    if ((dir = opendir(str_dir)) == NULL) {
        syslog(LOG_INFO, "[ScanDir] Work failed (Can't open dir %s)\n", str_dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *name = entry->d_name;
        string fullName = str_dir;
        fullName += "/";
        fullName.append(name);

        if (strcmp(name, ".") == 0
                || strcmp(name, "..") == 0)
            continue;     /* пропустить себя и родителя */

        if (stat(fullName.c_str(), &stbuf) == -1) {
            syslog(LOG_INFO, "[ScanDir] Work failed (Can't access to file %s)\n", fullName.c_str());
            continue;
        }

        if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
            scanDir(fullName.c_str(), isInit);
        else {
            map<string,long int>::iterator cit = m_map.find(fullName);

            if(cit == m_map.end() && !isInit) {
                syslog(LOG_INFO, "[MODIFY_CHECK] New file was found: %s\n", fullName.c_str());
                m_map.insert( pair<string,long int>(fullName.c_str(), stbuf.st_mtime ) );
            } else
                if(stbuf.st_mtime != cit->second  && !isInit)
                {
                    syslog(LOG_INFO, "[MODIFY_CHECK] File was modified: %s\n", fullName.c_str());
                    m_map[fullName] = stbuf.st_mtime;
                } else
                    m_map.insert ( pair<string,long int>(fullName.c_str(), stbuf.st_mtime ) );
        }

    }
    closedir(dir);
    return 0;
}

int DirScaner::run() {
    int pid;
    int index = -1;

    for(unsigned int i = 0; i < m_buffer.size(); i++) {
        pid = fork();

        if (pid == -1) // если произошла ошибка
        {
            // запишем в лог сообщение об этом
            syslog(LOG_INFO, "[run] Fork failed (Can't create thread)\n");
            return -1;
        }
        else if (!pid) // если мы потомок
        {
            syslog(LOG_INFO, "[run] Fork successful\n");
            signal(SIGHUP, DirScaner::signalHandler);
            signal(SIGTERM, DirScaner::signalHandler);
            index = i;
            break;
        }
        else // если мы родитель
        {
            m_childPID.push_back(pid);
        }
    }

    if (!pid) // если мы потомок
    {
        bool isInit = true;
        int status = 0;
        while(true) {
            status = scanDir(m_buffer[index].dirName.c_str(), isInit);
            if(status)
                break;
            if(isInit) {
                syslog(LOG_INFO, "[run] initial map successfull\n");
                isInit = false;
            }

            sleep(m_buffer[index].mTime);
        }
        return status;
    } else {
        // бесконечный цикл работы
        while(true){}
    }
    return 0;
}

