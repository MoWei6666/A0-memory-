#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <cstdlib>
#include <thread>
#include <sys/inotify.h>
#include <string>
#include <array>
#include <dirent.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <sys/wait.h>


const std::string log_path = "/sdcard/Android/MW_A0/日志.txt";
const std::string memoryconf_path = "/sdcard/Android/MW_A0/配置文件.conf";
const std::string standby_conf = "/sdcard/Android/MW_A0/休眠进程.conf";
const std::string Kill_process_conf = "/sdcard/Android/MW_A0/乖巧进程.conf";
const std::string HookLmkd_path = "/data/adb/modules/MW_A0/system/bin/hooklmkd.sh";
const std::string log_conf = "/sdcard/Android/MW_A0/详细日志.conf";
const std::string Lmkd_adj = "/sdcard/Android/MW_A0/兼容模式.conf";
const std::string APP_Survival = "/sdcard/Android/MW_A0/Hook_LMKD.conf";
const std::string screen_kill  = "/sdcard/Android/MW_A0/息屏清理.conf";
const std::string get_screen = "/data/adb/modules/MW_A0/system/bin/GetScreen.sh"; 
const std::string get_screen_log = "/data/adb/modules/MW_A0/system/bin/GetScreen.log"; 
const std::string Lmkd_adj_commands = 
        "resetprop -n ro.lmk.use_psi true\n"
        "resetprop -n ro.lmk.use_new_strategy true\n"
        "resetprop -n ro.lmk.use_minfree_levels false\n"
        "resetprop -n persist.device_config.lmkd_native.use_minfree_levels false\n"
        "resetprop -n ro.lmk.low 1001\n"
        "resetprop -n ro.lmk.medium 1001\n"
        "resetprop -n ro.lmk.critical 0\n"
        "resetprop -n ro.lmk.critical_upgrade false\n"
        "resetprop -n ro.lmk.upgrade_pressure 100\n"
        "resetprop -n ro.lmk.downgrade_pressure 100\n"
        "resetprop -n ro.lmk.kill_heaviest_task false\n"
        "resetprop -n ro.lmk.kill_timeout_ms 100\n"
        "resetprop -n ro.lmk.psi_partial_stall_ms 200\n"
        "resetprop -n ro.lmk.psi_complete_stall_ms 700\n"
        "resetprop -n ro.lmk.thrashing_limit 100\n"
        "resetprop -n ro.lmk.thrashing_limit_decay 10\n"
        "resetprop -n ro.lmk.swap_util_max 100\n"
        "resetprop -n ro.lmk.swap_free_low_percentage 0\n"
        "resetprop -n ro.lmk.filecache_min_kb 0\n"
        "resetprop -n ro.lmk.stall_limit_critical 100\n"
        "resetprop -n ro.lmk.delay_monitors_until_boot true\n"
        "resetprop -n ro.lmk.enable_adaptive_lmk false\n"
        "stop lmkd && start lmkd";
std::string executeCommand(const std::string& command) {
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, 128, pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }
    return result;
}
std::string readStatusFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << filePath << std::endl;
        return "";
    }
    
    std::string status;
    getline(file, status);
    file.close();
    
    // 可选：转换为小写，使比较不区分大小写
    for(auto &c : status) c = tolower(c);
    
    return status;
}
void Log(const std::string& message) {
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", local_time);

    std::ofstream logfile(log_path, std::ios_base::app);
    if (logfile.is_open()) {
        logfile << time_str << " " << message << std::endl;
        logfile.close();
    }
    else {
        std::cerr << "无法打开日志文件" << std::endl;
    }
}
void ExecuteCommand(const std::string& command, std::string& output) {
    FILE* fp = popen(command.c_str(), "r");
    if (fp == nullptr) {
        std::cerr << "执行命令失败" << std::endl;
        return;
    }

    char buffer[128];
    while (fgets(buffer, 128, fp) != nullptr) {
        output += buffer;
    }

    pclose(fp);
}
void Clear_logs(){
    std::ofstream ofs;
    ofs.open(log_path, std::ofstream::out | std::ofstream::trunc);
    ofs.close();
}
void ReadConfigFile(const std::string& filename, std::vector<std::string>& killList) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t found = line.find("KILL ");
        if (found != std::string::npos) {
            std::string packageName = line.substr(found + 5); // 5 是 "KILL " 的长度
            killList.push_back(packageName);
        }
    }

    file.close();
}
/*void ReadStopAPP(const std::string& filename, std::vector<std::string>& stopList) {
   if (!file.is_open()) {
        std::cerr << "无法打开配置文件" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("STOP ") != std::string::npos) {
            std::string packageName2 = line.substr(4); // 3 是 "休眠 " 的长度
            stopList.push_back(packageName2);
        }
    }

    file.close();
}
*/
void readConfig(const std::string& filename, std::vector<std::string>& whiteList) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("standby ") != std::string::npos) {
            std::string packageName1 = line.substr(8); // 3 是 "休眠 " 的长度
            whiteList.push_back(packageName1);
        }
    }

    file.close();
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

