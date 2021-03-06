/*
 * Copyright (c) 2008-2014 by Gerasimenko Roman.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistribution of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistribution in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 * 3. The name "JFX" must not be used to endorse or promote
 *     products derived from this software without prior written
 *     permission.
 *     For written permission, please contact roman.gerasimenko@gmail.com
 *
 * 4. Products derived from this software may not be called "JFX",
 *     nor may "JFX" appear in their name, without prior written
 *     permission of Gerasimenko Roman.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE JFX CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
// mt45if.cpp : Defines the exported functions for the DLL application.
// 定义了DLL的导出功能
//

#include "stdafx.h"
#include "box_id.h"
#include <exception>
#include <string.h>
#include <mutex>

//#include <Winternl.h>
//#include <Ntstatus.h>
using std::exception;

#pragma comment(lib, "advapi32.lib")

#ifdef USE_MT4_THREADS  //使用MT4的线程

extern HWND get_window_handle_by_PID(int pid); //通过PID来获取windows窗体的句柄
//找到当前的线程或者进程的PID
DWORD GetWindowThreadID()
{
	//GetCurrentProcessId是获取当前进程一个唯一的标识符。
	HWND hWnd = get_window_handle_by_PID(GetCurrentProcessId());
	//GetWindowThreadProcessId 找出某个窗口的创建者（线程或进程），返回创建者的标志符。
	return GetWindowThreadProcessId(hWnd, nullptr);
}

int abc = 0;

/*
__imp__OpenProcessToken@12
__imp__GetTokenInformation@20
__imp__EqualSid@8
__imp__AllocateAndInitializeSid@44
__imp__FreeSid@4
*/

#ifdef _WIN32  //如果是win32的话执行下面的函数
/*++

Routine Description:

    This routine returns if the service specified is running interactively
	这个例程返回如果指定的服务交互，不调用控制器
    (not invoked \by the service controller).

Arguments:

    None

Return Value:

    BOOL - TRUE if the service is an EXE.如果是exe就返回true


Note:

--*/

BOOL WINAPI IsDBGServiceAnExe(VOID)
{
	HANDLE hProcessToken = nullptr;
	DWORD groupLength = 50;

	PTOKEN_GROUPS groupInfo = static_cast<PTOKEN_GROUPS>(LocalAlloc(0,
	                                                                groupLength));

	SID_IDENTIFIER_AUTHORITY siaNt = SECURITY_NT_AUTHORITY;
	PSID InteractiveSid = nullptr;
	PSID ServiceSid = nullptr;
	DWORD i;

	// Start with assumption that process is an EXE, not a Service.
	BOOL fExe = TRUE;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hProcessToken))
		goto ret;

	if (groupInfo == nullptr)
		goto ret;

	if (!GetTokenInformation(hProcessToken, TokenGroups, groupInfo,
	                         groupLength, &groupLength))
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			goto ret;

		LocalFree(groupInfo);
		groupInfo = nullptr;

		groupInfo = static_cast<PTOKEN_GROUPS>(LocalAlloc(0, groupLength));

		if (groupInfo == nullptr)
			goto ret;

		if (!GetTokenInformation(hProcessToken, TokenGroups, groupInfo,
		                         groupLength, &groupLength))
		{
			goto ret;
		}
	}

	//
	//  We now know the groups associated with this token.  We want to look to see if
	//  the interactive group is active in the token, and if so, we know that
	//  this is an interactive process.
	//
	//  We also look for the "service" SID, and if it's present, we know we're a service.
	//
	//  The service SID will be present iff the service is running in a
	//  user account (and was invoked by the service controller).
	//

	if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_INTERACTIVE_RID, 0, 0,
	                              0, 0, 0, 0, 0, &InteractiveSid))
	{
		goto ret;
	}

	if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_SERVICE_RID, 0, 0, 0,
	                              0, 0, 0, 0, &ServiceSid))
	{
		goto ret;
	}

	for (i = 0; i < groupInfo->GroupCount; i += 1)
	{
		SID_AND_ATTRIBUTES sanda = groupInfo->Groups[i];
		PSID Sid = sanda.Sid;

		//
		//  Check to see if the group we're looking at is one of
		//  the 2 groups we're interested in.
		//
		if (EqualSid(Sid, InteractiveSid))
		{
			//
			//  This process has the Interactive SID in its
			//  token.  This means that the process is running as
			//  an EXE.
			//
			goto ret;
		}
		else if (EqualSid(Sid, ServiceSid))
		{
			//
			//  This process has the Service SID in its
			//  token.  This means that the process is running as
			//  a service running in a user account.
			//
			fExe = FALSE;
			goto ret;
		}
	}

	//
	//  Neither Interactive or Service was present in the current users token,
	//  This implies that the process is running as a service, most likely
	//  running as LocalSystem.
	//
	fExe = FALSE;

