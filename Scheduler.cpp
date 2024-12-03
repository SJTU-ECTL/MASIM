#include "Scheduler.h"
clock_t gStart;
int gTimeOut = 600;

Scheduler::Scheduler()
{
	m_nThread = 48;
	m_nTrialTot = 500;
}

void Scheduler::CRScheduleThread()
{
	srand(GetCurrentThreadId());
	while (WaitForSingleObject(m_hEventKillThread, 1) != WAIT_OBJECT_0)
	{
		if (m_nTrial > m_nTrialTot)
			break;
		LONG m = InterlockedIncrement(&m_nTrial);
		if (m > m_nTrialTot)
			break;
		if ((clock() - gStart) / CLOCKS_PER_SEC > gTimeOut)
			break;
		NetList net = m_netlist;
		net.CrossRlvScheduleRand();
		EnterCriticalSection(&m_rCritical);
		//cout << m << ": " << net.m_nCross << "\n";
		if (net.m_nCross < m_nBestCopy)
		{
			m_nBestCopy = net.m_nCross;
			m_bestNet = net;
		}
		LeaveCriticalSection(&m_rCritical);
	}
}

DWORD WINAPI ThreadCRSched(LPVOID lpParam)
{
	if (lpParam == NULL)
		return 0;

	Scheduler* pMyScheduler = (Scheduler*)lpParam;
	pMyScheduler->CRScheduleThread();
	return 0;
}

void Scheduler::CRScheduleMult()
{
	m_nTrial = 0;
	m_nBestCopy = INT_MAX;
	
	NetList net = m_netlist;
	for (int i = 0; i < net.m_vecNode.size(); i++)
		net.m_vecnSchedule.push_back(i);
	net.CrossRlvRA();
	m_bestNet = net;
	m_nBestCopy = net.m_nCross;
	
	gStart = clock();
	InitializeCriticalSection(&m_rCritical);
	m_hEventKillThread = CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset, initially reset
	HANDLE* pThreadHandle = new HANDLE[m_nThread];
	PDWORD pdwThreadID = new DWORD[m_nThread];
	for (int i = 0; i < m_nThread; i++)
	{
		pThreadHandle[i] = CreateThread(NULL, 0, ThreadCRSched, this, 0, pdwThreadID + i);
		assert(pThreadHandle[i] != NULL);
	}
	WaitForMultipleObjects(m_nThread, pThreadHandle, TRUE, INFINITE);
	for (int i = 0; i < m_nThread; i++)
		CloseHandle(pThreadHandle[i]);

	delete[] pThreadHandle;
	delete[] pdwThreadID;
	CloseHandle(m_hEventKillThread);
	DeleteCriticalSection(&m_rCritical);
}

void Scheduler::FurImproveThread()
{
	srand(GetCurrentThreadId());
	while (WaitForSingleObject(m_hEventKillThread, 1) != WAIT_OBJECT_0)
	{
		if ((m_nTrial > m_nTrialTot) || m_bImprove)
			break;
		LONG m = InterlockedIncrement(&m_nTrial);
		if (m > m_nTrialTot)
			break;
		NetList TryNet = m_netlist;

		TryNet.m_vecnSchedule = m_vecnBestSched;
		int nIndex = rand() % TryNet.m_vecNode.size();
		while (m_vecvecESRdy[nIndex].size() < 2)
			nIndex = rand() % TryNet.m_vecNode.size();
		int nRdyNd = m_vecvecESRdy[nIndex].size();
		int nNd = m_vecvecESRdy[nIndex][rand() % nRdyNd];
		while (nNd == TryNet.m_vecnSchedule[nIndex])
			nNd = m_vecvecESRdy[nIndex][rand() % nRdyNd];
		if (TryNet.m_vecnSchedule[m_vecNdES[nNd]] != nNd)
			break;
		//cout << "calculate node " << nNd << " in clock " << nIndex << "\n";
		TryNet.m_vecnSchedule.erase(TryNet.m_vecnSchedule.begin() + m_vecNdES[nNd]);
		TryNet.m_vecnSchedule.insert(TryNet.m_vecnSchedule.begin() + nIndex, nNd);
		if (m_bImprove)
			break;
		TryNet.CrossRlvRA();
		EnterCriticalSection(&m_rCritical);
		//cout << m << ": " << TryNet.m_nCross << "\n";
		if (TryNet.m_nCross < m_nBestCopy)
		{
			m_bImprove = true;
			m_nBestCopy = TryNet.m_nCross;
			m_bestNet = TryNet;
		}
		LeaveCriticalSection(&m_rCritical);
	}
}

