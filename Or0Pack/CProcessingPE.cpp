#include "pch.h"
#include "CProcessingPE.h"
#include<stdlib.h>
CProcessingPE::CProcessingPE()
{
}

CProcessingPE::~CProcessingPE()
{
}

DWORD CProcessingPE::RVAToOffset(ULONG dwRva)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);
	//判断是否落在头部
	if (dwRva < pSectionHeader[0].VirtualAddress)
	{
		return dwRva;
	}

	for (int i = 0; i < m_pNt_Header->FileHeader.NumberOfSections; i++)
	{
		//节在内存中的位置RVA
		DWORD dwSectionBeginRva = pSectionHeader[i].VirtualAddress;
		//节在内存中相对于文件中结束的位置RVA
		DWORD dwSectionEndRva = pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData;
		//判断RVA是否在当前节中
		if (dwRva >= dwSectionBeginRva && dwRva <= dwSectionEndRva)
		{
			//FOA = RVA - 节在内存中的位置 + 节在文件中的偏移
			DWORD dwFoa = dwRva - dwSectionBeginRva + pSectionHeader[i].PointerToRawData;
			return dwFoa;
		}
	}
	return 0;
}

DWORD CProcessingPE::OffsetToRVA(ULONG dwFoa)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);

	//判断是否落在头部
	if (dwFoa < m_pNt_Header->OptionalHeader.SizeOfHeaders)
	{
		return dwFoa;
	}

	for (int i = 0; i < m_pNt_Header->FileHeader.NumberOfSections; i++)
	{
		if (dwFoa >= pSectionHeader[i].PointerToRawData && \
			dwFoa < pSectionHeader[i].PointerToRawData + pSectionHeader[i].SizeOfRawData)
		{
			return dwFoa - pSectionHeader[i].PointerToRawData + pSectionHeader[i].VirtualAddress;
		}
	}
	return 0;
}

BOOL CProcessingPE::GetPeInfo(LPVOID lpImageData, DWORD dwImageSize, PPE_INFO pPeInfo)
{
	// 1、判断映像指针是否有效
	if (m_stcPeInfo.dwOEP)
	{
		CopyMemory(pPeInfo, &m_stcPeInfo, sizeof(PE_INFO));
		return true;
	}
	else
	{
		if (!lpImageData)  return false;
		m_dwFileDataAddr = (DWORD)lpImageData;
		m_dwFileDataSize = dwImageSize;
	}

	// 2. 获取基本信息
	// 2.1 获取DOS头、NT头
	m_pDos_Header = (PIMAGE_DOS_HEADER)lpImageData;
	m_pNt_Header = (PIMAGE_NT_HEADERS)((DWORD)lpImageData + m_pDos_Header->e_lfanew);
	// 2.2 获取OEP
	m_stcPeInfo.dwOEP = m_pNt_Header->OptionalHeader.AddressOfEntryPoint;
	// 2.3 获取映像基址
	m_stcPeInfo.dwImageBase = m_pNt_Header->OptionalHeader.ImageBase;
	// 2.4 获取关键数据目录表的内容
	PIMAGE_DATA_DIRECTORY lpDataDir = m_pNt_Header->OptionalHeader.DataDirectory;
	m_stcPeInfo.pDataDir = lpDataDir;
	CopyMemory(&m_stcPeInfo.stcExport, lpDataDir + IMAGE_DIRECTORY_ENTRY_EXPORT, sizeof(IMAGE_DATA_DIRECTORY));
	// 2.5 获取区段表与其他详细信息
	m_stcPeInfo.pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);

	// 3. 检查PE文件是否有效
	if ((m_pDos_Header->e_magic != IMAGE_DOS_SIGNATURE) || (m_pNt_Header->Signature != IMAGE_NT_SIGNATURE))
	{
		// 这不是一个有效的PE文件
		return false;
	}

	// 4. 传出处理结果
	CopyMemory(pPeInfo, &m_stcPeInfo, sizeof(PE_INFO));

	return true;
}