ret:

	if (InteractiveSid)
		FreeSid(InteractiveSid);

	if (ServiceSid)
		FreeSid(ServiceSid);

	if (groupInfo)
		LocalFree(groupInfo);

	if (hProcessToken)
		CloseHandle(hProcessToken);

	return(fExe);
}
#endif

typedef struct tagMT4Thread  //MT4的线程结构体
{
	DWORD tID;//貌似是线程ID
	LONG priorityOriginal, prioritySet; //线程优先级
	BOOL isSuspended, isEntry;//是否暂停，是否入口
	ULONG64 lastCycleTime, deltaCycleTime;//上次循环时间，???
	int addr;//貌似是路径
} MT4Thread;

MT4Thread threads[100]; //结构体数组
int myBaseAddr = 0;
int threadsPos, updateThreadsCount;//用于数组遍历的

void dumpThreads()  //线程分析，打印出线程的信息
{
	for (int i = 0; i < threadsPos; ++i)
	{
		//threads[i].addr = threadsBA[i];
		char info2[1000];
		sprintf_s(info2, 1000,
		          "%sThread %d/%d id=%d, delta_cycles=%llu, addr=%#8x"
		          , threads[i].isEntry ? "*" : " "
		          , i + 1
		          , threadsPos
		          , threads[i].tID
		          , threads[i].deltaCycleTime
		          , threads[i].addr
		);
		debug("%s\n", info2);
	}
}
//设置线程优先级
void SetThreadPriority(int tNo, BOOL resume)
{
	if (tNo >= 0 && tNo < threadsPos)
	{
		//OpenThread是Windows API(应用程序接口) 中的一个常用函数，用于打开一个现有线程对象。 
		HANDLE hThread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, threads[tNo].tID);
		if (hThread == nullptr)//线程是空的
		{
			char info2[1000];
			sprintf_s(info2, 1000,
			          "ERROR OPEN Thread %d, delta_cycles, %llu"
			          , threads[tNo].tID
			          , threads[tNo].deltaCycleTime
			);
			debug("%s\n", info2);
		}
		else
		{
			if (resume)  //resume：继续，恢复
			{
				BOOL res = FALSE;
				if (threads[tNo].prioritySet != threads[tNo].priorityOriginal
					&& (res = SetThreadPriority(hThread, threads[tNo].priorityOriginal)))
				{
					threads[tNo].prioritySet = threads[tNo].priorityOriginal;//设置线程的优先级
					//
					char info2[1000];
					sprintf_s(info2, 1000,
					          "RESUME Thread %d, priority, %d, GetWindowThreadID()=%d"  //打印出设置好的优先级
					          , threads[tNo].tID
					          , threads[tNo].prioritySet
					          , GetWindowThreadID()
					);
					debug("%s\n", info2);
				}
				else if (!res)
				{
					char info2[1000];
					sprintf_s(info2, 1000,
					          "ERROR RESUME Thread %d, orig priority %d"   //调整线程优先级失败
					          , threads[tNo].tID
					          , threads[tNo].priorityOriginal
					);
					debug("%s\n", info2);
				}
			}
			else
			{
				BOOL res = FALSE;
				if (threads[tNo].prioritySet != THREAD_PRIORITY_IDLE
					&& (res = SetThreadPriority(hThread, THREAD_PRIORITY_IDLE)))
				{
					threads[tNo].prioritySet = THREAD_PRIORITY_IDLE ;
					//
					char info2[1000];
					sprintf_s(info2, 1000,
					          "SUSPEND Thread %d, priority, %d, GetWindowThreadID()=%d"  //延迟线程，调低线程
					          , threads[tNo].tID
					          , threads[tNo].prioritySet
					          , GetWindowThreadID()
					);
					debug("%s\n", info2);
				}
				else if (!res)
				{
					char info2[1000];
					sprintf_s(info2, 1000,
					          "ERROR SUSPEND Thread %d, orig priority %d"
					          , threads[tNo].tID
					          , threads[tNo].priorityOriginal
					);
					debug("%s\n", info2);
				}
			}
			CloseHandle(hThread);
		}
	}
	else if (threadsPos > 0)
	{
		char info2[1000];
		sprintf_s(info2, 1000,
		          "Invalid thread no %d, SetThreadPriority, %d"
		          , tNo
		          , resume
		);
		debug("%s\n", info2);
	}
}
//延时线程
void SuspendResumeThread(int tNo, BOOL resume)  //resume这个变量：true：恢复线程；false：延时线程
{
	if (tNo >= 0 && tNo < threadsPos)
	{
		HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threads[tNo].tID);
		if (hThread == nullptr)
		{
			char info2[1000];
			sprintf_s(info2, 1000,
			          "ERROR OPEN Thread %d, delta_cycles, %llu"
			          , threads[tNo].tID
			          , threads[tNo].deltaCycleTime
			);
			debug("%s\n", info2);
		}
		else
		{
			if (resume)
			{
				DWORD res = 0;
				if (threads[tNo].isSuspended && -1 != (res = ResumeThread(hThread)))  //ResumeThread 线程恢复函数
				{
					threads[tNo].isSuspended = FALSE;
					//
					char info2[1000];
					sprintf_s(info2, 1000,
					          "RESUME Thread %d, delta_cycles, %llu"
					          , threads[tNo].tID
					          , threads[tNo].deltaCycleTime
					);
					debug("%s\n", info2);
				}
				else if (res == -1)//恢复失败之后
				{
					char info2[1000];
					sprintf_s(info2, 1000,
					          "ERROR RESUME Thread %d, delta_cycles, %llu"
					          , threads[tNo].tID
					          , threads[tNo].deltaCycleTime
					);
					debug("%s\n", info2);
				}
			}
			else
			{
				DWORD res = 0;
				if (!threads[tNo].isSuspended && -1 != (res = SuspendThread(hThread)))
				{
					threads[tNo].isSuspended = TRUE;
					char info2[1000];
					sprintf_s(info2, 1000,
					          "SUSPEND Thread %d, delta_cycles, %llu"
					          , threads[tNo].tID
					          , threads[tNo].deltaCycleTime
					);
					debug("%s\n", info2);
				}
				else if (res == -1)
				{
					char info2[1000];
					sprintf_s(info2, 1000,
					          "ERROR SUSPEND Thread %d, delta_cycles, %llu"
					          , threads[tNo].tID
					          , threads[tNo].deltaCycleTime
					);
					debug("%s\n", info2);
				}
			}
			CloseHandle(hThread);
		}
	}
	else if (threadsPos > 0)
	{
		char info2[1000];
		sprintf_s(info2, 1000,
		          "Invalid thread no %d, SuspendResumeThread, %d"
		          , tNo
		          , resume
		);
		debug("%s\n", info2);
	}
}
// volatile  提醒编译器它后面所定义的变量随时都有可能改变，因此编译后的程序每次需要存储或读取这个变量的时候，都会直接从变量地址中读取数据。
bool volatile isGUIThreadSuspended = false;
DWORD volatile GUIThreadID = NULL;
//恢复或者延时GUI线程
void SuspendResumeGUIThread(BOOL resume)
{
	if (resume && !isGUIThreadSuspended || !resume && isGUIThreadSuspended)
	{
		return;
	}
	//
	if (WaitForSingleObject(hThreads, 60000) == WAIT_TIMEOUT)
	{
		return;
	}
	if (GUIThreadID == NULL)
	{
		GUIThreadID = GetWindowThreadID();
	}
	HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, GUIThreadID);
	if (hThread == nullptr)
	{
		char info2[1000];
		sprintf_s(info2, 1000,
		          "ERROR OPEN Thread %d"
		          , GUIThreadID
		);
		debug("%s\n", info2);
	}
	else
	{
		if (resume)
		{
			DWORD res = 0;
			if (isGUIThreadSuspended && -1 != (res = ResumeThread(hThread)))
			{
				isGUIThreadSuspended = FALSE;
				//
				char info2[1000];
				sprintf_s(info2, 1000,
				          "RESUME Thread %d"
				          , GUIThreadID
				);
				debug("%s\n", info2);
			}
			else if (res == -1)
			{
				char info2[1000];
				sprintf_s(info2, 1000,
				          "ERROR RESUME Thread %d"
				          , GUIThreadID
				);
				debug("%s\n", info2);
			}
		}
		else
		{
			DWORD res = 0;
			if (!isGUIThreadSuspended && -1 != (res = SuspendThread(hThread)))
			{
				isGUIThreadSuspended = TRUE;
				char info2[1000];
				sprintf_s(info2, 1000,
				          "SUSPEND Thread %d"
				          , GUIThreadID
				);
				debug("%s\n", info2);
			}
			else if (res == -1)
			{
				char info2[1000];
				sprintf_s(info2, 1000,
				          "ERROR SUSPEND Thread %d"
				          , GUIThreadID
				);
				debug("%s\n", info2);
			}
		}
		CloseHandle(hThread);
	}
	//
	ReleaseMutex(hThreads);
}

