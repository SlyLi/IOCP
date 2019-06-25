#include "IOCPServer.h"

//using namespace std;


// ÿһ���������ϲ������ٸ��߳�(Ϊ������޶ȵ��������������ܣ���������ĵ�)
#define WORKER_THREADS_PER_PROCESSOR 2
// ͬʱͶ�ݵ�Accept���������(���Ҫ����ʵ�ʵ�����������)
#define MAX_POST_ACCEPT              10
// ���ݸ�Worker�̵߳��˳��ź�
#define EXIT_CODE                    NULL

//��stringת����wstring  
std::wstring Char2WString(char * str)
{
	std::wstring result;
	//��ȡ��������С��������ռ䣬��������С���ַ�����  
	int len = MultiByteToWideChar(CP_ACP, 0, str, strlen(str), NULL, 0);
	TCHAR* buffer = new TCHAR[len + 1];
	//���ֽڱ���ת���ɿ��ֽڱ���  
	MultiByteToWideChar(CP_ACP, 0, str, strlen(str), buffer, len);
	buffer[len] = '\0';             //����ַ�����β  
	//ɾ��������������ֵ  
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

//�������̣߳� ΪIOCP�������
DWORD WINAPI IOCPServer::WorkerThread(LPVOID lp_param)
{
	WORKER_PARAMS *worker_params = (WORKER_PARAMS *)lp_param;
	IOCPServer *iocp_server = worker_params->p_iocp_server;
	int thread_id = worker_params->worker_id;
	std::wstring str = L"�������߳�����,ID:" + std::to_wstring(thread_id);
	iocp_server->ShowStatusMessage(str);

	OVERLAPPED           *p_overlapped = NULL;
	SOCKET_CONTEXT		*socket_context = NULL;
	DWORD                dw_bytes_transfered = 0;

	// ѭ����������֪�����յ�Shutdown�¼�Ϊֹ
	while (WaitForSingleObject(iocp_server->h_event_shutdown, 0) != WAIT_OBJECT_0)
	{
		//socket_context ��socket����iocpʱ����ģ�CompletionKey��
		bool b_return = GetQueuedCompletionStatus(iocp_server->h_iocp, &dw_bytes_transfered,
			(PULONG_PTR)&socket_context, &p_overlapped, INFINITE);

		// ����յ������˳���־����ֱ���˳�
		if ((DWORD)socket_context == EXIT_CODE)
		{
			std::wstring str1 = L"��������˹رգ��������߳��˳���ID:" + std::to_wstring(thread_id);
			iocp_server->ShowStatusMessage(str1);
			break;
		}

		// �ж��Ƿ�����˴���
		if (!b_return)
		{
			DWORD err = GetLastError();
			if (err == WAIT_TIMEOUT || err == ERROR_NETNAME_DELETED)
			{
				iocp_server->ShowStatusMessage(L"�ͻ����쳣�������ʱ��");
				continue;
			}
			else
			{
				iocp_server->ShowStatusMessage(L"��ɶ˿ڲ������ִ����߳��˳���");
				break;
			}
		}
		else
		{
			// ��ȡ����Ĳ���������ͨ�������첽����Ĵ����overlapped������ã�
			IO_CONTEXT *io_context = CONTAINING_RECORD(p_overlapped, IO_CONTEXT, overlapped);

			// �ж��Ƿ��пͻ��˶Ͽ���
			if (dw_bytes_transfered == 0 & (io_context->op_type == RECV_POSTED || io_context->op_type == SEND_POSTED))
			{
				std::wstring str = L"�ͻ��� " + Char2WString(inet_ntoa(socket_context->client_addr.sin_addr))
					+ L":" + std::to_wstring(ntohs(socket_context->client_addr.sin_port)) + L" �Ͽ�����";
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
					iocp_server->ShowStatusMessage(L"OPERATION_TYPE���ʹ���");
					break;
				}
			}
		}
	}


	std::wstring str2 = L"�������߳��˳�,ID:" + std::to_wstring(thread_id);
	iocp_server->ShowStatusMessage(str2);
	if (lp_param != NULL)
	{
		delete lp_param;
	}
	return 0;
}


