// Or0Pack.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "framework.h"
#include "Or0Pack.h"
#include<stdio.h>

bool Or0Pack(LPTSTR strPath, bool bShowMsg)
{
	CProcessingPE objProcPE; // PE处理对象
	PE_INFO       stcPeInfo; // PE信息

	HANDLE  hFile_In;
	HANDLE  hFile_Out;
	DWORD   dwFileSize;
	LPVOID  lpFileImage;
	WCHAR   szOutPath[MAX_PATH] = { 0 };

	// 1. 生成输出文件路径
	LPWSTR strSuffix = PathFindExtension(strPath);         // 获取文件的后缀名
	wcsncpy_s(szOutPath, MAX_PATH, strPath, wcslen(strPath)); // 备份目标文件路径到szOutPath
	PathRemoveExtension(szOutPath);                        // 将szOutPath中保存路径的后缀名去掉
	wcscat_s(szOutPath, MAX_PATH, L"_Pack");                 // 在路径最后附加“_Pack”
	wcscat_s(szOutPath, MAX_PATH, strSuffix);                // 在路径最后附加刚刚保存的后缀名

	// 2. 获取文件信息，并映射进内存中
	if (INVALID_HANDLE_VALUE == (hFile_In = CreateFile(strPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)))
	{
		return false;
	}
	if (INVALID_FILE_SIZE == (dwFileSize = GetFileSize(hFile_In, NULL)))
	{
		CloseHandle(hFile_In);
		return false;
	}
	if (!(lpFileImage = VirtualAlloc(NULL, dwFileSize * 2, MEM_COMMIT, PAGE_READWRITE)))
	{
		CloseHandle(hFile_In);
		return false;
	}
	DWORD dwRet;
	if (!ReadFile(hFile_In, lpFileImage, dwFileSize, &dwRet, NULL))
	{
		MessageBox(0, L"2 INVALID_ViReadFile", 0, 0);
		CloseHandle(hFile_In);
		VirtualFree(lpFileImage, 0, MEM_RELEASE);
		return false;
	}

	// 3. 获取PE文件信息
	if (!objProcPE.GetPeInfo(lpFileImage, dwFileSize, &stcPeInfo))
	{
		MessageBox(0, L"3 获取PE文件信息", 0, 0);
		return false;
	};


	// 4. 获取目标文件代码段的起始结束信息
	//    读取第一个区段的相关信息，并将其加密（默认第一个区段为代码段）
	PBYTE lpStart = (PBYTE)(stcPeInfo.pSectionHeader->PointerToRawData + (DWORD)lpFileImage);
	PBYTE lpEnd = (PBYTE)((DWORD)lpStart + stcPeInfo.pSectionHeader->SizeOfRawData);
	PBYTE lpStartVA = (PBYTE)(stcPeInfo.pSectionHeader->VirtualAddress + stcPeInfo.dwImageBase);
	PBYTE lpEndVA = (PBYTE)((DWORD)lpStartVA + stcPeInfo.pSectionHeader->SizeOfRawData);

	// 5. 对文件进行预处理
	Pretreatment(lpStart, lpEnd, stcPeInfo);

	// 6. 植入Stub
	DWORD        dwStubSize = 0;
	GLOBAL_PARAM stcParam = { 0 };
	stcParam.bShowMessage = bShowMsg;
	stcParam.dwOEP = stcPeInfo.dwOEP + stcPeInfo.dwImageBase;
	stcParam.lpStartVA = lpStartVA;
	stcParam.lpEndVA = lpEndVA;
	dwStubSize = Implantation(lpFileImage, dwFileSize, &objProcPE, stcPeInfo, stcParam);

	// 7. 将处理完成后的结果写入到新文件中
	if (INVALID_HANDLE_VALUE != (hFile_Out = CreateFile(szOutPath, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL)))
	{
		DWORD dwRet = 0;
		WriteFile(hFile_Out, lpFileImage, dwStubSize + dwFileSize, &dwRet, NULL);
	}

	// 8. 释放相关资源并返回
	CloseHandle(hFile_In);
	CloseHandle(hFile_Out);
	VirtualFree(lpFileImage, 0, MEM_RELEASE);
	return true;
}