BOOL isAService;//是否后台服务
BOOL errorState = FALSE;


long lastUpdate = 0;


typedef NTSTATUS (WINAPI *NtQueryInformationThread)(
	   IN HANDLE ThreadHandle,
	   IN int ThreadInformationClass,
	   OUT PVOID ThreadInformation,
	   IN ULONG ThreadInformationLength,
	   OUT PULONG ReturnLength OPTIONAL
);
NtQueryInformationThread ntQit;


#endif //USE_MT4_THREADS

//这个类定义了外部的接口函数，主要是两部分：一部分是通信连接方面；另一部分是MQ的处理，具体的实现都是其他类来完成的，相当于是个代理。

static map<wstring, Client*> clients; //Client是C++ Socket的一个对象
static __int64 lastStatsDump = 0;

void dumpStatistics() 
{
#if DEBUG_DLL
	__int64 now = Util::currentTimeMillis();
	if (lastStatsDump == 0)
	{
		lastStatsDump = now;
	}
	else if (now - lastStatsDump > 600000 && lastStatsDump > -2)
	{
		lastStatsDump = lastStatsDump >= 0 ? now : -2;
		//
		try
		{
			printStats("-stats dump-", true);
			map<wstring, Client*>::iterator it;
			for (it = clients.begin(); it != clients.end(); ++it)
			{
				Client* c = it->second;
				if (c != nullptr) c->StatsCSV();
			}
			printStats("-stats dump-", false);
		}
		catch (...) {
			debug("%s> dumpStatistics error\n", Util::getCurrentTime().c_str());
		}
	}
#endif
}


