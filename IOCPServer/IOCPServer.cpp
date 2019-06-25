#include "IOCPServer.h"

//using namespace std;


// 每一个处理器上产生多少个线程(为了最大限度的提升服务器性能，详见配套文档)
#define WORKER_THREADS_PER_PROCESSOR 2
// 同时投递的Accept请求的数量(这个要根据实际的情况灵活设置)
#define MAX_POST_ACCEPT              10
// 传递给Worker线程的退出信号
#define EXIT_CODE                    NULL

//将string转换成wstring  
std::wstring Char2WString(char * str)
{
	std::wstring result;
	//获取缓冲区大小，并申请空间，缓冲区大小按字符计算  
	int len = MultiByteToWideChar(CP_ACP, 0, str, strlen(str), NULL, 0);
	TCHAR* buffer = new TCHAR[len + 1];
	//多字节编码转换成宽字节编码  
	MultiByteToWideChar(CP_ACP, 0, str, strlen(str), buffer, len);
	buffer[len] = '\0';             //添加字符串结尾  
	//删除缓冲区并返回值  
	result.append(buffer);
	delete[] buffer;
	return result;
}


IOCPServer::IOCPServer():	h_event_shutdown(NULL),
							h_iocp(NULL),
							h_worker_threads(NULL),
							threads_num(0), 
							listen_socket_context(NULL),
							lpfnAcceptEx(NULL)  ,      
							lpfnGetAcceptExSockAddrs(NULL),
							main_dlg(NULL),      	
							listen_port(0),
							listen_addr("")
{
	LoadSocketLib();	
}


IOCPServer::~IOCPServer()
{
	UnloadSocketLib();
	StopServer();
}

//工作者线程， 为IOCP请求服务
DWORD WINAPI IOCPServer::WorkerThread(LPVOID lp_param)
{
	WORKER_PARAMS *worker_params = (WORKER_PARAMS *)lp_param;
	IOCPServer *iocp_server = worker_params->p_iocp_server;
	int thread_id = worker_params->worker_id;
	std::wstring str = L"工作者线程启动,ID:" + std::to_wstring(thread_id);
	iocp_server->ShowStatusMessage(str);

	OVERLAPPED           *p_overlapped = NULL;
	SOCKET_CONTEXT		*socket_context = NULL;
	DWORD                dw_bytes_transfered = 0;

	// 循环处理请求，知道接收到Shutdown事件为止
	while (WaitForSingleObject(iocp_server->h_event_shutdown, 0) != WAIT_OBJECT_0)
	{
		//socket_context 将socket绑定至iocp时传入的（CompletionKey）
		bool b_return = GetQueuedCompletionStatus(iocp_server->h_iocp, &dw_bytes_transfered,
			(PULONG_PTR)&socket_context, &p_overlapped, INFINITE);

		// 如果收到的是退出标志，则直接退出
		if ((DWORD)socket_context == EXIT_CODE)
		{
			std::wstring str1 = L"因服务器端关闭，工作者线程退出。ID:" + std::to_wstring(thread_id);
			iocp_server->ShowStatusMessage(str1);
			break;
		}

		// 判断是否出现了错误
		if (!b_return)
		{
			DWORD err = GetLastError();
			if (err == WAIT_TIMEOUT || err == ERROR_NETNAME_DELETED)
			{
				iocp_server->ShowStatusMessage(L"客户端异常或操作超时。");
				continue;
			}
			else
			{
				iocp_server->ShowStatusMessage(L"完成端口操作出现错误，线程退出。");
				break;
			}
		}
		else
		{
			// 读取传入的参数（这里通过发送异步请求的传入的overlapped参数获得）
			IO_CONTEXT *io_context = CONTAINING_RECORD(p_overlapped, IO_CONTEXT, overlapped);

			// 判断是否有客户端断开了
			if (dw_bytes_transfered == 0 & (io_context->op_type == RECV_POSTED || io_context->op_type == SEND_POSTED))
			{
				std::wstring str = L"客户端 " + Char2WString(inet_ntoa(socket_context->client_addr.sin_addr))
					+ L":" + std::to_wstring(ntohs(socket_context->client_addr.sin_port)) + L" 断开连接";
				iocp_server->ShowStatusMessage(str);
				iocp_server->RemoveClientContext(socket_context);
				continue;
			}
			else
			{
				switch (io_context->op_type)
				{
				case ACCEPT_POSTED:
				{
					iocp_server->GetAccept(socket_context, io_context);
					
				}
				break;

				case RECV_POSTED:
				{
					iocp_server->GetReceive(socket_context, io_context);
					
				}
				break;

				
				case SEND_POSTED:
				{

				}
				break;
				default:
					iocp_server->ShowStatusMessage(L"OPERATION_TYPE类型错误！");
					break;
				}
			}
		}
	}


	std::wstring str2 = L"工作者线程退出,ID:" + std::to_wstring(thread_id);
	iocp_server->ShowStatusMessage(str2);
	if (lp_param != NULL)
	{
		delete lp_param;
	}
	return 0;
}


