/**
 * 头文件包含规范
 * 1.本类的声明（第一个包含本类h文件，有效减少以来）
 * 2.C系统文件
 * 3.C++系统文件
 * 4.其他库头文件
 * 5.本项目内头文件
*/

// using std::string; // 可以在整个cc文件和h文件内使用using， 禁止使用using namespace xx污染命名空间

#include "netlib.h"
#include "ConfigFileReader.h"
#include "HttpConn.h"
#include "util.h"
#include "DBPool.h"
#include "CachePool.h"

#include "Logging.h"
#include "AsyncLogging.h"
#include "ApiUpload.h"
#include "ApiDealfile.h"
// 
off_t kRollSize = 1 * 1000 * 1000;    // 只设置1M
static AsyncLogging *g_asyncLog = NULL;

static void asyncOutput(const char *msg, int len)
{
  g_asyncLog->append(msg, len);
}

int InitLog()
{
    printf("pid = %d\n", getpid());

    char name[256] = "tuchuang";
    // strncpy(name, argv[0], sizeof name - 1);
    // 回滚大小kRollSize（1M）, 最大1秒刷一次盘（flush）
    AsyncLogging log(::basename(name), kRollSize, 1);  // 注意，每个文件的大小 还取决于时间的问题，不是到了大小就一定换文件。
    // Logger::setOutput(asyncOutput);   // 不是说只有一个实例

    // g_asyncLog = &log;
    log.start();        // 启动日志写入线程


    LOG_INFO << "InitLog ok"; // 47个字节

    return 0;
}

void http_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
    
    if (msg == NETLIB_MSG_CONNECT)
    {
		// 这里是不是觉得很奇怪,为什么new了对象却没有释放?
		// 实际上对象在被Close时使用delete this的方式释放自己
        CHttpConn* pConn = new CHttpConn();
        pConn->OnConnect(handle);
    }
    else
    {
        LOG_ERROR << "!!!error msg: " << msg;
    }
}
 


int main(int argc, char* argv[])
{
	signal(SIGPIPE, SIG_IGN);

    // 初始化日志，暂且只是打印到屏幕上，课程不断迭代
    InitLog();

    // 初始化mysql、redis连接池，内部也会读取读取配置文件tc_http_server.conf
    CacheManager* pCacheManager = CacheManager::getInstance();
	if (!pCacheManager) {
		LOG_ERROR << "CacheManager init failed";
        std::cout << "CacheManager init failed";
		return -1;
	}

	CDBManager* pDBManager = CDBManager::getInstance();
	if (!pDBManager) {
		LOG_ERROR << "DBManager init failed";
        std::cout << "DBManager init failed";
		return -1;
	}

    // 读取配置文件
	CConfigFileReader config_file("tc_http_server.conf");

    char* http_listen_ip = config_file.GetConfigName("HttpListenIP");
    char* str_http_port = config_file.GetConfigName("HttpPort");

    char* dfs_path_client = config_file.GetConfigName("dfs_path_client");
    char* web_server_ip = config_file.GetConfigName("web_server_ip");
    char* web_server_port = config_file.GetConfigName("web_server_port");
    char* storage_web_server_ip = config_file.GetConfigName("storage_web_server_ip");
    char* storage_web_server_port = config_file.GetConfigName("storage_web_server_port");

    // 将配置文件的参数传递给对应模块
    ApiUploadInit(dfs_path_client, web_server_ip, web_server_port, storage_web_server_ip, storage_web_server_port);
    ApiDealfileInit(dfs_path_client);

    // 检测监听ip和端口
	if (!http_listen_ip || !str_http_port) {
		printf("config item missing, exit... ip:%s, port:%s", http_listen_ip, str_http_port);
		return -1;
	}

    uint16_t http_port = atoi(str_http_port);

	int ret = netlib_init();

	if (ret == NETLIB_ERROR)
		return ret; 
    
    CStrExplode http_listen_ip_list(http_listen_ip, ';');
    for (uint32_t i = 0; i < http_listen_ip_list.GetItemCnt(); i++) {
        ret = netlib_listen(http_listen_ip_list.GetItem(i), http_port, http_callback, NULL);
        if (ret == NETLIB_ERROR)
            return ret;
    }
    

	printf("server start listen on:For http:%s:%d\n", http_listen_ip, http_port);
    init_http_conn();

	printf("now enter the event loop...\n");
    
    writePid();

	netlib_eventloop();
	return 0;
}
