#pragma once
#include "utils.h"

class Node
{
public:
	Node()
	{
		m_bPO = false;
		m_nUnschedSucc = 0;
		m_nUnschedPred = 0;
		m_nConstPI = -1;
	}
	vector<int> m_vecnSucc;
	vector<int> m_vecnPred;
	vector<bool> m_vecbPredComp;
	vector<int> m_vecnPredPI;
	vector<bool> m_vecbPredPIComp;
	unsigned int m_nIndex;
	bool m_bPO;
	bool m_bMAJ;
	int m_nConstPI;

	int m_nUnschedPred;
	int m_nUnschedSucc;
	bool m_bScheduled;
	std::map<int, int> m_mapArrayRow;
	int m_nES;
};


class NetList
{
public:
	NetList() {}
	NetList(vector<Node> vecNode, vector<Node> vecIn, vector<int> vecnPo);
	void ReadFromFile(string strFile);
	void ConfigWithXMG();

	void UpdateMem(Node& nd);
	void RemoveFromMem(int nArray, int nRow);
	void AddToMem(int nIndex, int nArray, int nRow);
	string PrintCompute(Node& nd, int nArray, int nRow);
	string PrintCopy(int nArrayFrom, int nRowFrom, int nArrayTo, int nRowTo);
	int CalcRlv(const Node& nd, int nArray, int nNdRow);
	vector<int> PickRow(int nArray, bool bAllowCopy = true);
	void PickCrossRlvForNode(Node& nd, vector<vector<int>>& vecvecBestResult, int& nBestCopy, int& nBestRlv, int& nBestArray);
	void PrintSchedCrossRlv(Node& nd, vector<vector<int>>& vecvecBestResult, int& nBestArray);
	void CrossRlvRA();
	void CrossRlvScheduleRand();
	void PrintCurrentInstr();

	std::map<int, std::map<int, int>> m_mapmapNodeArrayRowTmp;
	std::map<int, int>& GetTempArrayRow(int nNodeIndex);
	std::map<int, int>& GetCurrentArrayRow(int nNodeIndex);

	xmg_network m_net;
	int m_nOffset;
	vector<Node> m_vecNode;
	vector<Node> m_vecIn;
	vector<int> m_vecnPO;
	vector<int> m_vecnSchedule;
	vector<int> m_vecnPOIndex;
	vector<bool> m_vecbPOComp;
	int m_nSize;
	int m_nCross;
	int m_nNumArray;
	int m_nArrayRow;
	int m_nNumPI;
	string m_strBench;

	vector<set<int>> m_vecsetBan;
	int m_nArrayBegin;
	vector<vector<int>> m_vecvecClkInst;
	vector<vector<int>> m_matMemStatus;
};