bool IOCPServer::LoadSocketLib()	//初始化WinSock 2.2
{
	WSADATA wsadata;
	int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (result != NO_ERROR)
	{
		ShowStatusMessage(L"初始化WinSock 2.2失败！");
		UnloadSocketLib();
		return false;
	}
	ShowStatusMessage(L"初始化WinSock 2.2成功！");
	return true;
}

void IOCPServer::UnloadSocketLib()	//卸载WinSock 2.2
{
	WSACleanup();
}

void IOCPServer::BingMainWnd(ServerWnd * dlg)
{
	main_dlg = dlg;
}

//启动服务器
bool IOCPServer::StartServer()	
{
	// 初始化线程互斥量（用于工人线程）
	InitializeCriticalSection(&cs_thread_synchro);
	//创建系统推出事件
	h_event_shutdown = CreateEvent(NULL, TRUE, FALSE, NULL);

	//初始化IOCP
	if (InitIOCP() == false)	
	{
		CloseHandle(h_event_shutdown);
		DeleteCriticalSection(&cs_thread_synchro);
		return false;
	}
	else
	{
		ShowStatusMessage(L"初始化IOCP成功！");
	}

	//初始化SOCKET
	if (InitListenSocket() == false)
	{
		
		CloseHandle(h_event_shutdown);
		DeleteCriticalSection(&cs_thread_synchro);

		closesocket(listen_socket_context->socket);
		delete listen_socket_context;
		listen_socket_context = NULL;
		lpfnAcceptEx = NULL;
		lpfnGetAcceptExSockAddrs = NULL;

		return false;
	}
	else
	{
		ShowStatusMessage(L"初始化ListenSocket成功！");
	}
	ShowStatusMessage(L"系统初始化完毕，等待连接。。。。");

	return true;
}

//服务器退出
void IOCPServer::StopServer()
{
	//保证服务器是有效的
	if (listen_socket_context != NULL && listen_socket_context->socket != INVALID_SOCKET)
	{
		//设置退出事件
		SetEvent(h_event_shutdown);
		for (int i = 0; i < threads_num; i++)
		{
			// 通知所有的完成端口操作退出
			PostQueuedCompletionStatus(h_iocp, 0, (DWORD)EXIT_CODE, NULL);
		}

		// 等待所有的客户端资源退出
		WaitForMultipleObjects(threads_num, h_worker_threads, TRUE, INFINITE);

		//清理所有线程信息
		for (int i = 0; i < threads_num; i++)
		{
			if (h_worker_threads[i] != NULL && h_worker_threads[i] != INVALID_HANDLE_VALUE)
			{
				CloseHandle(h_worker_threads[i]);
				h_worker_threads[i] = NULL;
			}
		}
		delete h_worker_threads;

		// 清除客户端列表信息
		ClearClientContext();

		//关闭用于监听的SOCKET 并释放相关资源
		closesocket(listen_socket_context->socket);
		delete listen_socket_context;

		CloseHandle(h_event_shutdown);
		CloseHandle(h_iocp);		

		lpfnAcceptEx = NULL;
		lpfnGetAcceptExSockAddrs = NULL;
		ShowStatusMessage(L"停止监听。");
		DeleteCriticalSection(&cs_thread_synchro);

	}
}