DWORD WINAPI ThreadFurImprove(LPVOID lpParam)
{
	if (lpParam == NULL)
		return 0;

	Scheduler* pMyScheduler = (Scheduler*)lpParam;
	pMyScheduler->FurImproveThread();
	return 0;
}

bool Scheduler::FurImproveMult()
{
	m_bImprove = false;
	m_nTrial = 0;

	m_vecnBestSched = m_bestNet.m_vecnSchedule;
	m_vecvecESRdy.clear();
	vector<int> vvv(m_bestNet.m_vecNode.size(), 0);
	m_vecNdES = vvv;
	vector<int> vecUnCFI;
	set<int> setRdy;
	for (int i = 0; i < m_bestNet.m_vecNode.size(); i++)
	{
		vecUnCFI.push_back(m_bestNet.m_vecNode[i].m_vecnPred.size());
		if (m_bestNet.m_vecNode[i].m_vecnPred.size() == 0)
			setRdy.insert(i);
		m_vecNdES[m_vecnBestSched[i]] = i;
	}
	for (int i = 0; i < m_bestNet.m_vecNode.size(); i++)
	{
		vector<int> v;
		v.assign(setRdy.begin(), setRdy.end());
		m_vecvecESRdy.push_back(v);
		int nSchedIndex = m_vecnBestSched[i];
		setRdy.erase(nSchedIndex);
		for (int fi : m_bestNet.m_vecNode[nSchedIndex].m_vecnSucc)
		{
			vecUnCFI[fi]--;
			if (vecUnCFI[fi] == 0)
				setRdy.insert(fi);
		}
	}
	InitializeCriticalSection(&m_rCritical);
	m_hEventKillThread = CreateEvent(NULL, TRUE, FALSE, NULL); // manual reset, initially reset
	HANDLE* pThreadHandle = new HANDLE[m_nThread];
	PDWORD pdwThreadID = new DWORD[m_nThread];
	for (int i = 0; i < m_nThread; i++)
	{
		pThreadHandle[i] = CreateThread(NULL, 0, ThreadFurImprove, this, 0, pdwThreadID + i);
		assert(pThreadHandle[i] != NULL);
	}
	WaitForMultipleObjects(m_nThread, pThreadHandle, TRUE, INFINITE);
	for (int i = 0; i < m_nThread; i++)
		CloseHandle(pThreadHandle[i]);

	delete[] pThreadHandle;
	delete[] pdwThreadID;
	CloseHandle(m_hEventKillThread);
	DeleteCriticalSection(&m_rCritical);
	return m_bImprove;
}

