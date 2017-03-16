#include "cef_manager.h"
#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"

#include "client_app.h"
#include "browser_handler.h"

#if defined(SUPPORT_CEF)
#if defined(SUPPORT_CEF_FLASH)
#if defined(_DEBUG)
#pragma comment(lib, "cef_sandbox_d.lib")
#else
#pragma comment(lib, "cef_sandbox.lib")
#endif
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

#if defined(SUPPORT_CEF_FLASH)
CefScopedSandboxInfo scoped_sandbox;
#endif
#endif

namespace nim_cef
{

BOOL CefMessageLoopDispatcher::IsIdleMessage(const MSG* pMsg)
{
	switch (pMsg->message)
	{
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
	case WM_PAINT:
		return FALSE;
	}

	return TRUE;
}

bool CefMessageLoopDispatcher::Dispatch(const MSG &msg)
{
	static BOOL bDoIdle = TRUE;

	TranslateMessage(&msg);
	DispatchMessage(&msg);

	if (IsIdleMessage(&msg))
	{
		bDoIdle = TRUE;
	}

	while (bDoIdle && !::PeekMessage(const_cast<MSG*>(&msg), NULL, 0, 0, PM_NOREMOVE))
	{
		CefDoMessageLoopWork();
		bDoIdle = FALSE;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////
CefManager::CefManager()
{
	browser_count_ = 0;
	is_enable_offset_render_ = true;
}

void CefManager::AddCefDllToPath()
{
#if !defined(SUPPORT_CEF)
	return;
#endif

	TCHAR path_envirom[4096] = { 0 };
	GetEnvironmentVariable(L"path", path_envirom, 4096);
	
	std::wstring cef_path = QPath::GetAppPath();	
#ifdef _DEBUG
	//cef_path += L"cef_debug"; // ���ڼ�ʹ��debugģʽ��Ҳʹ��cef release�汾��dll��Ϊ�����ε�cef�˳�ʱ���жϣ��������Ҫ����cef�Ĺ��ܲ���Ҫʹ��debug�汾��dll
	cef_path += L"cef";
#else
	cef_path += L"cef";
#endif
	if (!nbase::FilePathIsExist(cef_path, true))
	{
		MessageBox(NULL, L"���ѹCef.rarѹ����", L"��ʾ", MB_OK);
		exit(0);
	}
	std::wstring new_envirom = path_envirom;
	new_envirom.append(L";");
	new_envirom.append(cef_path);
	SetEnvironmentVariable(L"path", new_envirom.c_str());
}

// Cef�ĳ�ʼ���ӿڣ�ͬʱ��ע��ʹ�ø����汾��Cefʱ�����ĸ��ֿ�
// Cef1916�汾���ȶ�����������ʹ������������ĳЩ��debugģʽ��ҳ��ʱ����жϾ��棨�������Ǵ��󣩣���������Ϊ����html��׼֧�ֲ�����������releaseģʽ������ʹ��
// Cef2357�汾�޷�ʹ�ã����������ض�����Ϣ�������¼���ҳ�����Ⱦ���̻����
// Cef2526��2623�汾�Ը�����ҳ�涼֧�֣�Ψһ�ĿӾ���debugģʽ�ڶ��߳���Ϣѭ�������£������˳�ʱ���жϣ�����releaseģʽ������
//		(PS:��������߲�ʹ�ø���Cef���ܵĿ����������л���releaseģʽ��cef dll�ļ���������ʹ��deubg��Ҳ���ᱨ�����޸�AddCefDllToPath��������л���releaseĿ¼)
bool CefManager::Initialize(bool is_enable_offset_render)
{
#if !defined(SUPPORT_CEF)
	return true;
#endif
	is_enable_offset_render_ = is_enable_offset_render;

	void* sandbox_info = NULL;
#if defined(SUPPORT_CEF)
#if defined(SUPPORT_CEF_FLASH)
	sandbox_info = scoped_sandbox.sandbox_info();
#endif
#endif

	CefMainArgs main_args(GetModuleHandle(NULL));
	CefRefPtr<ClientApp> app(new ClientApp);
	
	// ��������ӽ����е��ã������ֱ���ӽ����˳�������exit_code���ش��ڵ���0
	// �����Browser�����е��ã�����������-1
	int exit_code = CefExecuteProcess(main_args, app.get(), sandbox_info);
	if (exit_code >= 0)
		return false;

	CefSettings settings;
#if !defined(SUPPORT_CEF_FLASH)
	settings.no_sandbox = true;
#endif

	// ����localstorage����Ҫ��·��ĩβ��"\\"��������ʱ�ᱨ��
	std::wstring app_data = nbase::win32::GetLocalAppDataDir();
#ifdef _DEBUG
	app_data.append(L"Netease\\NIM_Debug\\");
#else
	app_data.append(L"Netease\\NIM\\");
#endif

	CefString(&settings.cache_path) = app_data + L"CefLocalStorage";

	// ����debug log�ļ�λ��
	CefString(&settings.log_file) = app_data + L"cef.log";

	// ����ģ����ʹ�õ����̣�����ǧ��Ҫ��release�����汾��ʹ�ã��ٷ��Ѿ����Ƽ�ʹ�õ�����ģʽ
	// cef1916�汾debugģʽ:�ڵ�����ģʽ�³����˳�ʱ�ᴥ���ж�
#ifdef _DEBUG
	settings.single_process = true;
#else
	settings.single_process = false;
#endif

	// cef2623��2526�汾debugģʽ:��ʹ��multi_threaded_message_loopʱ�˳�����ᴥ���ж�
	// ����disable-extensions���������޸�������⣬���ǻᵼ��һЩҳ���ʱ����
	// ����Cef���߳���Ϣѭ��������nbase����Ϣѭ��
	settings.multi_threaded_message_loop = true;

	// ����������Ⱦ
	settings.windowless_rendering_enabled = is_enable_offset_render_;

	bool bRet = CefInitialize(main_args, settings, app.get(), sandbox_info);
	return bRet;
}

void CefManager::UnInitialize()
{
#if !defined(SUPPORT_CEF)
	return;
#endif
	QLOG_APP(L"shutting down cef...");
	CefShutdown();
}

bool CefManager::IsEnableOffsetRender() const
{
	return is_enable_offset_render_;
}

nbase::Dispatcher* CefManager::GetMessageDispatcher()
{
	return &message_dispatcher_;
}

void CefManager::AddBrowserCount()
{
	browser_count_++;
}

void CefManager::SubBrowserCount()
{
	browser_count_--;
	ASSERT(browser_count_ >= 0);
}

int CefManager::GetBrowserCount()
{
	return browser_count_;
}

void CefManager::PostQuitMessage(int nExitCode)
{
#if !defined(SUPPORT_CEF)
	::PostQuitMessage(nExitCode);
	return;
#endif

	// ��������Ҫ��������ʱ��ǧ��Ҫֱ�ӵ���::PostQuitMessage�����ǿ��ܻ������������û����Ϣ
	// Ӧ�õ�����������������ٺ��ٵ���::PostQuitMessage
	if (browser_count_ == 0)
	{
		Post2UI([nExitCode]()
	 	{
			::PostQuitMessage(nExitCode);
	 	});
	}
	else
	{
		auto cb = [nExitCode]()
		{
			CefManager::GetInstance()->PostQuitMessage(nExitCode);
		};

		nbase::ThreadManager::PostDelayedTask(kThreadUI, cb, nbase::TimeDelta::FromMilliseconds(500));
	}
}

}