//初始化IOCP 
bool IOCPServer::InitIOCP()
{
	//创建一个完成端口
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (h_iocp == NULL)
	{
		ShowStatusMessage(L"建立完成端口失败！");
		return false;
	}

	// 根据本机中的处理器数量，建立对应的线程数
	SYSTEM_INFO si;
	GetSystemInfo(&si);	
	threads_num = si.dwNumberOfProcessors * WORKER_THREADS_PER_PROCESSOR;

	// 为工作者线程初始化句柄
	h_worker_threads = new HANDLE[threads_num];
	DWORD nThreadID;

	//创建N个工作者线程运行WorkerThread处理程序
	for (int i = 0; i < threads_num; i++)
	{
		WORKER_PARAMS* p_thread_param = new WORKER_PARAMS;
		p_thread_param->p_iocp_server = this;
		p_thread_param->worker_id = i + 1;
		h_worker_threads[i] = CreateThread(0, 0, WorkerThread, (void *)p_thread_param, 0, &nThreadID);
	}
	ShowStatusMessage(L"创建工人线程成功！");
	return true;
}
//初始化AcceptEx函数及AcceptExSockAddr函数的地址
bool IOCPServer::GetPtrOfAcceptExAndGetAcceptExSockaddrs()	//必须在初始化监听Socket后才能调用（参数需要）
{	
	// AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	DWORD dwBytes = 0;

	// 获取GetAcceptEx函数指针，也是同理
	if (SOCKET_ERROR == WSAIoctl(
		listen_socket_context->socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx,
		sizeof(GuidAcceptEx),
		&lpfnAcceptEx,
		sizeof(lpfnAcceptEx),
		&dwBytes,
		NULL,
		NULL))
	{
		ShowStatusMessage(L"WSAIoctl 未能获取AcceptEx函数指针。\n");
		return false;
	}

	// 获取GetAcceptExSockAddrs函数指针，也是同理
	if (SOCKET_ERROR == WSAIoctl(
		listen_socket_context->socket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidGetAcceptExSockAddrs,
		sizeof(GuidGetAcceptExSockAddrs),
		&lpfnGetAcceptExSockAddrs,
		sizeof(lpfnGetAcceptExSockAddrs),
		&dwBytes,
		NULL,
		NULL))
	{
		ShowStatusMessage(L"WSAIoctl 未能获取GuidGetAcceptExSockAddrs函数指针");
		return false;
	}
	return true;
}

// 初始化Socket

