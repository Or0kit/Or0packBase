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
	//�ж��Ƿ�����ͷ��
	if (dwRva < pSectionHeader[0].VirtualAddress)
	{
		return dwRva;
	}

	for (int i = 0; i < m_pNt_Header->FileHeader.NumberOfSections; i++)
	{
		//�����ڴ��е�λ��RVA
		DWORD dwSectionBeginRva = pSectionHeader[i].VirtualAddress;
		//�����ڴ���������ļ��н�����λ��RVA
		DWORD dwSectionEndRva = pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData;
		//�ж�RVA�Ƿ��ڵ�ǰ����
		if (dwRva >= dwSectionBeginRva && dwRva <= dwSectionEndRva)
		{
			//FOA = RVA - �����ڴ��е�λ�� + �����ļ��е�ƫ��
			DWORD dwFoa = dwRva - dwSectionBeginRva + pSectionHeader[i].PointerToRawData;
			return dwFoa;
		}
	}
	return 0;
}

DWORD CProcessingPE::OffsetToRVA(ULONG dwFoa)
{
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);

	//�ж��Ƿ�����ͷ��
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
	// 1���ж�ӳ��ָ���Ƿ���Ч
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

	// 2. ��ȡ������Ϣ
	// 2.1 ��ȡDOSͷ��NTͷ
	m_pDos_Header = (PIMAGE_DOS_HEADER)lpImageData;
	m_pNt_Header = (PIMAGE_NT_HEADERS)((DWORD)lpImageData + m_pDos_Header->e_lfanew);
	// 2.2 ��ȡOEP
	m_stcPeInfo.dwOEP = m_pNt_Header->OptionalHeader.AddressOfEntryPoint;
	// 2.3 ��ȡӳ���ַ
	m_stcPeInfo.dwImageBase = m_pNt_Header->OptionalHeader.ImageBase;
	// 2.4 ��ȡ�ؼ�����Ŀ¼�������
	PIMAGE_DATA_DIRECTORY lpDataDir = m_pNt_Header->OptionalHeader.DataDirectory;
	m_stcPeInfo.pDataDir = lpDataDir;
	CopyMemory(&m_stcPeInfo.stcExport, lpDataDir + IMAGE_DIRECTORY_ENTRY_EXPORT, sizeof(IMAGE_DATA_DIRECTORY));
	// 2.5 ��ȡ���α���������ϸ��Ϣ
	m_stcPeInfo.pSectionHeader = IMAGE_FIRST_SECTION(m_pNt_Header);

	// 3. ���PE�ļ��Ƿ���Ч
	if ((m_pDos_Header->e_magic != IMAGE_DOS_SIGNATURE) || (m_pNt_Header->Signature != IMAGE_NT_SIGNATURE))
	{
		// �ⲻ��һ����Ч��PE�ļ�
		return false;
	}

	// 4. ����������
	CopyMemory(pPeInfo, &m_stcPeInfo, sizeof(PE_INFO));

	return true;
}

void CProcessingPE::FixReloc(DWORD dwLoadImageAddr)
{
	// 1. ��ȡӳ���ַ������ָ��
	DWORD             dwImageBase;
	PVOID             lpCode;
	dwImageBase = m_pNt_Header->OptionalHeader.ImageBase;
	lpCode = (PVOID)((DWORD)m_dwFileDataAddr + RVAToOffset(m_pNt_Header->OptionalHeader.BaseOfCode));

	// 2. ��ȡ�ض�λ�����ڴ��еĵ�ַ
	PIMAGE_DATA_DIRECTORY  pDataDir;
	PIMAGE_BASE_RELOCATION pReloc;
	pDataDir = m_pNt_Header->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_BASERELOC;
	pReloc = (PIMAGE_BASE_RELOCATION)((DWORD)m_dwFileDataAddr + RVAToOffset(pDataDir->VirtualAddress));

	// 3. �����ض�λ������Ŀ���������ض�λ
	while (pReloc->SizeOfBlock && pReloc->SizeOfBlock < 0x100000)
	{
		// 3.1 ȡ���ض�λ��TypeOffset��������
		PWORD  pTypeOffset = (PWORD)((DWORD)pReloc + sizeof(IMAGE_BASE_RELOCATION));
		DWORD  dwCount = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

		// 3.2 ѭ������ض�λ��
		for (DWORD i = 0; i < dwCount; i++)
		{
			if (!*pTypeOffset) continue;

			// 3.2.1 ��ȡ���ض�λ��ָ���ָ��
			DWORD  dwPointToRVA = (*pTypeOffset & 0x0FFF) + pReloc->VirtualAddress;
			PDWORD pPtr = (PDWORD)(RVAToOffset(dwPointToRVA) + (DWORD)m_dwFileDataAddr);
			// 3.2.2 �����ض�λ����ֵ
			DWORD dwIncrement = dwLoadImageAddr - dwImageBase;
			// 3.2.3 �޸����ض�λ�ĵ�ַ����
			*((PDWORD)pPtr) += dwIncrement;
			pTypeOffset++;
		}

		// 3.3 ָ����һ���ض�λ�飬��ʼ��һ��ѭ��
		pReloc = (PIMAGE_BASE_RELOCATION)((DWORD)pReloc + pReloc->SizeOfBlock);
	}
}