void Pretreatment(PBYTE lpCodeStart, PBYTE lpCodeEnd, PE_INFO stcPeInfo)
{
	// 1. 加密指定区域
	while (lpCodeStart < lpCodeEnd)
	{
		*lpCodeStart ^= 0xA1;
		*lpCodeStart += 0x88;
		lpCodeStart++;
	}

	// 2. 给第一个区段附加上可写属性
	PDWORD pChara = &(stcPeInfo.pSectionHeader->Characteristics);
	*pChara = *pChara | IMAGE_SCN_MEM_WRITE;
}

DWORD Implantation(LPVOID& lpFileData, DWORD dwSize, CProcessingPE* pobjPE, PE_INFO stcPeInfo, GLOBAL_PARAM stcParam)
{
	// 1. 在资源中读取文件内容
	HRSRC   hREC = NULL; // 资源对象
	HGLOBAL hREC_Handle = NULL; // 资源句柄
	DWORD   dwStubSize = NULL; // 文件大小
	LPVOID  lpResData = NULL; // 资源数据指针
	HMODULE hModule = GetModuleHandle(L"Or0Pack.dll");
	if (hModule == NULL)
	{
		MessageBox(0, L"GetModuleHandle Fail!", 0, 0);
		return 0;
	}
	if (!(hREC = FindResource(hModule, MAKEINTRESOURCE(IDR_STUB1), L"STUB")))  return false;
	if (!(hREC_Handle = LoadResource(hModule, hREC)))                          return false;
	if (!(lpResData = LockResource(hREC_Handle)))                              return false;
	if (!(dwStubSize = SizeofResource(hModule, hREC)))                         return false;

	// 2. 提取Stub部分的关键信息
	CProcessingPE objProcPE;
	PE_INFO       stcStubPeInfo;
	PBYTE         lpData = new BYTE[dwStubSize];
	// 2.1 将Stub复制到临时缓冲区，防止重复操作
	CopyMemory(lpData, lpResData, dwStubSize);
	// 2.2 获取Stub的PE信息
	objProcPE.GetPeInfo(lpData, dwStubSize, &stcStubPeInfo);
	// 2.3 算出代码段的相关信息（默认第一个区段为代码段）
	PBYTE lpText = (PBYTE)(stcStubPeInfo.pSectionHeader->PointerToRawData + (DWORD)lpData);
	DWORD dwTextSize = stcStubPeInfo.pSectionHeader->SizeOfRawData;

	// 3. 添加区段
	DWORD                 dwNewSectionSize = 0;
	IMAGE_SECTION_HEADER  stcNewSection = { 0 };
	PVOID lpNewSectionData = pobjPE->AddSection(
		L".A1Pass",
		dwTextSize, 
		IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE,
		&stcNewSection,
		&dwNewSectionSize
	);

	// 4. 对Stub部分进行的重定位操作
	//    新的加载地址 = (新区段的起始RVA - Stub的".Text"区段的起始RVA) + 映像基址
	DWORD dwLoadImageAddr = (stcNewSection.VirtualAddress - stcStubPeInfo.pSectionHeader->VirtualAddress) + stcPeInfo.dwImageBase;
	objProcPE.FixReloc(dwLoadImageAddr);

	// 5. 写入配置参数
	// 5.1 获取Stub的导出变量地址
	PVOID lpPatam = objProcPE.GetExpVarAddr(L"g_stcParam");
	// 5.2 保存配置信息到Stub中
	CopyMemory(lpPatam, &stcParam, sizeof(GLOBAL_PARAM));

	// 6. 将Stub复制到新区段中
	CopyMemory(lpNewSectionData, lpText, dwTextSize);

	// 7. 计算并设置新OEP
	DWORD dwNewOEP = 0;
	// 7.1 计算新OEP
	DWORD dwStubOEP = stcStubPeInfo.dwOEP;
	DWORD dwStubTextRVA = stcStubPeInfo.pSectionHeader->VirtualAddress;
	DWORD dwNewSectionRVA = stcNewSection.VirtualAddress;
	dwNewOEP = (dwStubOEP - dwStubTextRVA) + dwNewSectionRVA;
	// 7.2 设置新OEP
	pobjPE->SetOEP(dwNewOEP);

	// 8. 释放资源，函数返回
	delete[] lpData;
	FreeResource(hREC_Handle);
	return dwNewSectionSize;
}
