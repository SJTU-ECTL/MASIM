#pragma once
#include "NetList.h"

class Scheduler
{
public:
	Scheduler();
	void CRScheduleThread();
	void CRScheduleMult();
	void FurImproveThread();
	bool FurImproveMult();
	bool CheckMultiArrayResultSimp(NetList& netlist, string strFile);

	NetList m_netlist;
	
	CRITICAL_SECTION m_rCritical;
	HANDLE	m_hEventKillThread;

	int m_nThread;

	volatile LONG m_nTrial;
	int m_nTrialTot;
	int m_nBestCopy;
	NetList m_bestNet;
	vector<vector<int>> m_vecvecESRdy;
	vector<int> m_vecNdES;
	vector<int> m_vecnBestSched;
	bool m_bImprove;
};