/**
    Connect to FXServer  连接到FXServer
    Setup currency pair (symbol)   货币对
    Setup strategy name   策略名称
*/
MT4_EXPFUNC wchar_t const* __stdcall jfxConnect(wchar_t const* host, int port, wchar_t const* symbol, int period, wchar_t const* strategy)
{
	try
	{
		//res[0] = L'\0';
		//debug("%s> enter to jfxConnect, %s.\n", Util::getCurrentTime().c_str(), strategy);
		//
		if (DEBUG_DLL)
		{
			debug("enter to jfxConnect, %s.\n", strategy);
		}
		//
		ConnectionInfo ci(symbol, period, strategy);
		//
#ifndef USE_MT4_THREADS
		if (my_lock(&sConnectionsXP) /* WaitForSingleObject(hConnections, 60000) == WAIT_TIMEOUT */)
		{
			return L"";
		}
#else
		unique_lock<mutex> lock(sConnections);
#endif
		//
		Client* c = clients[ci.getKey()];  //连接为空
		if (c != nullptr)
		{
			debug("exit (0) from jfxConnect, %s.\n", ci.getKey());
			//fflush(debug);
			//
#ifndef USE_MT4_THREADS
			my_unlock(&sConnectionsXP) /* ReleaseMutex(hConnections) */;
#endif
			//
			return c->connInfo->getKey();//"";
		}
		//
		if (port == 17342)  //端口是17342
		{
			debug("jfxConnect [key=%s]\n", ci.getKey());
		}
		//
		c = clients[ci.getKey()] = new Client(host, port, symbol, period, strategy);  //新建连接Client
		//
#ifdef USE_MT4_THREADS
		BOOL ir = false;// InitMT4Threads();
		wchar_t msg[1000];
		swprintf_s(msg, 1000, L"%s %s", ir ? L"tok" : L"tnok", ci.getKey());
		debug("jfxConnect, %s.\n", msg);
#else
		debug("jfxConnect, %s.\n", ci.getKey());
#endif //USE_MT4_THREADS


		//fflush(debug);
		//
#ifndef USE_MT4_THREADS
		my_unlock(&sConnectionsXP) /* ReleaseMutex(hConnections) */;
#endif
		//
		// SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		//
		//wcscpy(res, ci.getKey());
		return c->connInfo->getKey();
	}
	catch (...)
	{
		if (port == 17342)
		{
			debug("erroneos exit from jfxConnect, %s.\n", strategy);
			//} else {
			//	debug("+");
		}
		//fflush(debug);
#ifndef USE_MT4_THREADS
		my_unlock(&sConnectionsXP) /* ReleaseMutex(hConnections) */;
#endif
		return L"";
	}
}

