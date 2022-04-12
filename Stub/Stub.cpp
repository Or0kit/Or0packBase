// Stub.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "framework.h"
#include "Stub.h"

extern __declspec(dllexport) GLOBAL_PARAM g_stcParam = { 0 };
LPGETPROCADDRESS  g_funGetProcAddress = nullptr;
LPLOADLIBRARYEX   g_funLoadLibraryEx = nullptr;

LPEXITPROCESS     g_funExitProcess = nullptr;
LPMESSAGEBOX      g_funMessageBox = nullptr;
LPGETMODULEHANDLE g_funGetModuleHandle = nullptr;
LPVIRTUALPROTECT  g_funVirtualProtect = nullptr;

DWORD GetKernel32Base()
{
	DWORD dwKernel32Addr = 0;
	__asm
	{
		push eax;
		mov eax, fs: [0x30] ;
		mov eax, [eax + 0xC];
		mov eax, [eax + 0x1C];
		mov eax, [eax];
		mov eax, [eax + 0x8];
		mov dwKernel32Addr, eax;
		pop eax;
	}
	return dwKernel32Addr;
}

// 执行成功返回函数的地址，失败返回0
DWORD GetGPAFunAddr()
{	
	DWORD dwKernelBase = 0;

	PIMAGE_DOS_HEADER pDosHeader = NULL;//DOS头 指针
	PIMAGE_NT_HEADERS pNtHeader = NULL;//NT头 指针
	PIMAGE_EXPORT_DIRECTORY pExportDirectory = NULL;//导出表 指针

	PDWORD  pExportAddress = NULL;
	PDWORD pExportName = NULL;
	PWORD pExportOrdinal = NULL;
	
	char szGetProcAddr[] = { 'G','e','t','P','r','o','c','A','d','d','r','e','s','s',0 };

	//遍历导出表时用的索引
	DWORD dwCnt = 0, index = 0;
	dwKernelBase = GetKernel32Base();

	// 遍历 Kernel32.dll 的导出表 找到 GetProcAddress
	pDosHeader = (PIMAGE_DOS_HEADER)dwKernelBase;
	pNtHeader = (PIMAGE_NT_HEADERS)((DWORD)dwKernelBase + pDosHeader->e_lfanew);
	pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((DWORD)dwKernelBase + pNtHeader->OptionalHeader.DataDirectory[0].VirtualAddress);

	pExportAddress = (PDWORD)(pExportDirectory->AddressOfFunctions + dwKernelBase);
	//导出函数名称表地址
	pExportName = (PDWORD)(pExportDirectory->AddressOfNames + dwKernelBase);
	//导出函数序号表地址
	pExportOrdinal = (PWORD)(pExportDirectory->AddressOfNameOrdinals + dwKernelBase);


	//由函数名查找函数地址
	char* pFinded = NULL, * pszGetProcAddr;
	// 返回函数的地址
	DWORD dwGetProcAddress = 0;
	do
	{
		pszGetProcAddr = szGetProcAddr;
		pFinded = (char*)((DWORD)dwKernelBase + pExportName[dwCnt]);
		for (; *pFinded == *pszGetProcAddr; pFinded++, pszGetProcAddr++)
		{
			if (*pszGetProcAddr == 0)
			{
				dwGetProcAddress = ((DWORD)dwKernelBase + pExportAddress[pExportOrdinal[dwCnt]]);
				break;
			}
		}
		dwCnt++;
	} while ((dwCnt < pExportDirectory->NumberOfNames) & (dwGetProcAddress == 0));

	return dwGetProcAddress;
}

bool InitializationAPI()
{
	HMODULE hModule;

	// 1. 初始化基础API
	g_funGetProcAddress = (LPGETPROCADDRESS)GetGPAFunAddr();
	g_funLoadLibraryEx = (LPLOADLIBRARYEX)g_funGetProcAddress((HMODULE)GetKernel32Base(), "LoadLibraryExW");

	// 2. 初始化其他API
	hModule = NULL;
	if (!(hModule = g_funLoadLibraryEx(L"kernel32.dll", NULL, NULL)))  return false;
	g_funExitProcess = (LPEXITPROCESS)g_funGetProcAddress(hModule, "ExitProcess");
	hModule = NULL;
	if (!(hModule = g_funLoadLibraryEx(L"user32.dll", NULL, NULL)))  return false;
	g_funMessageBox = (LPMESSAGEBOX)g_funGetProcAddress(hModule, "MessageBoxW");
	hModule = NULL;
	if (!(hModule = g_funLoadLibraryEx(L"kernel32.dll", NULL, NULL)))  return false;
	g_funGetModuleHandle = (LPGETMODULEHANDLE)g_funGetProcAddress(hModule, "GetModuleHandleW");
	hModule = NULL;
	if (!(hModule = g_funLoadLibraryEx(L"kernel32.dll", NULL, NULL)))  return false;
	g_funVirtualProtect = (LPVIRTUALPROTECT)g_funGetProcAddress(hModule, "VirtualProtect");

	return true;
}

void Decrypt()
{
	// 在导出的全局变量中读取需解密区域的起始于结束VA
	PBYTE lpStart = g_stcParam.lpStartVA;
	PBYTE lpEnd = g_stcParam.lpEndVA;

	// 循环解密
	while (lpStart < lpEnd)
	{
		*lpStart -= 0x88;
		*lpStart ^= 0xA1;
		lpStart++;
	}
}

