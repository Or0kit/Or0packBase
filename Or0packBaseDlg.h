
// Or0packBaseDlg.h: 头文件
//

#pragma once
#define A1PACK_BASE_EXPORTS
#include "./Or0Pack/Or0Pack.h"

#ifdef _DEBUG
#pragma comment(lib, "./Debug/Or0Pack.lib")
#else
#pragma comment(lib, "./Release/Or0Pack.lib")
#endif

// COr0packBaseDlg 对话框
class COr0packBaseDlg : public CDialogEx
{
// 构造
public:
	COr0packBaseDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OR0PACKBASE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedCheck1();
	
	CString m_strPath;
	BOOL m_bShowMsg;
	afx_msg void OnDropFiles(HDROP hDropInfo);
};