extern int hwnd;

void CALLBACK PrintInfo(char* b, size_t sz, LPARAM cPtr)//打印信息的回调，节约资源？
{
	debug("%s\n", b);
}

MT4_EXPFUNC void __stdcall jfxLog(wchar_t const* info)   //jfx的日志
{
	try
	{
		//debug("%s> %s\n", Util::getCurrentTime().c_str(), "jfxLog");
#ifdef USE_MT4_THREADS
		const wchar_t* x = wcsstr(info, L"gui_suspend=");
		if (x != nullptr)
		{
			// ReSharper disable once CppEntityNeverUsed
			int susp = _wtoi(x + 12);
			//			if (susp == 100 || susp == 200) 
			//				SuspendResumeGUIThread(susp == 200);
			return;
		}
#endif
#if DEBUG_DLL
#else
		const wchar_t * dest = wcsstr(info, L"debug=");
		if (dest != nullptr) {
			IS_DLL_IN_DEBUG_MODE = _wtoi(dest + 6);
		}
#endif
		mywcstombs(info, static_cast<WCSTOMBSPROC>(PrintInfo), NULL, '\0', '\0');
		//debug("%s> %s\n", Util::getCurrentTime().c_str(), info);
	}
	catch (...)
	{
		debug("error in jfxLog, %s.\n", info);
	}
}

Client* GetClient(wchar_t const* sessionID)		//根据SessionId获取连接
{
#ifndef USE_MT4_THREADS
	if (my_lock(&sConnectionsXP, 10000, true) /* WaitForSingleObject(hConnections, 60000) == WAIT_TIMEOUT */)
	{
		return nullptr;
	}
#else
	unique_lock<mutex> lock(sConnections);
#endif
	Client* c = clients[sessionID];
#ifndef USE_MT4_THREADS
	my_unlock(&sConnectionsXP);
#endif
	return c;
}