bool IOCPServer::LoadSocketLib()	//��ʼ��WinSock 2.2
{
	WSADATA wsadata;
	int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (result != NO_ERROR)
	{
		ShowStatusMessage(L"��ʼ��WinSock 2.2ʧ�ܣ�");
		UnloadSocketLib();
		return false;
	}
	ShowStatusMessage(L"��ʼ��WinSock 2.2�ɹ���");
	return true;
}

void IOCPServer::UnloadSocketLib()	//ж��WinSock 2.2
{
	WSACleanup();
}

void IOCPServer::BingMainWnd(ServerWnd * dlg)
{
	main_dlg = dlg;
}

//����������
bool IOCPServer::StartServer()	
{
	// ��ʼ���̻߳����������ڹ����̣߳�
	InitializeCriticalSection(&cs_thread_synchro);
	//����ϵͳ�Ƴ��¼�
	h_event_shutdown = CreateEvent(NULL, TRUE, FALSE, NULL);

	//��ʼ��IOCP
	if (InitIOCP() == false)	
	{
		CloseHandle(h_event_shutdown);
		DeleteCriticalSection(&cs_thread_synchro);
		return false;
	}
	else
	{
		ShowStatusMessage(L"��ʼ��IOCP�ɹ���");
	}

	//��ʼ��SOCKET
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
		ShowStatusMessage(L"��ʼ��ListenSocket�ɹ���");
	}
	ShowStatusMessage(L"ϵͳ��ʼ����ϣ��ȴ����ӡ�������");

	return true;
}

//�������˳�
void IOCPServer::StopServer()
{
	//��֤����������Ч��
	if (listen_socket_context != NULL && listen_socket_context->socket != INVALID_SOCKET)
	{
		//�����˳��¼�
		SetEvent(h_event_shutdown);
		for (int i = 0; i < threads_num; i++)
		{
			// ֪ͨ���е���ɶ˿ڲ����˳�
			PostQueuedCompletionStatus(h_iocp, 0, (DWORD)EXIT_CODE, NULL);
		}

		// �ȴ����еĿͻ�����Դ�˳�
		WaitForMultipleObjects(threads_num, h_worker_threads, TRUE, INFINITE);

		//���������߳���Ϣ
		for (int i = 0; i < threads_num; i++)
		{
			if (h_worker_threads[i] != NULL && h_worker_threads[i] != INVALID_HANDLE_VALUE)
			{
				CloseHandle(h_worker_threads[i]);
				h_worker_threads[i] = NULL;
			}
		}
		delete h_worker_threads;

		// ����ͻ����б���Ϣ
		ClearClientContext();

		//�ر����ڼ�����SOCKET ���ͷ������Դ
		closesocket(listen_socket_context->socket);
		delete listen_socket_context;

		CloseHandle(h_event_shutdown);
		CloseHandle(h_iocp);		

		lpfnAcceptEx = NULL;
		lpfnGetAcceptExSockAddrs = NULL;
		ShowStatusMessage(L"ֹͣ������");
		DeleteCriticalSection(&cs_thread_synchro);

	}
}