PVOID CProcessingPE::GetExpVarAddr(LPCTSTR strVarName)
{
	// 1����ȡ�������ַ����������strVarNameתΪASCII��ʽ������ԱȲ���
	CHAR szVarName[MAX_PATH] = { 0 };
	PIMAGE_EXPORT_DIRECTORY lpExport = (PIMAGE_EXPORT_DIRECTORY)(m_dwFileDataAddr + RVAToOffset(m_stcPeInfo.stcExport.VirtualAddress));
	WideCharToMultiByte(CP_ACP, NULL, strVarName, -1, szVarName, _countof(szVarName), NULL, FALSE);

	// 2��ѭ����ȡ�����������������������������szVarName���ȶԣ������ͬ����ȡ�����Ӧ�ĺ�����ַ
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

	// 1. ��ȡ������Ϣ
	DWORD dwDosSize = m_pDos_Header->e_lfanew;
	DWORD dwPeSize = sizeof(IMAGE_NT_HEADERS32);
	DWORD dwStnSize = m_pNt_Header->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
	DWORD dwHeadSize = dwDosSize + dwPeSize + dwStnSize;

	// 2. �����α��м��������ε���Ϣ
	// 2.1 ��ȡ������Ϣ
	CHAR  szVarName[7] = { 0 };
	DWORD dwFileAlign = m_pNt_Header->OptionalHeader.FileAlignment;    // �ļ�����
	DWORD dwSectAlign = m_pNt_Header->OptionalHeader.SectionAlignment; // ��������   
	WORD  dwNumOfsect = m_pNt_Header->FileHeader.NumberOfSections;     // ������Ŀ

	// 2.2 ��ȡ���һ�����ε���Ϣ
	IMAGE_SECTION_HEADER stcLastSect = { 0 };
	CopyMemory(&stcLastSect, &pSectionHeader[dwNumOfsect - 1], sizeof(IMAGE_SECTION_HEADER));

	// 2.3 �����������ȼ�����Ӧ��ַ��Ϣ
	DWORD dwVStart = 0;                                                        // �����ַ��ʼλ��
	DWORD dwFStart = stcLastSect.SizeOfRawData + stcLastSect.PointerToRawData; // �ļ���ַ��ʼλ��

	if (stcLastSect.Misc.VirtualSize % dwSectAlign)
		dwVStart = (stcLastSect.Misc.VirtualSize / dwSectAlign + 1) * dwSectAlign + stcLastSect.VirtualAddress;
	else
		dwVStart = (stcLastSect.Misc.VirtualSize / dwSectAlign) * dwSectAlign + stcLastSect.VirtualAddress;

	DWORD dwVirtualSize = 0; // ���������С
	DWORD dwSizeOfRawData = 0; // �����ļ���С
	if (dwSize % dwSectAlign)
		dwVirtualSize = (dwSize / dwSectAlign + 1) * dwSectAlign;
	else
		dwVirtualSize = (dwSize / dwSectAlign) * dwSectAlign;

	if (dwSize % dwFileAlign)
		dwSizeOfRawData = (dwSize / dwFileAlign + 1) * dwFileAlign;
	else
		dwSizeOfRawData = (dwSize / dwFileAlign) * dwFileAlign;

	WideCharToMultiByte(CP_ACP, NULL, strName, -1, szVarName, _countof(szVarName), NULL, FALSE);

	// 2.4 ��װһ���µ�����ͷ
	IMAGE_SECTION_HEADER stcNewSect = { 0 };
	CopyMemory(stcNewSect.Name, szVarName, 7);     // ��������
	stcNewSect.Misc.VirtualSize = dwVirtualSize;   // �����С
	stcNewSect.VirtualAddress = dwVStart;        // �����ַ
	stcNewSect.SizeOfRawData = dwSizeOfRawData; // �ļ���С
	stcNewSect.PointerToRawData = dwFStart;        // �ļ���ַ
	stcNewSect.Characteristics = dwChara;         // ��������

	// 2.5 д��ָ��λ��
	CopyMemory((PVOID)((DWORD)m_dwFileDataAddr + dwHeadSize), &stcNewSect, sizeof(IMAGE_SECTION_HEADER));

	// 3. �޸�������Ŀ�ֶ�NumberOfSections
	m_pNt_Header->FileHeader.NumberOfSections++;

	// 4. �޸�PE�ļ��ľ���ߴ��ֶ�SizeOfImage
	m_pNt_Header->OptionalHeader.SizeOfImage += dwVirtualSize;

	// 5. ���������ε���ϸ��Ϣ����С���Լ���ֱ�ӷ��ʵĵ�ַ
	CopyMemory(pNewSection, &stcNewSect, sizeof(IMAGE_SECTION_HEADER));
	*lpSize = dwSizeOfRawData;
	return (PVOID)(m_dwFileDataAddr + dwFStart);
}