void CProcessingPE::FixReloc(DWORD dwLoadImageAddr)
{
	// 1. 获取映像基址与代码段指针
	DWORD             dwImageBase;
	PVOID             lpCode;
	dwImageBase = m_pNt_Header->OptionalHeader.ImageBase;
	lpCode = (PVOID)((DWORD)m_dwFileDataAddr + RVAToOffset(m_pNt_Header->OptionalHeader.BaseOfCode));

	// 2. 获取重定位表在内存中的地址
	PIMAGE_DATA_DIRECTORY  pDataDir;
	PIMAGE_BASE_RELOCATION pReloc;
	pDataDir = m_pNt_Header->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_BASERELOC;
	pReloc = (PIMAGE_BASE_RELOCATION)((DWORD)m_dwFileDataAddr + RVAToOffset(pDataDir->VirtualAddress));

	// 3. 遍历重定位表，并对目标代码进行重定位
	while (pReloc->SizeOfBlock && pReloc->SizeOfBlock < 0x100000)
	{
		// 3.1 取得重定位项TypeOffset与其数量
		PWORD  pTypeOffset = (PWORD)((DWORD)pReloc + sizeof(IMAGE_BASE_RELOCATION));
		DWORD  dwCount = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

		// 3.2 循环检查重定位项
		for (DWORD i = 0; i < dwCount; i++)
		{
			if (!*pTypeOffset) continue;

			// 3.2.1 获取此重定位项指向的指针
			DWORD  dwPointToRVA = (*pTypeOffset & 0x0FFF) + pReloc->VirtualAddress;
			PDWORD pPtr = (PDWORD)(RVAToOffset(dwPointToRVA) + (DWORD)m_dwFileDataAddr);
			// 3.2.2 计算重定位增量值
			DWORD dwIncrement = dwLoadImageAddr - dwImageBase;
			// 3.2.3 修复需重定位的地址数据
			*((PDWORD)pPtr) += dwIncrement;
			pTypeOffset++;
		}

		// 3.3 指向下一个重定位块，开始另一次循环
		pReloc = (PIMAGE_BASE_RELOCATION)((DWORD)pReloc + pReloc->SizeOfBlock);
	}
}

PVOID CProcessingPE::GetExpVarAddr(LPCTSTR strVarName)
{
	// 1、获取导出表地址，并将参数strVarName转为ASCII形式，方便对比查找
	CHAR szVarName[MAX_PATH] = { 0 };
	PIMAGE_EXPORT_DIRECTORY lpExport = (PIMAGE_EXPORT_DIRECTORY)(m_dwFileDataAddr + RVAToOffset(m_stcPeInfo.stcExport.VirtualAddress));
	WideCharToMultiByte(CP_ACP, NULL, strVarName, -1, szVarName, _countof(szVarName), NULL, FALSE);

	// 2、循环读取导出表输出项的输出函数，并依次与szVarName做比对，如果相同，则取出相对应的函数地址
	for (DWORD i = 0; i < lpExport->NumberOfNames; i++)
	{
		PDWORD pNameAddr = (PDWORD)(m_dwFileDataAddr + RVAToOffset(lpExport->AddressOfNames + i));
		PCHAR strTempName = (PCHAR)(m_dwFileDataAddr + RVAToOffset(*pNameAddr));
		if (!strcmp(szVarName, strTempName))
		{
			PDWORD pFunAddr = (PDWORD)(m_dwFileDataAddr + RVAToOffset(lpExport->AddressOfFunctions + i));
			return (PVOID)(m_dwFileDataAddr + RVAToOffset(*pFunAddr));
		}
	}
	return 0;
}

void CProcessingPE::SetOEP(DWORD dwOEP)
{
	m_pNt_Header->OptionalHeader.AddressOfEntryPoint = dwOEP;
}

