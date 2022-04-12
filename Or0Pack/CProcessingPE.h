#pragma once

// 关键PE信息
typedef struct _PE_INFO
{
	DWORD                 dwOEP;          // 入口点
	DWORD                 dwImageBase;    // 映像基址
	PIMAGE_DATA_DIRECTORY pDataDir;       // 数据目录指针
	IMAGE_DATA_DIRECTORY  stcExport;      // 导出目录
	PIMAGE_SECTION_HEADER pSectionHeader; // 区段表头部指针
}PE_INFO, * PPE_INFO;


class CProcessingPE
{
private:
	DWORD             m_dwFileDataAddr; // 目标文件所在缓存区的地址
	DWORD             m_dwFileDataSize; // 目标文件大小
	PIMAGE_DOS_HEADER m_pDos_Header;    // DOS头指针
	PIMAGE_NT_HEADERS m_pNt_Header;     // NT头指针

	PE_INFO           m_stcPeInfo;      // PE关键信息
public:
	CProcessingPE();
	virtual ~CProcessingPE();
public:
	DWORD RVAToOffset(ULONG uRvaAddr);                                        // RVA转文件偏移
	DWORD OffsetToRVA(ULONG uOffsetAddr);                                     // 文件偏移转RVA
	BOOL  GetPeInfo(LPVOID lpImageData, DWORD dwImageSize, PPE_INFO pPeInfo); // 获取PE文件的信息
	void  FixReloc(DWORD dwLoadImageAddr);                                    // 修复重定位信息
	PVOID GetExpVarAddr(LPCTSTR strVarName);                                  // 获取导出全局变量的文件偏移
	void  SetOEP(DWORD dwOEP);                                                // 设置新OEP
	PVOID AddSection(LPCTSTR strName, DWORD dwSize, DWORD dwChara, PIMAGE_SECTION_HEADER pNewSection, PDWORD lpSize); // 添加区段
};