bool IOCPServer::InitListenSocket()
{
	// 服务器地址信息，用于绑定Socket
	sockaddr_in server_address;
	ZeroMemory((char *)&server_address, sizeof(server_address));

	
	server_address.sin_family = AF_INET;

	//一台电脑可能右多个网卡，故可能有多个ip地址信息
	// 这里可以绑定任何可用的IP地址，或者绑定一个指定的IP地址 
	//GetLocalIP() 绑定一个ip
	if (listen_addr == "")
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		server_address.sin_addr.s_addr = inet_addr(listen_addr.c_str());

	//服务器的监听端口
	if (listen_port == 0)
		server_address.sin_port = htons(DEFAULT_PORT);
	else
		server_address.sin_port = htons(listen_port);


	//服务器监听socket上下文。（根socket）
	listen_socket_context = new SOCKET_CONTEXT;
	listen_socket_context->socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (listen_socket_context->socket == INVALID_SOCKET)
	{
		ShowStatusMessage(L"初始化Socket失败！");
		//ShowStatusMessage(std::to_wstring(WSAGetLastError()));

		return false;
	}
	else
	{
		ShowStatusMessage(L"初始化Socket成功！");
	}

	// 将Listen Socket绑定至完成端口中
	if (CreateIoCompletionPort((HANDLE)listen_socket_context->socket, h_iocp,
		(DWORD)listen_socket_context, 0) == NULL)
	{
		ShowStatusMessage(L"绑定 Listen Socket至完成端口失败！");

		return false;
	}
	else
	{
		ShowStatusMessage(L"绑定 Listen Socket至完成端口成功！");

	}

	// 绑定地址和端口
	if (bind(listen_socket_context->socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
	{

		ShowStatusMessage(L"bind()函数执行错误.");
		ShowStatusMessage(std::to_wstring(WSAGetLastError()));
		return false;
	}
	else
	{
		ShowStatusMessage(L"bind()函数执行成功.");
	}

	// 开始进行监听

	if (listen(listen_socket_context->socket, SOMAXCONN) == SOCKET_ERROR)
	{

		ShowStatusMessage(L"listen()函数执行错误.");
		return false;
	}
	else
	{
		ShowStatusMessage(L"listen()函数执行成功.");

	}

	//获取AcceptEx函数及AcceptExSockAddr函数的地址
	if (GetPtrOfAcceptExAndGetAcceptExSockaddrs() == false)
	{
		return false;
	}
	else
	{
		ShowStatusMessage(L"获取AcceptEx,GuidGetAcceptExSockAddrs函数指针成功。");
	}

	// 为AcceptEx 准备参数，然后投递n个 AcceptEx I/O请求（异步操作，关联到IOCP）
	for (int i = 0; i < MAX_POST_ACCEPT; i++)
	{
		IO_CONTEXT* p_accept_io_content = listen_socket_context->GetNewIOContext();
		if (PostAccept(p_accept_io_content) == false)
		{
			listen_socket_context->RemoveIOContext(p_accept_io_content);
			return false;
		}
	}
	std::wstring str = L"投递" + std::to_wstring(MAX_POST_ACCEPT) + L"个AcceptEx请求完毕";
	ShowStatusMessage(str);
	return true;
}

//将socket关联到io完成端口
bool IOCPServer::AssociateIOCP(SOCKET_CONTEXT * socket_context)
{
	HANDLE handle = CreateIoCompletionPort((HANDLE)socket_context->socket, h_iocp, (DWORD)socket_context, 0);
	if (handle == NULL)
	{
		ShowStatusMessage(L"执行CreateIoCompletionPort()出现错误.");
		return false;
	}
	return true;
}


bool IOCPServer::PostAccept(IO_CONTEXT *io_context)	//投递acceptEx操作
{
	if (listen_socket_context->socket == INVALID_SOCKET)
	{
		return false;
	}
	DWORD bytes = 0;
	io_context->op_type = ACCEPT_POSTED;
	io_context->ResetBuffer();

	// 为以后新连入的客户端先准备好socket( 这个是与传统accept最大的区别 ) 
	io_context->socket_accept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (io_context->socket_accept == INVALID_SOCKET)
	{
		ShowStatusMessage(L"创建用于Accept的Socket失败！");
		return false;
	}

	// 投递AcceptEx
	if (lpfnAcceptEx(listen_socket_context->socket, io_context->socket_accept, io_context->wsabuf.buf,
		io_context->wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, &(io_context->overlapped)) == FALSE)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			ShowStatusMessage(L"投递 AcceptEx 请求失败。");
			return false;
		}
	}
	return true;
}

// 投递接收数据的请求
bool IOCPServer::PostReceive(IO_CONTEXT * io_context)
{
	DWORD flags = 0;
	DWORD bytes = 0;
	WSABUF *wsa_buf = &io_context->wsabuf;
	OVERLAPPED *p_overlapped = &io_context->overlapped;

	io_context->ResetBuffer();
	io_context->op_type = RECV_POSTED;

	// 初始化完成后，投递WSARecv请求
	int bytes_recv = WSARecv(io_context->socket_accept, &(io_context->wsabuf), 1, &bytes,
		&flags,&(io_context->overlapped), NULL);

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if ((bytes_recv == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING))
	{
		ShowStatusMessage(L"投递PostReceive()失败！");
		return false;
	}

	return true;
}

