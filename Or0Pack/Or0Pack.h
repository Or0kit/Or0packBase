// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 OR0PACK_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// OR0PACK_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#pragma once
#include "resource.h"
#include "CProcessingPE.h"
#include <stdlib.h>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#ifdef OR0PACK_EXPORTS
#define OR0PACK_API __declspec(dllexport)

#else
#define OR0PACK_API __declspec(dllimport)
#endif

OR0PACK_API bool Or0Pack(LPTSTR strPath, bool bShowMsg);

// 用以保存传递给Stub部分的参数
typedef struct _GLOBAL_PARAM
{
	BOOL  bShowMessage; // 是否显示解密信息
	DWORD dwOEP;        // 程序入口点
	PBYTE lpStartVA;    // 起始虚拟地址（被异或加密区）
	PBYTE lpEndVA;      // 结束虚拟地址（被异或加密区）
}GLOBAL_PARAM, * PGLOBAL_PARAM;

// 加壳时会用到的函数声明
extern void  Pretreatment(PBYTE lpCodeStart, PBYTE lpCodeEnd, PE_INFO stcPeInfo);                                             // 预处理函数
extern DWORD Implantation(LPVOID& lpFileData, DWORD dwSize, CProcessingPE* pobjPE, PE_INFO stcPeInfo, GLOBAL_PARAM stcParam); // Stub植入函数