int InotifyMain(const char *dir_name, uint32_t mask) {
    int fd = inotify_init();
    if (fd < 0) {
        std::cerr << "Failed to initialize inotify." << std::endl;
        return -1;
    }

    int wd = inotify_add_watch(fd, dir_name, mask);
    if (wd < 0) {
        std::cerr << "Failed to add watch for directory: " << dir_name << std::endl;
        close(fd);
        return -1;
    }

    const int buflen = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buf[buflen];
    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        int iRet = select(fd + 1, &readfds, nullptr, nullptr, nullptr);
        if (iRet < 0) {
            break;
        }

        int len = read(fd, buf, buflen);
        if (len < 0) {
            std::cerr << "Failed to read inotify events." << std::endl;
            break;
        }

        const struct inotify_event *event = reinterpret_cast<const struct inotify_event *>(buf);
        if (event->mask & mask) {
            break;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    return 0;
}

void HookLMKD() {
    FILE* fp = popen("pidof lmkd", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
    }

    char pidofLmkd[128];
    if (fgets(pidofLmkd, sizeof(pidofLmkd)-1, fp) != NULL) {
    }
    pclose(fp);

    const char* lmkd = pidofLmkd;
    std::string lmkd_str = lmkd;
Log(std::string("当前LMKD的PID为:") + lmkd_str);
int result = system("/data/adb/modules/MW_A0/system/bin/hooklmkd.sh");
    if (result == 0) {
        // 命令执行成功
            Log("LMKD已成功Hook");
    } else {
            Log("LMKD Hook失败");
    }
}

void Kill_process(){
    std::vector<std::string> killList;
    ReadConfigFile(memoryconf_path, killList);

    // 将包名存储到const char* kill_list中
    std::string killListStr;
    for (const auto& packageName : killList) {
        killListStr += packageName + " ";
    }
    const char* kill_list = killListStr.c_str();
    // 执行pidof命令
    std::string pidCommand = "pidof ";
    pidCommand += kill_list;
    std::string killPidList;
    ExecuteCommand(pidCommand, killPidList);

    // 将pid列表存储到const char* kill_pid_list中
    const char* kill_pid_list = killPidList.c_str();

    std::istringstream iss(kill_pid_list);
    std::string pid;
    while (iss >> pid) {
        int pid_int = std::stoi(pid);
        kill(pid_int, SIGKILL);
        if (fileExists(log_conf)){
        Log("进程:" + pid + " 已被杀死");
        }
    }
}
void standby_process() {
  std::vector<std::string> whiteList;
readConfig(memoryconf_path, whiteList);
for (const auto& packageName1 : whiteList) {
    std::string standby = "am set-inactive " + packageName1 + " true"; 
        // 这里不能使用const char*因为他不能使用加法
    system(standby.c_str());
    if (fileExists(log_conf)) {    
        Log("进程: " + packageName1 + " 已休眠");
    } 
}
}
void LMKD_adj(){
    std::string pidofLmkd = executeCommand("pidof lmkd");
    Log("当前LMKD的PID为: " + pidofLmkd);
    int result = system(Lmkd_adj_commands.c_str());
    if (result == 0) {
        Log("LMKD已成功调整");
    } else {
        Log("LMKD调整失败");
    }
}
/*void STOPAPP(){
 std::vector<std::string> stopList;
ReadStopAPP(memoryconf_path, stopList);
for (const auto& packageName2 : stopList) {
    std::string stopAPP = "am force-stop " + packageName2; 
    system(stopAPP.c_str());
}
}
*/
int main(){
    Log("此版本为:91测试版 编译时间:2024/06/21 13:32");
    int count = 0;
    Clear_logs();
    usleep(500);
   if (fileExists(memoryconf_path)){
       Log("配置文件存在");
       //开始进行下一步操作
      }else{
       Log("配置文件不存在 请重启安装模块再试");
   }
if (fileExists(Lmkd_adj)) {
        Log("已开启兼容模式 将不再Hook LMKD 将进行对LMKD进行兼容调整");
        LMKD_adj();
    } 
if (fileExists(Lmkd_adj)) {
  if (fileExists(APP_Survival)) {
    Log("检测到你开启了兼容模式请关闭");
  }     
} else {
    if (fileExists(APP_Survival)) {
      HookLMKD();
    }
}
if (fileExists(Kill_process_conf)){
       while (true) {
  /*   if (fileExists(screen_kill)){
       system(get_screen.c_str());
std::string status = readStatusFromFile(get_screen_log);
 if (status.empty()) {
        std::cerr << "Failed to read status from file." << std::endl;
        return 1;
    }
       if (status == "false") {
        std::cout << "The status is false. Executing KillApp..." << std::endl;
        STOPAPP();
    }
     }*/
           InotifyMain("/dev/cpuset/top-app", IN_MODIFY);
           usleep(500);
           count++; // 增加计数器
           if (count == 4) {
               Kill_process(); 
           if (fileExists(standby_conf)){
               standby_process();
           }
               count = 0; // 重置计数器
         }
      }
   } else {
while (true){
if (fileExists(standby_conf)){
InotifyMain("/dev/cpuset/top-app", IN_MODIFY);
     standby_process();
}
  }
}
}