PVOID CProcessingPE::AddSection(LPCTSTR strName, DWORD dwSize, DWORD dwChara, PIMAGE_SECTION_HEADER pNewSection, PDWORD lpSize)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);

	// 1. 获取基本信息
	DWORD dwDosSize = m_pDos_Header->e_lfanew;
	DWORD dwPeSize = sizeof(IMAGE_NT_HEADERS32);
	DWORD dwStnSize = m_pNt_Header->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
	DWORD dwHeadSize = dwDosSize + dwPeSize + dwStnSize;

	// 2. 在区段表中加入新区段的信息
	// 2.1 获取基本信息
	CHAR  szVarName[7] = { 0 };
	DWORD dwFileAlign = m_pNt_Header->OptionalHeader.FileAlignment;    // 文件粒度
	DWORD dwSectAlign = m_pNt_Header->OptionalHeader.SectionAlignment; // 区段粒度   
	WORD  dwNumOfsect = m_pNt_Header->FileHeader.NumberOfSections;     // 区段数目

	// 2.2 获取最后一个区段的信息
	IMAGE_SECTION_HEADER stcLastSect = { 0 };
	CopyMemory(&stcLastSect, &pSectionHeader[dwNumOfsect - 1], sizeof(IMAGE_SECTION_HEADER));

	// 2.3 根据区段粒度计算相应地址信息
	DWORD dwVStart = 0;                                                        // 虚拟地址起始位置
	DWORD dwFStart = stcLastSect.SizeOfRawData + stcLastSect.PointerToRawData; // 文件地址起始位置

	if (stcLastSect.Misc.VirtualSize % dwSectAlign)
		dwVStart = (stcLastSect.Misc.VirtualSize / dwSectAlign + 1) * dwSectAlign + stcLastSect.VirtualAddress;
	else
		dwVStart = (stcLastSect.Misc.VirtualSize / dwSectAlign) * dwSectAlign + stcLastSect.VirtualAddress;

	DWORD dwVirtualSize = 0; // 区段虚拟大小
	DWORD dwSizeOfRawData = 0; // 区段文件大小
	if (dwSize % dwSectAlign)
		dwVirtualSize = (dwSize / dwSectAlign + 1) * dwSectAlign;
	else
		dwVirtualSize = (dwSize / dwSectAlign) * dwSectAlign;

	if (dwSize % dwFileAlign)
		dwSizeOfRawData = (dwSize / dwFileAlign + 1) * dwFileAlign;
	else
		dwSizeOfRawData = (dwSize / dwFileAlign) * dwFileAlign;

	WideCharToMultiByte(CP_ACP, NULL, strName, -1, szVarName, _countof(szVarName), NULL, FALSE);

	// 2.4 组装一个新的区段头
	IMAGE_SECTION_HEADER stcNewSect = { 0 };
	CopyMemory(stcNewSect.Name, szVarName, 7);     // 区段名称
	stcNewSect.Misc.VirtualSize = dwVirtualSize;   // 虚拟大小
	stcNewSect.VirtualAddress = dwVStart;        // 虚拟地址
	stcNewSect.SizeOfRawData = dwSizeOfRawData; // 文件大小
	stcNewSect.PointerToRawData = dwFStart;        // 文件地址
	stcNewSect.Characteristics = dwChara;         // 区段属性

	// 2.5 写入指定位置
	CopyMemory((PVOID)((DWORD)m_dwFileDataAddr + dwHeadSize), &stcNewSect, sizeof(IMAGE_SECTION_HEADER));

	// 3. 修改区段数目字段NumberOfSections
	m_pNt_Header->FileHeader.NumberOfSections++;

	// 4. 修改PE文件的景象尺寸字段SizeOfImage
	m_pNt_Header->OptionalHeader.SizeOfImage += dwVirtualSize;

	// 5. 返回新区段的详细信息、大小，以及可直接访问的地址
	CopyMemory(pNewSection, &stcNewSect, sizeof(IMAGE_SECTION_HEADER));
	*lpSize = dwSizeOfRawData;
	return (PVOID)(m_dwFileDataAddr + dwFStart);
}
