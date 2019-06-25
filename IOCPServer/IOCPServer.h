#pragma once

// winsock 2 ��ͷ�ļ��Ϳ�
#include <winsock2.h>
#include <MSWSock.h>
#pragma comment(lib,"ws2_32.lib")

#include <Windows.h>	//�������winsock�ĺ��棬�г�ͻ
#include "ServerWnd.h"

// ���������� (1024*8)
#define MAX_BUFFER_LEN 8192
#define DEFAULT_PORT 12345


// ����ɶ˿���Ͷ�ݵ�I/O����������
typedef enum _OPERATION_TYPE
{
	ACCEPT_POSTED,                     // ��־Ͷ�ݵ�Accept����
	SEND_POSTED,                       // ��־Ͷ�ݵ��Ƿ��Ͳ���
	RECV_POSTED,                       // ��־Ͷ�ݵ��ǽ��ղ���
	NULL_POSTED                        // ���ڳ�ʼ����������

}OPERATION_TYPE;

//ÿ��io���������ṹ�嶨��
typedef struct _IO_CONTEXT {
	OVERLAPPED overlapped;				 // ÿһ���ص�����������ص��ṹ(���ÿһ��Socket��ÿһ����������Ҫ��һ��)
	SOCKET socket_accept;				 // ������������ʹ�õ�Socket
	WSABUF wsabuf;						 // WSA���͵Ļ����������ڸ��ص�������������
	char buffer[MAX_BUFFER_LEN];		 // �����WSABUF�������ַ��Ļ�����
	OPERATION_TYPE	op_type;			 // ��ʶ�������������(��Ӧ�����ö��)

	// ��ʼ��
	_IO_CONTEXT()
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		socket_accept = INVALID_SOCKET;
		ZeroMemory(buffer, MAX_BUFFER_LEN);
		wsabuf.buf = buffer;
		wsabuf.len = MAX_BUFFER_LEN;
		op_type = NULL_POSTED;
	}

	// �ͷ�
	~_IO_CONTEXT()
	{
		if (socket_accept != INVALID_SOCKET)
		{
			closesocket(socket_accept);
			socket_accept = INVALID_SOCKET;
		}
	}

	// ���û���������
	void ResetBuffer()
	{
		ZeroMemory(buffer, MAX_BUFFER_LEN);
	}
}IO_CONTEXT;


//ÿ����ɶ˿ڽṹ�嶨�壨ÿ���ͻ�����Ϣ��
typedef struct _SOCKET_CONTEXT {
	SOCKET socket;											// ÿһ���ͻ������ӵ�Socket
	SOCKADDR_IN client_addr;								// �ͻ��˵ĵ�ַ
	std::vector<IO_CONTEXT *> p_io_context_vector;			// �ͻ���������������������ݣ�
															// Ҳ����˵����ÿһ���ͻ���Socket���ǿ���������ͬʱͶ�ݶ��IO�����
	// ��ʼ��
	_SOCKET_CONTEXT()
	{
		socket = INVALID_SOCKET;
		ZeroMemory(&client_addr, sizeof(client_addr));
	}
	// �ͷ���Դ
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

	// ��ȡһ���µ�IoContext
	IO_CONTEXT *GetNewIOContext()
	{
		IO_CONTEXT *new_io = new IO_CONTEXT;
		p_io_context_vector.push_back(new_io);
		return new_io;
	}

	// ���������Ƴ�һ��ָ����IoContext
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

// �����̵߳��̲߳���
typedef struct _WORKER_PARAMS
{
	IOCPServer *p_iocp_server;		// ��ָ�룬���ڵ������еĺ���
	int worker_id;					// �̱߳�ţ�ûʲôʵ���Ե����ã���������)

} WORKER_PARAMS;



class IOCPServer
{
public:
	IOCPServer() ;
	~IOCPServer();
	//�������ڣ�������ʾ��Ϣ
	void BingMainWnd(ServerWnd *dlg);

	//����������
	bool StartServer();

	//ֹͣ������
	void StopServer();

private:
	//�����̣߳�Ϊiocp������
	static DWORD WINAPI WorkerThread(LPVOID lpParam);

	//����socket��
	bool LoadSocketLib();	

	//ж��socker��
	void UnloadSocketLib();														

	//��ʼ��iocp
	bool InitIOCP();							

	//��ʼ��AcceptEx������AcceptExSockAddr�����ĵ�ַ
	bool GetPtrOfAcceptExAndGetAcceptExSockaddrs();

	// ��ʼ����Socket��root
	bool InitListenSocket();

	// ������󶨵���ɶ˿���
	bool AssociateIOCP(SOCKET_CONTEXT *socket_context);

	// Ͷ��Accept����
	bool PostAccept(IO_CONTEXT *io_context);
	//Ͷ��receive����
	bool PostReceive(IO_CONTEXT *io_context);
	//����accept���󣬲�Ͷ��
	bool GetAccept(SOCKET_CONTEXT *socket_context, IO_CONTEXT *io_context);
	//����receive���󣬲�Ͷ��
	bool GetReceive(SOCKET_CONTEXT *socket_context, IO_CONTEXT *io_context);

	//����ͻ�����Ϣ���ͷ���Դ
	void ClearClientContext();
	//�Ƴ�һ���ͻ���
	void RemoveClientContext(SOCKET_CONTEXT * &socket_context);
	//���һ���ͻ���
	void AddToClientContext(SOCKET_CONTEXT *socket_context);

	//��������ʾ��Ϣ
	void ShowStatusMessage(std::wstring str);		
	//������ip���õ�������������ַ
	void GetLocalIP();																

	HANDLE h_event_shutdown;														//�����߳��˳��¼�
	HANDLE h_iocp;																	//IOCP ���
	HANDLE *h_worker_threads;														//�����߳��б�
	int threads_num;																//�����߳�����
	CRITICAL_SECTION cs_thread_synchro;												//�������������߳�ͬ��
	std::vector<SOCKET_CONTEXT *> client_context_vector;							//�ͻ���Socket��Context��Ϣ�б�      
	SOCKET_CONTEXT * listen_socket_context;											//���ڼ�����Socket����������Ϣ
	LPFN_ACCEPTEX                lpfnAcceptEx;										// AcceptEx �� GetAcceptExSockaddrs �ĺ���ָ�룬���ڵ�����������չ����
	LPFN_GETACCEPTEXSOCKADDRS    lpfnGetAcceptExSockAddrs;

	ServerWnd *main_dlg;															//������������ʾ��Ϣ
	int listen_port;																//�����������˿�
	std::string listen_addr;														//������������ַ


};
