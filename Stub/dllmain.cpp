// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "Stub.h"

#pragma comment(linker, "/entry:\"StubEntryPoint\"") // 指定程序入口函数为StubEntryPoint()
#pragma comment(linker, "/merge:.data=.text")        // 将.data合并到.text
#pragma comment(linker, "/merge:.rdata=.text")       // 将.rdata合并到.text
#pragma comment(linker, "/section:.text,RWE")        // 将.text段的属性设置为可读、可写、可执行


void Start() {
	// 1. 初始化所有API
	if (!InitializationAPI())  return;


	// 2. 解密宿主程序
	Decrypt();


	// 3. 询问是否执行解密后的程序
	if (g_stcParam.bShowMessage)
	{
		int nRet = g_funMessageBox(NULL, L"解密完成，是否运行原程序？", L"解密完成", MB_OKCANCEL);
		if (IDCANCEL == nRet)  return;
	}

	// 4. 跳转到OEP
	__asm jmp g_stcParam.dwOEP;

}

void __declspec(naked) StubEntryPoint() 
{
	__asm sub esp, 0x50; // 提升堆栈，提高容错性。
	Start();
	__asm add esp, 0x50;
	__asm retn;
}