//��ʼ��IOCP 
bool IOCPServer::InitIOCP()
{
	//����һ����ɶ˿�
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (h_iocp == NULL)
	{
		ShowStatusMessage(L"������ɶ˿�ʧ�ܣ�");
		return false;
	}

	// ���ݱ����еĴ�����������������Ӧ���߳���
	SYSTEM_INFO si;
	GetSystemInfo(&si);	
	threads_num = si.dwNumberOfProcessors * WORKER_THREADS_PER_PROCESSOR;

	// Ϊ�������̳߳�ʼ�����
	h_worker_threads = new HANDLE[threads_num];
	DWORD nThreadID;

	//����N���������߳�����WorkerThread�������
	for (int i = 0; i < threads_num; i++)
	{
		WORKER_PARAMS* p_thread_param = new WORKER_PARAMS;
		p_thread_param->p_iocp_server = this;
		p_thread_param->worker_id = i + 1;
		h_worker_threads[i] = CreateThread(0, 0, WorkerThread, (void *)p_thread_param, 0, &nThreadID);
	}
	ShowStatusMessage(L"���������̳߳ɹ���");
	return true;
}
//��ʼ��AcceptEx������AcceptExSockAddr�����ĵ�ַ
bool IOCPServer::GetPtrOfAcceptExAndGetAcceptExSockaddrs()	//�����ڳ�ʼ������Socket����ܵ��ã�������Ҫ��
{	
	// AcceptEx �� GetAcceptExSockaddrs ��GUID�����ڵ�������ָ��
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	DWORD dwBytes = 0;

	// ��ȡGetAcceptEx����ָ�룬Ҳ��ͬ��
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
		ShowStatusMessage(L"WSAIoctl δ�ܻ�ȡAcceptEx����ָ�롣\n");
		return false;
	}

	// ��ȡGetAcceptExSockAddrs����ָ�룬Ҳ��ͬ��
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
		ShowStatusMessage(L"WSAIoctl δ�ܻ�ȡGuidGetAcceptExSockAddrs����ָ��");
		return false;
	}
	return true;
}

// ��ʼ��Socket

bool IOCPServer::InitListenSocket()
{
	// ��������ַ��Ϣ�����ڰ�Socket
	sockaddr_in server_address;
	ZeroMemory((char *)&server_address, sizeof(server_address));

	
	server_address.sin_family = AF_INET;

	//һ̨���Կ����Ҷ���������ʿ����ж��ip��ַ��Ϣ
	// ������԰��κο��õ�IP��ַ�����߰�һ��ָ����IP��ַ 
	//GetLocalIP() ��һ��ip
	if (listen_addr == "")
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		server_address.sin_addr.s_addr = inet_addr(listen_addr.c_str());

	//�������ļ����˿�
	if (listen_port == 0)
		server_address.sin_port = htons(DEFAULT_PORT);
	else
		server_address.sin_port = htons(listen_port);


	//����������socket�����ġ�����socket��
	listen_socket_context = new SOCKET_CONTEXT;
	listen_socket_context->socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (listen_socket_context->socket == INVALID_SOCKET)
	{
		ShowStatusMessage(L"��ʼ��Socketʧ�ܣ�");
		//ShowStatusMessage(std::to_wstring(WSAGetLastError()));

		return false;
	}
	else
	{
		ShowStatusMessage(L"��ʼ��Socket�ɹ���");
	}

	// ��Listen Socket������ɶ˿���
	if (CreateIoCompletionPort((HANDLE)listen_socket_context->socket, h_iocp,
		(DWORD)listen_socket_context, 0) == NULL)
	{
		ShowStatusMessage(L"�� Listen Socket����ɶ˿�ʧ�ܣ�");

		return false;
	}
	else
	{
		ShowStatusMessage(L"�� Listen Socket����ɶ˿ڳɹ���");

	}

	// �󶨵�ַ�Ͷ˿�
	if (bind(listen_socket_context->socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
	{

		ShowStatusMessage(L"bind()����ִ�д���.");
		ShowStatusMessage(std::to_wstring(WSAGetLastError()));
		return false;
	}
	else
	{
		ShowStatusMessage(L"bind()����ִ�гɹ�.");
	}

	// ��ʼ���м���

	if (listen(listen_socket_context->socket, SOMAXCONN) == SOCKET_ERROR)
	{

		ShowStatusMessage(L"listen()����ִ�д���.");
		return false;
	}
	else
	{
		ShowStatusMessage(L"listen()����ִ�гɹ�.");

	}

	//��ȡAcceptEx������AcceptExSockAddr�����ĵ�ַ
	if (GetPtrOfAcceptExAndGetAcceptExSockaddrs() == false)
	{
		return false;
	}
	else
	{
		ShowStatusMessage(L"��ȡAcceptEx,GuidGetAcceptExSockAddrs����ָ��ɹ���");
	}

	// ΪAcceptEx ׼��������Ȼ��Ͷ��n�� AcceptEx I/O�����첽������������IOCP��
	for (int i = 0; i < MAX_POST_ACCEPT; i++)
	{
		IO_CONTEXT* p_accept_io_content = listen_socket_context->GetNewIOContext();
		if (PostAccept(p_accept_io_content) == false)
		{
			listen_socket_context->RemoveIOContext(p_accept_io_content);
			return false;
		}
	}
	std::wstring str = L"Ͷ��" + std::to_wstring(MAX_POST_ACCEPT) + L"��AcceptEx�������";
	ShowStatusMessage(str);
	return true;
}

//��socket������io��ɶ˿�
bool IOCPServer::AssociateIOCP(SOCKET_CONTEXT * socket_context)
{
	HANDLE handle = CreateIoCompletionPort((HANDLE)socket_context->socket, h_iocp, (DWORD)socket_context, 0);
	if (handle == NULL)
	{
		ShowStatusMessage(L"ִ��CreateIoCompletionPort()���ִ���.");
		return false;
	}
	return true;
}


bool IOCPServer::PostAccept(IO_CONTEXT *io_context)	//Ͷ��acceptEx����
{
	if (listen_socket_context->socket == INVALID_SOCKET)
	{
		return false;
	}
	DWORD bytes = 0;
	io_context->op_type = ACCEPT_POSTED;
	io_context->ResetBuffer();

	// Ϊ�Ժ�������Ŀͻ�����׼����socket( ������봫ͳaccept�������� ) 
	io_context->socket_accept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (io_context->socket_accept == INVALID_SOCKET)
	{
		ShowStatusMessage(L"��������Accept��Socketʧ�ܣ�");
		return false;
	}

	// Ͷ��AcceptEx
	if (lpfnAcceptEx(listen_socket_context->socket, io_context->socket_accept, io_context->wsabuf.buf,
		io_context->wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, &(io_context->overlapped)) == FALSE)
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			ShowStatusMessage(L"Ͷ�� AcceptEx ����ʧ�ܡ�");
			return false;
		}
	}
	return true;
}