// 在有客户端连入的时候，进行处理 
bool IOCPServer::GetAccept(SOCKET_CONTEXT * socket_context, IO_CONTEXT * io_context)
{
	SOCKADDR_IN *client_addr = NULL;
	SOCKADDR_IN *local_addr = NULL;
	int client_len = sizeof(SOCKADDR), local_len = sizeof(SOCKADDR);

	// 首先取得连入客户端的地址信息，顺便取出客户端发来的第一组数据
	lpfnGetAcceptExSockAddrs(io_context->wsabuf.buf, io_context->wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR *)&local_addr, &local_len,
		(LPSOCKADDR *)&client_addr, &client_len);
	
	std::wstring msg = L"客户端" + Char2WString(inet_ntoa(client_addr->sin_addr)) + L":" +
		std::to_wstring(ntohs(client_addr->sin_port))+L"连入。";
	ShowStatusMessage(msg);

	//为新连入的Socket新建一个socket_context, 之前的socket_content还要继续监听下一个接入的客户端
	//io_context->socket_accept是这个客户端的socket，投递socket_content时要创建一个新的socket
	SOCKET_CONTEXT *new_socket_context = new SOCKET_CONTEXT;
	new_socket_context->socket = io_context->socket_accept;
	memcpy(&new_socket_context->client_addr, client_addr, sizeof(SOCKADDR_IN));

	// 参数设置完毕，将这个Socket和完成端口绑定(这也是一个关键步骤) 这样这个socket的io操作就可以通过iocp进行服务了
	if (AssociateIOCP(new_socket_context) == false)
	{
		delete new_socket_context;
		return false;
	}

	//建立一个io_content
	//一是用来处理之前取出的客户端发来的第一组数据
	//二是在这个客户端socket上投递receive
	IO_CONTEXT *new_io_context = new_socket_context->GetNewIOContext();
	new_io_context->socket_accept = new_socket_context->socket;
	memcpy(new_io_context->wsabuf.buf, io_context->wsabuf.buf, io_context->wsabuf.len);
	new_io_context->wsabuf.len = io_context->wsabuf.len;

	if (GetReceive(new_socket_context, new_io_context) == false)
	{
		new_socket_context->RemoveIOContext(new_io_context);
		return false;
	}

	//新连入的Socket的socket_context,统一管理，方便释放资源
	AddToClientContext(new_socket_context);

	//最后准备投递新的AcceptEx（这样才能继续接收客户端的接入请求）
	return PostAccept(io_context);
}

// 在有接收的数据到达的时候，进行处理
bool IOCPServer::GetReceive(SOCKET_CONTEXT * socket_context, IO_CONTEXT * io_context)
{
	std::wstring str = L"客户端收到" + Char2WString(inet_ntoa(socket_context->client_addr.sin_addr)) + L":" +
		std::to_wstring(ntohs(socket_context->client_addr.sin_port)) + L"信息："+ Char2WString(io_context->buffer);
	ShowStatusMessage(str);

	return PostReceive(io_context);
}

// 清空客户端信息
void IOCPServer::ClearClientContext()
{
	EnterCriticalSection(&cs_thread_synchro);
	for (auto &var : client_context_vector)
	{
		delete var;
		var = NULL;
	}
	client_context_vector.clear();
	std::vector<SOCKET_CONTEXT*>().swap(client_context_vector);
	LeaveCriticalSection(&cs_thread_synchro);
}

//移除某个特定的客户端信息，一般来说客户端断开连接时使用
void IOCPServer::RemoveClientContext( SOCKET_CONTEXT* &socket_context)
{
	EnterCriticalSection(&cs_thread_synchro);
	for (auto var=client_context_vector.begin();var<client_context_vector.end();var++)
	{
		if (socket_context == *var)
		{
			client_context_vector.erase(var);
			break;
		}
	}
	delete socket_context;
	socket_context = NULL;
	LeaveCriticalSection(&cs_thread_synchro);
}

//将客户端信息添加到到向量中
void IOCPServer::AddToClientContext(SOCKET_CONTEXT * socket_context)
{
	EnterCriticalSection(&cs_thread_synchro);

	client_context_vector.push_back(socket_context);

	LeaveCriticalSection(&cs_thread_synchro);
}

//用来显示信息
void IOCPServer::ShowStatusMessage(std::wstring str)	
{

	EnterCriticalSection(&cs_thread_synchro);
	if (main_dlg != NULL)
	{
		main_dlg->ShowStatusMessage(str);
	}
	LeaveCriticalSection(&cs_thread_synchro);
}

// 获得本机的IP地址
void IOCPServer::GetLocalIP()
{
	// 获得本机主机名
	char host_name[MAX_PATH] = { 0 };
	gethostname(host_name, MAX_PATH);
	struct hostent FAR* lp_hostent = gethostbyname(host_name);
	if (lp_hostent != NULL)
	{
		// 取得IP地址列表中的第一个为返回的IP(因为一台主机可能会绑定多个IP)
		LPSTR lp_addr = lp_hostent->h_addr_list[0];

		// 将IP地址转化成字符串形式
		struct in_addr in_addr;
		memmove(&in_addr, lp_addr, 4);
		listen_addr = inet_ntoa(in_addr);
		
	}
}