Client* RemoveClient(wchar_t const* sessionID)  //删除连接
{
#ifndef USE_MT4_THREADS
	if (my_lock(&sConnectionsXP, 10000, true) /* WaitForSingleObject(hConnections, 60000) == WAIT_TIMEOUT */)
	{
		return nullptr;
	}
#else
	unique_lock<mutex> lock(sConnections);
#endif
	Client* c = clients[sessionID];
	clients[sessionID] = nullptr;
#ifndef USE_MT4_THREADS
	my_unlock(&sConnectionsXP);
#endif
	return c;
}

extern bool swhide();
extern bool swshow();
MT4_EXPFUNC void __stdcall jfxHWnd(int hWnd, bool swHide)   //MT4的外部函数，貌似是操作jfx的句柄
{
	if (hWnd != 0)
	{
		hwnd = hWnd;
		if (swHide)
		{
			debug("-> sw hide\n");
			// ReSharper disable once CppEntityNeverUsed
			bool hidden = swhide();
		}
		else
		{
			debug("-> sw show\n");
			// ReSharper disable once CppEntityNeverUsed
			bool shown = swshow();
		}
	}
}

MT4_EXPFUNC void __stdcall jfxDisconnect(wchar_t const* sessionID) //断开连接
{
	debug("enter to jfxDisconnect, %s.\n", sessionID);
	//fflush(debug);
	//
	if (wcslen(sessionID) > 0)
	{
		lastStatsDump = lastStatsDump >= 0 ? -1 : lastStatsDump;
		dumpStatistics();
	}
	//
	Client* c = RemoveClient(sessionID);
	if (c != nullptr)
	{
		delete c;
		debug("exit (1) from jfxDisconnect, %s.\n", sessionID);
		//fflush(debug);
		//
	}
	else
	{
		debug("exit (2) from jfxDisconnect, %s.\n", sessionID);
		//fflush(debug);
	}
}

static int cmd = 0;
//获取命令
MT4_EXPFUNC int __stdcall jfxGetCommand(wchar_t const* sessionID, wchar_t* p1, wchar_t* p2, wchar_t* p3, wchar_t* p4, wchar_t* p5, wchar_t* p6, wchar_t* p7, wchar_t* p8, wchar_t* p9, wchar_t* p10, wchar_t* p11, wchar_t* p12, wchar_t* p13, wchar_t* p14, wchar_t* p15)
{
	int cmdId = -1;
	try
	{
		wchar_t* args[15];
		args[0] = p1;
		args[1] = p2;
		args[2] = p3;
		args[3] = p4;
		args[4] = p5;
		args[5] = p6;
		args[6] = p7;
		args[7] = p8;
		args[8] = p9;
		args[9] = p10;
		args[10] = p11;
		args[11] = p12;
		args[12] = p13;
		args[13] = p14;
		args[14] = p15;
		for (int a = 0; a < 15; ++a)
		{
			args[a][0] = L'\0';
		}
		//
		Client* c = GetClient(sessionID);
		//
		if (c == nullptr)
		{
			debug("jfxGetCommand, %s -> c == NULL.\n", sessionID);
			wcscpy_s(p1, 500, L"STOP");
			//copyStringToArray("STOP", p1);
			//Sleep(5000);
			return -1;
		}
		//
		//char hello[1000];
		//sprintf_s(hello, 1000, "sess_id=%s proto=%d", sessionID, proto);
		//debug("%s> jfxGetCommand2, %s\n", Util::getCurrentTime().c_str(), hello);
		//
		switch (proto)
		{
		case 0: cmdId = c->getCmdAsArray(args);
			break;
			//case 1: cmdId = c->getCmdAsArray3(args); break;
		case 2: cmdId = c->getCmdAsArray4(args);
			break;
		}
		//cmdId = c->getCmdAsArray2(args);
		//cmdId = c->getCmdAsArray3(args);
		//cmdId = c->getCmdAsArray4(args);
#ifdef USE_MT4_THREADS
		//SuspendResumeGUIThread(false);
		//
		//UpdateMT4Threads();
		//SuspendResumeMT4Threads(false, true);
#endif //USE_MT4_THREADS

	}
	catch (...)
	{
		//debug("%s> Error getting command\n", Util::getCurrentTime().c_str(), "");
		wcscpy_s(p1, 500, L"ERROR");
#ifdef USE_MT4_THREADS
		SuspendResumeGUIThread(true);
		//SuspendResumeMT4Threads(false, false);
#endif //USE_MT4_THREADS


		//copyStringToArray("ERROR", p1);
		//Sleep(2000);
	}

	return cmdId;
}