// Ͷ�ݽ������ݵ�����
bool IOCPServer::PostReceive(IO_CONTEXT * io_context)
{
	DWORD flags = 0;
	DWORD bytes = 0;
	WSABUF *wsa_buf = &io_context->wsabuf;
	OVERLAPPED *p_overlapped = &io_context->overlapped;

	io_context->ResetBuffer();
	io_context->op_type = RECV_POSTED;

	// ��ʼ����ɺ�Ͷ��WSARecv����
	int bytes_recv = WSARecv(io_context->socket_accept, &(io_context->wsabuf), 1, &bytes,
		&flags,&(io_context->overlapped), NULL);

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if ((bytes_recv == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING))
	{
		ShowStatusMessage(L"Ͷ��PostReceive()ʧ�ܣ�");
		return false;
	}

	return true;
}

// ���пͻ��������ʱ�򣬽��д��� 
bool IOCPServer::GetAccept(SOCKET_CONTEXT * socket_context, IO_CONTEXT * io_context)
{
	SOCKADDR_IN *client_addr = NULL;
	SOCKADDR_IN *local_addr = NULL;
	int client_len = sizeof(SOCKADDR), local_len = sizeof(SOCKADDR);

	// ����ȡ������ͻ��˵ĵ�ַ��Ϣ��˳��ȡ���ͻ��˷����ĵ�һ������
	lpfnGetAcceptExSockAddrs(io_context->wsabuf.buf, io_context->wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR *)&local_addr, &local_len,
		(LPSOCKADDR *)&client_addr, &client_len);
	
	std::wstring msg = L"�ͻ���" + Char2WString(inet_ntoa(client_addr->sin_addr)) + L":" +
		std::to_wstring(ntohs(client_addr->sin_port))+L"���롣";
	ShowStatusMessage(msg);

	//Ϊ�������Socket�½�һ��socket_context, ֮ǰ��socket_content��Ҫ����������һ������Ŀͻ���
	//io_context->socket_accept������ͻ��˵�socket��Ͷ��socket_contentʱҪ����һ���µ�socket
	SOCKET_CONTEXT *new_socket_context = new SOCKET_CONTEXT;
	new_socket_context->socket = io_context->socket_accept;
	memcpy(&new_socket_context->client_addr, client_addr, sizeof(SOCKADDR_IN));

	// ����������ϣ������Socket����ɶ˿ڰ�(��Ҳ��һ���ؼ�����) �������socket��io�����Ϳ���ͨ��iocp���з�����
	if (AssociateIOCP(new_socket_context) == false)
	{
		delete new_socket_context;
		return false;
	}

	//����һ��io_content
	//һ����������֮ǰȡ���Ŀͻ��˷����ĵ�һ������
	//����������ͻ���socket��Ͷ��receive
	IO_CONTEXT *new_io_context = new_socket_context->GetNewIOContext();
	new_io_context->socket_accept = new_socket_context->socket;
	memcpy(new_io_context->wsabuf.buf, io_context->wsabuf.buf, io_context->wsabuf.len);
	new_io_context->wsabuf.len = io_context->wsabuf.len;

	if (GetReceive(new_socket_context, new_io_context) == false)
	{
		new_socket_context->RemoveIOContext(new_io_context);
		return false;
	}

	//�������Socket��socket_context,ͳһ���������ͷ���Դ
	AddToClientContext(new_socket_context);

	//���׼��Ͷ���µ�AcceptEx���������ܼ������տͻ��˵Ľ�������
	return PostAccept(io_context);
}

