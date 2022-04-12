#pragma once

// �ؼ�PE��Ϣ
typedef struct _PE_INFO
{
	DWORD                 dwOEP;          // ��ڵ�
	DWORD                 dwImageBase;    // ӳ���ַ
	PIMAGE_DATA_DIRECTORY pDataDir;       // ����Ŀ¼ָ��
	IMAGE_DATA_DIRECTORY  stcExport;      // ����Ŀ¼
	PIMAGE_SECTION_HEADER pSectionHeader; // ���α�ͷ��ָ��
}PE_INFO, * PPE_INFO;


class CProcessingPE
{
private:
	DWORD             m_dwFileDataAddr; // Ŀ���ļ����ڻ������ĵ�ַ
	DWORD             m_dwFileDataSize; // Ŀ���ļ���С
	PIMAGE_DOS_HEADER m_pDos_Header;    // DOSͷָ��
	PIMAGE_NT_HEADERS m_pNt_Header;     // NTͷָ��

	PE_INFO           m_stcPeInfo;      // PE�ؼ���Ϣ
public:
	CProcessingPE();
	virtual ~CProcessingPE();
public:
	DWORD RVAToOffset(ULONG uRvaAddr);                                        // RVAת�ļ�ƫ��
	DWORD OffsetToRVA(ULONG uOffsetAddr);                                     // �ļ�ƫ��תRVA
	BOOL  GetPeInfo(LPVOID lpImageData, DWORD dwImageSize, PPE_INFO pPeInfo); // ��ȡPE�ļ�����Ϣ
	void  FixReloc(DWORD dwLoadImageAddr);                                    // �޸��ض�λ��Ϣ
	PVOID GetExpVarAddr(LPCTSTR strVarName);                                  // ��ȡ����ȫ�ֱ������ļ�ƫ��
	void  SetOEP(DWORD dwOEP);                                                // ������OEP
	PVOID AddSection(LPCTSTR strName, DWORD dwSize, DWORD dwChara, PIMAGE_SECTION_HEADER pNewSection, PDWORD lpSize); // �������
};