extern bool dumpRes;

void CALLBACK SendRes(char* b, size_t sz, LPARAM cPtr)
{
	reinterpret_cast<Client*>(cPtr)->sendRes(b, sz);
}
//发送结果
MT4_EXPFUNC void __stdcall jfxSendResult(wchar_t const* sessionID, wchar_t const* res)
{
	Client* c = GetClient(sessionID);
	if (c == nullptr)
	{
		return;
	}
	mywcstombs(res, static_cast<WCSTOMBSPROC>(SendRes), reinterpret_cast<LPARAM>(c), nullptr, '\0');
	//
	dumpStatistics();
}
//初始化位置？？？
MT4_EXPFUNC void __stdcall jfxPositionInit(wchar_t const* sessID, int mode)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("jfxPositionInit: %s\n", info);
	}
	else
	{
		c->PositionInit(mode);
	}
}
//订单？？？
MT4_EXPFUNC int __stdcall jfxPositionOrderInfo(wchar_t const* sessID, int is_history, int ticket, int type, int openTime, int closeTime, int magic, int expiration, wchar_t const* symbol, wchar_t const* comment, double lots, double openPrice, double closePrice, double sl, double tp, double profit, double commission, double swap)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("jfxPositionOrderInfo: %s\n", info);
		return 0;
	}
	else
	{
		return c->OrderInfo(is_history, ticket, type, openTime, closeTime, magic, expiration, symbol, comment, lots, openPrice, closePrice, sl, tp, profit, commission, swap);
	}
}
//这个目前还不知道是干嘛的
MT4_EXPFUNC wchar_t const* __stdcall jfxPositionRes(wchar_t const* sessID, int tCount, int hCount)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("jfxPositionRes: %s\n", info);
		return L"err:session-not-found";
	}
	//
	c->PositionRes(tCount, hCount);
	//
	return L"";
}

//初始化mql的行情，这些都是Client对象完成的
MT4_EXPFUNC int __stdcall jfxMqlRatesInit(wchar_t const* sessID)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("MqlRatesInit: %s\n", info);
		return 0;
	}
	//
	c->MqlRatesInit();
	//
	return 1;
}
//对汇率的操作，不知道Add是什么意思，貌似是增加到vector的后面
MT4_EXPFUNC int __stdcall jfxMqlRatesAdd(wchar_t const* sessID, MqlRates* rates)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("jfxPositionRes: %s\n", info);
		return 0;
	}
	//
	c->MqlRatesAdd(rates);
	//
//	wchar_t info[1000];
//	swprintf_s(info, 1000, L"rate: %d %.8g %.8g %.8g %.8g %ld %d %ld", rates->time, rates->open, rates->high, rates->low, rates->close, rates->tick_volume, rates->spread, rates->real_volume);
//	debug("%s> jfxAddMqlRates: %s\n", Util::getCurrentTime().c_str(), info);
	//
	return 1;
}
//Res是什么意思还不知道
MT4_EXPFUNC wchar_t const* __stdcall jfxMqlRatesRes(wchar_t const* sessID)
{
	Client* c = GetClient(sessID);
	if (c == nullptr)
	{
		wchar_t info[1000];
		swprintf_s(info, 1000, L"ERROR: session not found: %s", sessID);
		debug("MqlRatesRes: %s\n", info);
		return L"err:session-not-found";
	}
	//
	c->MqlRatesRes();
	//
	return L"";
}