// ���н��յ����ݵ����ʱ�򣬽��д���
bool IOCPServer::GetReceive(SOCKET_CONTEXT * socket_context, IO_CONTEXT * io_context)
{
	std::wstring str = L"�ͻ����յ�" + Char2WString(inet_ntoa(socket_context->client_addr.sin_addr)) + L":" +
		std::to_wstring(ntohs(socket_context->client_addr.sin_port)) + L"��Ϣ��"+ Char2WString(io_context->buffer);
	ShowStatusMessage(str);

	return PostReceive(io_context);
}

// ��տͻ�����Ϣ
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

//�Ƴ�ĳ���ض��Ŀͻ�����Ϣ��һ����˵�ͻ��˶Ͽ�����ʱʹ��
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

//���ͻ�����Ϣ��ӵ���������
void IOCPServer::AddToClientContext(SOCKET_CONTEXT * socket_context)
{
	EnterCriticalSection(&cs_thread_synchro);

	client_context_vector.push_back(socket_context);

	LeaveCriticalSection(&cs_thread_synchro);
}

//������ʾ��Ϣ
void IOCPServer::ShowStatusMessage(std::wstring str)	
{

	EnterCriticalSection(&cs_thread_synchro);
	if (main_dlg != NULL)
	{
		main_dlg->ShowStatusMessage(str);
	}
	LeaveCriticalSection(&cs_thread_synchro);
}

// ��ñ�����IP��ַ
void IOCPServer::GetLocalIP()
{
	// ��ñ���������
	char host_name[MAX_PATH] = { 0 };
	gethostname(host_name, MAX_PATH);
	struct hostent FAR* lp_hostent = gethostbyname(host_name);
	if (lp_hostent != NULL)
	{
		// ȡ��IP��ַ�б��еĵ�һ��Ϊ���ص�IP(��Ϊһ̨�������ܻ�󶨶��IP)
		LPSTR lp_addr = lp_hostent->h_addr_list[0];

		// ��IP��ַת�����ַ�����ʽ
		struct in_addr in_addr;
		memmove(&in_addr, lp_addr, 4);
		listen_addr = inet_ntoa(in_addr);
		
	}
}