bool Scheduler::CheckMultiArrayResultSimp(NetList& netlist, string strFile)
{
	string strLine;
	ifstream fin;
	fin.open(strFile, ios::in);
	if (!fin.is_open())
	{
		cout << "No file\n";
		return false;
	}
	istringstream sstream;

	while (getline(fin, strLine))
	{
		if (strLine == "")
			break;
	}
	int nCopy = 0;
	int nCompute = 0;
	vector<Node>& vecNode = netlist.m_vecNode;
	vector<int>& vecnPO = netlist.m_vecnPO;
	unsigned int nNumOp = (unsigned int)vecNode.size();
	unsigned int nNumPO = (unsigned int)vecnPO.size();
	int nNumPI = netlist.m_nOffset - 1;
	int nTotNodes = nNumOp + nNumPI;
	int nNumArray = netlist.m_nNumArray;
	int nArraySize = netlist.m_nArrayRow;

	vector<vector<int>> vecvecArrayRowStatus;
	for (int a = 0; a < nNumArray; a++)
	{
		vector<int> t(nArraySize, -1);
		vecvecArrayRowStatus.push_back(t);
	}

	for (int i = 0; i < nNumPI; i++)
		vecvecArrayRowStatus[i / nArraySize][i % nArraySize] = i;

	int nMaxArray = 0;
	int nMaxRow = 0;
	string strCommand, str1, str2, str3, str4, str5;
	int nArrayFrom, nArrayTo, nRowFrom, nRowTo, nIndex;
	while (getline(fin, strLine))
	{
		sstream.clear();
		sstream.str(strLine);
		sstream >> strCommand;
		if (strCommand == "Copy")
		{//Copy Array 1 Row 4 -> Array 0 Row 176
			sstream >> str1 >> nArrayFrom >> str2 >> nRowFrom >> str3 >> str4 >> nArrayTo >> str5 >> nRowTo;
			vecvecArrayRowStatus[nArrayTo][nRowTo] = vecvecArrayRowStatus[nArrayFrom][nRowFrom];
			if ((nArrayTo > nMaxArray) || ((nArrayTo == nMaxArray) && (nMaxRow > nRowTo)))
			{
				nMaxArray = nArrayTo;
				nMaxRow = nRowTo;
			}
			nCopy++;
			continue;
		}
		nCompute++;
		//Compute node 24 in Array 0 : 0 1 2  -> 24
		sstream >> str1 >> nIndex >> str2 >> str3 >> nArrayTo >> str4;
		Node& nd = netlist.m_vecNode[nIndex - nNumPI];
		for (int pi : nd.m_vecnPredPI)
		{
			nRowFrom = -1;
			sstream >> nRowFrom;
			if (vecvecArrayRowStatus[nArrayTo][nRowFrom] != pi)
			{
				cout << "Error! node " << nIndex << " has no fan-in " << pi << "\n";
				return false;
			}
		}
		for (int fi : nd.m_vecnPred)
		{
			nRowFrom = -1;
			sstream >> nRowFrom;
			if (vecvecArrayRowStatus[nArrayTo][nRowFrom] != (fi + nNumPI))
			{
				cout << "Error! node " << nIndex << " has no fan-in " << fi + nNumPI << "\n";
				return false;
			}
		}
		sstream >> str1 >> nRowTo;
		vecvecArrayRowStatus[nArrayTo][nRowTo] = nIndex;
		if ((nArrayTo > nMaxArray) || ((nArrayTo == nMaxArray) && (nRowTo > nMaxRow)))
		{
			nMaxArray = nArrayTo;
			nMaxRow = nRowTo;
		}
	}

	for (int i = 0; i < nNumPI; i++)
	{
		if (vecvecArrayRowStatus[i / nArraySize][i % nArraySize] != i)
		{
			cout << "Error! pi " << i << " not in position\n";
		}
	}

	for (int po : vecnPO)
	{
		int nPOIndex = po + nNumPI;
		bool bFind = false;
		for (int a = 0; a < nNumArray; a++)
		{
			for (int r = 0; r < nArraySize; r++)
			{
				if (vecvecArrayRowStatus[a][r] == nPOIndex)
				{
					//cout << "PO " << nPOIndex << " in array " << a << " row " << r << "\n";
					bFind = true;
					break;
				}
			}
			if (bFind)
				break;
		}
		if (!bFind)
		{
			cout << "Error! po " << nPOIndex << " not in mem\n";
			return false;
		}
	}
	cout << "#Copy = " << nCopy << ", #Compute = " << nCompute << "\n";
	if (nCopy != netlist.m_nCross)
	{
		cout << "Copy number error! " << netlist.m_nCross << " " << nCopy << "\n";
		return false;
	}
	netlist.m_nNumArray = nMaxArray + 1;
	return true;
}