#pragma once

// winsock 2 的头文件和库
#include <winsock2.h>
#include <MSWSock.h>
#pragma comment(lib,"ws2_32.lib")

#include <Windows.h>	//必须放在winsock的后面，有冲突
#include "ServerWnd.h"

// 缓冲区长度 (1024*8)
#define MAX_BUFFER_LEN 8192
#define DEFAULT_PORT 12345


// 在完成端口上投递的I/O操作的类型
typedef enum _OPERATION_TYPE
{
	ACCEPT_POSTED,                     // 标志投递的Accept操作
	SEND_POSTED,                       // 标志投递的是发送操作
	RECV_POSTED,                       // 标志投递的是接收操作
	NULL_POSTED                        // 用于初始化，无意义

}OPERATION_TYPE;

//每个io操作参数结构体定义
typedef struct _IO_CONTEXT {
	OVERLAPPED overlapped;				 // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)
	SOCKET socket_accept;				 // 这个网络操作所使用的Socket
	WSABUF wsabuf;						 // WSA类型的缓冲区，用于给重叠操作传参数的
	char buffer[MAX_BUFFER_LEN];		 // 这个是WSABUF里具体存字符的缓冲区
	OPERATION_TYPE	op_type;			 // 标识网络操作的类型(对应上面的枚举)

	// 初始化
	_IO_CONTEXT()
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		socket_accept = INVALID_SOCKET;
		ZeroMemory(buffer, MAX_BUFFER_LEN);
		wsabuf.buf = buffer;
		wsabuf.len = MAX_BUFFER_LEN;
		op_type = NULL_POSTED;
	}

	// 释放
	~_IO_CONTEXT()
	{
		if (socket_accept != INVALID_SOCKET)
		{
			closesocket(socket_accept);
			socket_accept = INVALID_SOCKET;
		}
	}

	// 重置缓冲区内容
	void ResetBuffer()
	{
		ZeroMemory(buffer, MAX_BUFFER_LEN);
	}
}IO_CONTEXT;


//每个完成端口结构体定义（每个客户端信息）
typedef struct _SOCKET_CONTEXT {
	SOCKET socket;											// 每一个客户端连接的Socket
	SOCKADDR_IN client_addr;								// 客户端的地址
	std::vector<IO_CONTEXT *> p_io_context_vector;			// 客户端网络操作的上下文数据，
															// 也就是说对于每一个客户端Socket，是可以在上面同时投递多个IO请求的
	// 初始化
	_SOCKET_CONTEXT()
	{
		socket = INVALID_SOCKET;
		ZeroMemory(&client_addr, sizeof(client_addr));
	}
	// 释放资源
	~_SOCKET_CONTEXT()
	{
		if (socket != INVALID_SOCKET)
		{
			closesocket(socket);
			socket = INVALID_SOCKET;
		}
		for (auto &var : p_io_context_vector)
		{
			delete var;
			var = NULL;
		}
		p_io_context_vector.clear();
		std::vector<IO_CONTEXT *>().swap(p_io_context_vector);
	}

	// 获取一个新的IoContext
	IO_CONTEXT *GetNewIOContext()
	{
		IO_CONTEXT *new_io = new IO_CONTEXT;
		p_io_context_vector.push_back(new_io);
		return new_io;
	}

	// 从向量中移除一个指定的IoContext
	void RemoveIOContext(IO_CONTEXT* &p_io_context)
	{
		for (auto i = p_io_context_vector.begin(); i < p_io_context_vector.end(); i++)
		{
			if (*i == p_io_context)
			{
				p_io_context_vector.erase(i);
				break;
			}
		}
		delete p_io_context;
		p_io_context = NULL;
	}

}SOCKET_CONTEXT;

class IOCPServer;

// 工人线程的线程参数
typedef struct _WORKER_PARAMS
{
	IOCPServer *p_iocp_server;		// 类指针，用于调用类中的函数
	int worker_id;					// 线程编号（没什么实质性的作用，用来观赏)

} WORKER_PARAMS;



class IOCPServer
{
public:
	IOCPServer() ;
	~IOCPServer();
	//绑定主窗口，用于显示信息
	void BingMainWnd(ServerWnd *dlg);

	//启动服务器
	bool StartServer();

	//停止服务器
	void StopServer();

private:
	//工人线程，为iocp请求工作
	static DWORD WINAPI WorkerThread(LPVOID lpParam);

	//加载socket库
	bool LoadSocketLib();	

	//卸载socker库
	void UnloadSocketLib();														

	//初始化iocp
	bool InitIOCP();							

	//初始化AcceptEx函数及AcceptExSockAddr函数的地址
	bool GetPtrOfAcceptExAndGetAcceptExSockaddrs();

	// 初始化主Socket，root
	bool InitListenSocket();

	// 将句柄绑定到完成端口中
	bool AssociateIOCP(SOCKET_CONTEXT *socket_context);

	// 投递Accept请求
	bool PostAccept(IO_CONTEXT *io_context);
	//投递receive请求
	bool PostReceive(IO_CONTEXT *io_context);
	//处理accept请求，并投递
	bool GetAccept(SOCKET_CONTEXT *socket_context, IO_CONTEXT *io_context);
	//处理receive请求，并投递
	bool GetReceive(SOCKET_CONTEXT *socket_context, IO_CONTEXT *io_context);

	//清理客户端信息，释放资源
	void ClearClientContext();
	//移除一个客户端
	void RemoveClientContext(SOCKET_CONTEXT * &socket_context);
	//添加一个客户端
	void AddToClientContext(SOCKET_CONTEXT *socket_context);

	//主界面显示消息
	void ShowStatusMessage(std::wstring str);		
	//将本机ip设置到服务器监听地址
	void GetLocalIP();																

	HANDLE h_event_shutdown;														//工人线程退出事件
	HANDLE h_iocp;																	//IOCP 句柄
	HANDLE *h_worker_threads;														//工人线程列表
	int threads_num;																//工人线程数量
	CRITICAL_SECTION cs_thread_synchro;												//互斥量，工人线程同步
	std::vector<SOCKET_CONTEXT *> client_context_vector;							//客户端Socket的Context信息列表      
	SOCKET_CONTEXT * listen_socket_context;											//用于监听的Socket的上下文信息
	LPFN_ACCEPTEX                lpfnAcceptEx;										// AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
	LPFN_GETACCEPTEXSOCKADDRS    lpfnGetAcceptExSockAddrs;

	ServerWnd *main_dlg;															//主窗口用于显示信息
	int listen_port;																//服务器监听端口
	std::string listen_addr;														//服务器监听地址


};
