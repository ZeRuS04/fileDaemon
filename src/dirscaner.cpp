#include "dirscaner.h"

bool g_destroySelf = false;
bool g_reloadCfg = false;

DirScaner::DirScaner()
    : m_config("")
{
    // open syslog:
    openlog(IDENT, LOG_PID, LOG_DAEMON);
}

DirScaner::~DirScaner()
{

    syslog(LOG_NOTICE, "[~DirScaner] closing thread\n");
    closelog();
}

int DirScaner::loadConfig()
{
    string line;
    ifstream cfgFile(m_config.c_str());

    m_buffer.clear();
    if (cfgFile.is_open())
    {
        while (getline (cfgFile, line))
        {
            vector<string> sVec;
            sVec = split(line, ' ');
            string dirStr = sVec[0];
            for(int k = 1; k < sVec.size()-1; k++) {
                dirStr += ' ';
                dirStr += sVec[k];
            }
            string timeStr = sVec[sVec.size()-1];

            long int time = strtol(timeStr.c_str(), NULL, 10);
            if(time == 0)
                time = 60;
            syslog(LOG_NOTICE, "[LoadConfig] Check %s every %d sec.\n", dirStr.c_str(), time);
            f_info i;
            i.dirName = dirStr;
            i.mTime = time;
            m_buffer.push_back(i);
        }
        cfgFile.close();
    }
    else {
        syslog(LOG_ERR, "[LoadConfig] Open config failed (%s)\n", strerror(errno));
        return -1;
    }
    return 0;
}

void DirScaner::signalHandler(int sig)
{

    switch(sig) {
    case SIGHUP:
        syslog(LOG_NOTICE, "SIGHUP signal catched");
        g_reloadCfg = true;
        break;
    case SIGTERM:
        syslog(LOG_NOTICE, "SIGTERM signal catched");
        g_destroySelf = true;
        break;
    }
}

int DirScaner::scanDir(const char *str_dir, bool isInit)
{
    struct dirent *entry;
    DIR * dir;
    struct stat stbuf;

    if(str_dir == NULL)
        return 0;

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
            } else {
                if(stbuf.st_mtime != cit->second  && !isInit)
                {
                    syslog(LOG_INFO, "[MODIFY_CHECK] File was modified: %s\n", fullName.c_str());
                    m_map[fullName] = stbuf.st_mtime;
                } else
                    m_map.insert ( pair<string,long int>(fullName.c_str(), stbuf.st_mtime ) );
            }
        }
    }
    closedir(dir);
    return 0;
}

int DirScaner::startProcesses()
{
    int pid;

    for(unsigned int i = 0; i < m_buffer.size(); i++) {
        pid = fork();

        if (pid == -1) // если произошла ошибка
        {
            // запишем в лог сообщение об этом
            syslog(LOG_INFO, "[run] Fork failed (Can't create thread)\n");
            return -255;
        }
        else if (!pid) // если мы потомок
        {
            syslog(LOG_INFO, "[run] Fork successful\n");
            return i;
        }
        else // если мы родитель
        {
            m_childPID.push_back(pid);
        }
    }
    return -1;
}

void DirScaner::destroyAllChildren()
{
    for(vector<int>::iterator i = m_childPID.begin(); i != m_childPID.end(); i++)
       kill(*i, SIGTERM);
    m_childPID.clear();
}

int DirScaner::run() {
    int retVal;
    bool reRun = false;
    do {
        retVal = startProcesses();

        syslog(LOG_INFO, "[run] retval %d\n", retVal);
        if (retVal >= 0) // если мы потомок
        {
            bool isInit = true;
            int status = 0;
            while(true) {
                status = scanDir(m_buffer[retVal].dirName.c_str(), isInit);
                if(status)
                    break;
                if(isInit) {
                    syslog(LOG_INFO, "[run] initial map successfull\n");
                    isInit = false;
                }

                if(g_destroySelf) {
                    exit(0);
                }
                sleep(m_buffer[retVal-1].mTime);
            }
            return status;
        } else if(retVal == -1){
            // бесконечный цикл работы
            while(true)
            {
                pause();
                if(g_destroySelf) {
                    destroyAllChildren();
                    exit(0);
                }
                if(g_reloadCfg){
                    destroyAllChildren();
                    loadConfig();
                    g_reloadCfg = false;
                    reRun = true;
                    break;
                }
            }
        }
    } while(reRun);

    return retVal;
}

int DirScaner::setConfig(const char *fileName)
{
    m_config = fileName;
    return loadConfig();
}

