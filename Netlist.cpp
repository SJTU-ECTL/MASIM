#include "Netlist.h"
extern clock_t gStart;
extern int gTimeOut;

NetList::NetList(vector<Node> vecNode, vector<Node> vecIn, vector<int> vecnPo)
{
	m_vecNode = vecNode;
	m_vecIn = vecIn;
	m_vecnPO = vecnPo;
}

void NetList::ReadFromFile(string strFile)
{//config XMG from input strFile
	if (strFile != "")
	{
		if (strFile.back() == 'v')
			lorina::read_verilog(strFile, verilog_reader(m_net));
		else if (strFile.back() == 'f')
			m_net = Bliff2Xmg(strFile);
		else
			m_net = Aig2Xmg(strFile);
	}
	ConfigWithXMG();
}

void NetList::ConfigWithXMG()
{//config netlist with XMG
	m_vecIn.clear();
	m_vecNode.clear();
	m_vecnPO.clear();
	m_vecnSchedule.clear();
	unsigned int nNumGates = m_net.num_gates();
	m_nOffset = m_net.size() - nNumGates;
	m_nNumPI = m_nOffset - 1;
	m_nSize = nNumGates;

	vector<Node> vecNode(nNumGates);
	m_vecNode = vecNode;
	for (unsigned int i = 0; i < nNumGates; i++)
		m_vecNode[i].m_nIndex = i;

	m_net.foreach_gate([&](auto const& n)
		{
			int nIndexNow = m_net.node_to_index(n) - m_nOffset;
			if (m_net.is_maj(n))
				m_vecNode[nIndexNow].m_bMAJ = true;
			else if (m_net.is_xor3(n))
				m_vecNode[nIndexNow].m_bMAJ = false;
			else
				cout << "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";

			m_net.foreach_fanin(n, [&](auto const& f)
				{
					bool bCompf = m_net.is_complemented(f);
					auto fNode = m_net.get_node(f);
					int fIndex = m_net.node_to_index(fNode);
					//constant or PI or regular
					if (m_net.is_constant(fNode))//constant
					{
						if (bCompf)
							m_vecNode[nIndexNow].m_nConstPI = 1;
						else
							m_vecNode[nIndexNow].m_nConstPI = 0;
					}//PI or regular
					else if (!m_net.is_pi(fNode))//regular
					{
						int fIndexNow = fIndex - m_nOffset;
						m_vecNode[fIndexNow].m_vecnSucc.push_back(nIndexNow);
						m_vecNode[nIndexNow].m_vecnPred.push_back(fIndexNow);
						//m_vecNode[fIndexNow].m_vecNodeSucc.push_back(&m_vecNode[nIndexNow]);
						//m_vecNode[nIndexNow].m_vecNodePred.push_back(&m_vecNode[fIndexNow]);
						m_vecNode[nIndexNow].m_vecbPredComp.push_back(bCompf);
					}//PI
					else
					{
						m_vecNode[nIndexNow].m_vecnPredPI.push_back(fIndex - 1);
						m_vecNode[nIndexNow].m_vecbPredPIComp.push_back(bCompf);
					}
				});
		});

	m_net.foreach_po([&](auto const& f, auto i)
		{
			int nPO = m_net.node_to_index(m_net.get_node(f)) - m_nOffset;
			if ((nPO >= 0) && !IsInVector(nPO, m_vecnPO))
			{
				m_vecnPO.push_back(nPO);
				m_vecNode[nPO].m_bPO = true;
			}
			m_vecnPOIndex.push_back(m_net.node_to_index(m_net.get_node(f)) - 1);
			m_vecbPOComp.push_back(m_net.is_complemented(f));
		});

	for (Node& nd : m_vecNode)
	{
		nd.m_bScheduled = false;
		nd.m_nUnschedSucc = nd.m_vecnSucc.size();
		nd.m_nUnschedPred = nd.m_vecnPred.size();
	}
}

void NetList::UpdateMem(Node& nd)
{//update memory
	nd.m_bScheduled = true;
	for (int pi : nd.m_vecnPredPI)
	{
		Node& piNode = m_vecIn[pi];
		piNode.m_nUnschedSucc--;
		if (piNode.m_nUnschedSucc == 0)
		{
			for (auto it = piNode.m_mapArrayRow.begin(); it != piNode.m_mapArrayRow.end();)
			{
				if (m_matMemStatus[it->first][it->second] >= 0)
				{
					m_matMemStatus[it->first][it->second] = -1;
					it = piNode.m_mapArrayRow.erase(it);
				}
				else
					++it;
			}
		}
	}
	for (int fi : nd.m_vecnPred)
	{
		Node& fiNode = m_vecNode[fi];
		fiNode.m_nUnschedSucc--;
		if (fiNode.m_nUnschedSucc == 0)
		{
			for (auto it = fiNode.m_mapArrayRow.begin(); it != fiNode.m_mapArrayRow.end();)
			{
				if (!fiNode.m_bPO || fiNode.m_mapArrayRow.size() > 1)
				{
					m_matMemStatus[it->first][it->second] = -1;
					it = fiNode.m_mapArrayRow.erase(it);
				}
				else
					++it;
			}
		}
	}
}

void NetList::RemoveFromMem(int nArray, int nRow)
{//remove from memory
	int nIndex = m_matMemStatus[nArray][nRow];
	if (nIndex < 0)
	{
		cout << "Error remove1\n";
		getchar();
		return;
	}
	Node& nd = (nIndex < m_nNumPI) ? m_vecIn[nIndex] : m_vecNode[nIndex - m_nNumPI];
	m_matMemStatus[nArray][nRow] = -1;
	if ((nd.m_mapArrayRow.find(nArray) == nd.m_mapArrayRow.end()) || (nd.m_mapArrayRow[nArray] != nRow))
	{
		cout << "Error remove2\n";
		getchar();
		return;
	}
	nd.m_mapArrayRow.erase(nArray);
}

void NetList::AddToMem(int nIndex, int nArray, int nRow)
{//add to memory
	//cout << "Add " << nIndex << " " << nArray << " " << nRow << "\n";
	if (m_matMemStatus[nArray][nRow] >= 0)
		RemoveFromMem(nArray, nRow);
	Node& nd = (nIndex < m_nNumPI) ? m_vecIn[nIndex] : m_vecNode[nIndex - m_nNumPI];
	m_matMemStatus[nArray][nRow] = nIndex;
	nd.m_mapArrayRow[nArray] = nRow;
}

string NetList::PrintCompute(Node& nd, int nArray, int nRow)
{//print compute instruction
	//cout << "Compute " << nd.m_nIndex << " " << nArray << " " << nRow << "\n";
	string strResult;
	strResult = "Compute node " + to_string(nd.m_nIndex + m_nNumPI) + " in Array " + to_string(nArray) + " : ";
	//cout << strResult << "\n";
	vector<int> vecInstr = { (int)m_vecvecClkInst.size(), 0, (int)nd.m_nIndex + m_nNumPI, nArray, nRow };
	for (int pi : nd.m_vecnPredPI)
	{
		if (m_vecIn[pi].m_mapArrayRow.find(nArray) == m_vecIn[pi].m_mapArrayRow.end())
		{
			cout << "Error! cannot find pi fan-in " << pi << " for node " << nd.m_nIndex << "\n";
			getchar();
		}
		strResult += (to_string(m_vecIn[pi].m_mapArrayRow[nArray]) + " ");
		vecInstr.push_back(m_vecIn[pi].m_mapArrayRow[nArray]);
	}
	for (int fi : nd.m_vecnPred)
	{
		if (m_vecNode[fi].m_mapArrayRow.find(nArray) == m_vecNode[fi].m_mapArrayRow.end())
		{
			cout << "Error! cannot find fan-in " << fi << " for node " << nd.m_nIndex << "\n";
			getchar();
		}
		strResult += (to_string(m_vecNode[fi].m_mapArrayRow[nArray]) + " ");
		vecInstr.push_back(m_vecNode[fi].m_mapArrayRow[nArray]);
	}
	strResult += (" -> " + to_string(nRow));
	m_vecvecClkInst.push_back(vecInstr);
	//cout << strResult << "\n";
	return strResult;
}

string NetList::PrintCopy(int nArrayFrom, int nRowFrom, int nArrayTo, int nRowTo)
{//pring copy instruction
	string strResult;
	strResult = "Copy Array " + to_string(nArrayFrom) + " Row " + to_string(nRowFrom) + " -> Array " + to_string(nArrayTo) + " Row " + to_string(nRowTo);
	//cout << strResult << "\n";
	int nNdIndex = m_matMemStatus[nArrayFrom][nRowFrom];
	if (nNdIndex == -1)
	{
		cout << "Error!!! Copy from empty row!!\n";
		getchar();
	}
	if (nNdIndex == -2)
		nNdIndex = nArrayFrom * m_nArrayRow + nRowFrom;
	vector<int> vecInstr = { (int)m_vecvecClkInst.size(), 1, nNdIndex, nArrayTo, nRowTo, nArrayFrom, nRowFrom };
	m_vecvecClkInst.push_back(vecInstr);
	//strResult += (" ( " + std::to_string(nNdIndex) + " )");
	return strResult;
}

std::map<int, int>& NetList::GetTempArrayRow(int nNodeIndex)
{
	if (m_mapmapNodeArrayRowTmp.find(nNodeIndex) == m_mapmapNodeArrayRowTmp.end())
	{
		Node& nd = (nNodeIndex < m_nNumPI) ? m_vecIn[nNodeIndex] : m_vecNode[nNodeIndex - m_nNumPI];
		m_mapmapNodeArrayRowTmp[nNodeIndex] = nd.m_mapArrayRow;
	}

	return m_mapmapNodeArrayRowTmp[nNodeIndex];
}

std::map<int, int>& NetList::GetCurrentArrayRow(int nNodeIndex)
{
	if (m_mapmapNodeArrayRowTmp.find(nNodeIndex) == m_mapmapNodeArrayRowTmp.end())
	{
		Node& nd = (nNodeIndex < m_nNumPI) ? m_vecIn[nNodeIndex] : m_vecNode[nNodeIndex - m_nNumPI];
		return nd.m_mapArrayRow;
	}

	return m_mapmapNodeArrayRowTmp[nNodeIndex];
}

int NetList::CalcRlv(const Node& nd, int nArray, int nNdRow)
{//calculate relevance of nd in nArray
	int nRlv = 0;
	int nIndex = nd.m_nIndex;
	for (int child : nd.m_vecnSucc)
	{
		const Node& childNode = m_vecNode[child];
		if (childNode.m_bScheduled)
			continue;
		for (int part : childNode.m_vecnPred)
		{
			std::map<int, int>& mapArrayRow = GetCurrentArrayRow(part + m_nNumPI);
			if (mapArrayRow.find(nArray) != mapArrayRow.end() && mapArrayRow[nArray] != nNdRow)
				nRlv++;
		}
		for (int part : childNode.m_vecnPredPI)
		{
			std::map<int, int>& mapArrayRow = GetCurrentArrayRow(part);
			if (mapArrayRow.find(nArray) != mapArrayRow.end() && mapArrayRow[nArray] != nNdRow)
				nRlv++;
		}
	}
	return nRlv;
}

vector<int> NetList::PickRow(int nArray, bool bAllowCopy)
{//pick row in nArray
	vector<int> vecResult;
	for (int r = 0; r < m_nArrayRow; r++)
	{
		if (m_vecsetBan[nArray].find(r) != m_vecsetBan[nArray].end())
			continue;
		if (m_matMemStatus[nArray][r] == -1)
		{
			vecResult.push_back(r);
			vecResult.push_back(0);
			vecResult.push_back(0);
			//cout << "pick empty row " << r << "\n";
			return vecResult;//row, copy, relevance
		}
	}
	int nBestRow = -1;
	int nBestRlv = INT_MAX;
	for (int r = 0; r < m_nArrayRow; r++)
	{
		if (m_vecsetBan[nArray].find(r) != m_vecsetBan[nArray].end())
			continue;
		int nIndex = m_matMemStatus[nArray][r];
		if (nIndex < 0)
			continue;
		if (GetCurrentArrayRow(nIndex).size() <= 1)
			continue;
		const Node& DupNode = (nIndex < m_nNumPI) ? m_vecIn[nIndex] : m_vecNode[nIndex - m_nNumPI];
		int nDupRlv = CalcRlv(DupNode, nArray, r);
		if (nDupRlv < nBestRlv)
		{
			nBestRlv = nDupRlv;
			nBestRow = r;
		}
	}
	if (nBestRow >= 0)
	{
		//cout << "pick row " << nBestRow << " overwriting " << m_matMemStatus[nArray][nBestRow] << "\n";
		vecResult.push_back(nBestRow);
		vecResult.push_back(m_matMemStatus[nArray][nBestRow]);
		vecResult.push_back(0);
		vecResult.push_back(-nBestRlv);
		return vecResult;//row, var, copy, relevance
	}
	if (!bAllowCopy)
		return vecResult;
	nBestRlv = INT_MIN;
	int nBestToArray = -1;
	int nBestToRow = -1;
	for (int r = 0; r < m_nArrayRow; r++)
	{
		if (m_vecsetBan[nArray].find(r) != m_vecsetBan[nArray].end())
			continue;
		int nIndex = m_matMemStatus[nArray][r];
		if (nIndex == -2)
			continue;
		if (nIndex == -1)
		{
			cout << "ERROR! There should be free row\n";
			getchar();
		}
		const Node& NDupNode = (nIndex < m_nNumPI) ? m_vecIn[nIndex] : m_vecNode[nIndex - m_nNumPI];
		if (GetCurrentArrayRow(nIndex).size() > 1)
		{
			cout << "ERROR! There should be duplicated var\n";
			getchar();
		}
		int nRlv = CalcRlv(NDupNode, nArray, r);
		for (int aa = m_nArrayBegin; aa < m_nNumArray; aa++)
		{
			if (aa == nArray)
				continue;
			vector<int> vecaaResult = PickRow(aa, false);
			if (vecaaResult.size() == 0)
				continue;
			int naaRlv = CalcRlv(NDupNode, aa, vecaaResult[0]) + vecaaResult.back() - nRlv;
			if (naaRlv > nBestRlv)
			{
				nBestRow = r;
				nBestToArray = aa;
				nBestToRow = vecaaResult[0];
				nBestRlv = naaRlv;
			}
		}
	}
	if (nBestRow >= 0)
	{
		vecResult.push_back(nBestRow);
		vecResult.push_back(nBestToArray);
		vecResult.push_back(nBestToRow);
		vecResult.push_back(1);
		vecResult.push_back(nBestRlv);
		if (nBestToArray == nArray)
		{
			cout << "Same array 123\n";
			getchar();
		}
		return vecResult;//row, toarray, torow, copy, relevance
	}
	return vecResult;
}

void NetList::PickCrossRlvForNode(Node& nd, vector<vector<int>>& vecvecBestResult, int& nBestCopy, int& nBestRlv, int& nBestArray)
{//pick row for node according to #copy, relevance
	//cout << "===========================\n";
	//cout << "Node " << nd.m_nIndex << "\n";
	int nIndex = nd.m_nIndex;
	vector<vector<int>> tmp;
	vecvecBestResult = tmp;
	nBestArray = -1;
	m_vecsetBan.clear();
	set<int> setEpt;
	for (int a = 0; a < m_nNumArray; a++)
		m_vecsetBan.push_back(setEpt);
	for (int a = m_nArrayBegin; a < m_nNumArray; a++)
	{
		//cout << "Check array " << a << "\n";
		std::map<int, int> mapInRow;
		vector<vector<int>> vecvecResult;
		int nCopy = 0;
		int nRlv = 0;
		bool bCanSchedInArray = true;
		for (int aa = 0; aa < m_nNumArray; aa++)
			m_vecsetBan[a].clear();
		m_mapmapNodeArrayRowTmp.clear();
		set<int> setMissFanIn;
		for (int pi : nd.m_vecnPredPI)
		{
			if (m_vecIn[pi].m_mapArrayRow.find(a) != m_vecIn[pi].m_mapArrayRow.end())
			{
				m_vecsetBan[a].insert(m_vecIn[pi].m_mapArrayRow[a]);
				if (m_matMemStatus[a][m_vecIn[pi].m_mapArrayRow[a]] != -2)
					mapInRow[pi] = m_vecIn[pi].m_mapArrayRow[a];
			}
			else
				setMissFanIn.insert(pi);
		}
		for (int in : nd.m_vecnPred)
		{
			if (m_vecNode[in].m_mapArrayRow.find(a) != m_vecNode[in].m_mapArrayRow.end())
			{
				m_vecsetBan[a].insert(m_vecNode[in].m_mapArrayRow[a]);
				mapInRow[in + m_nNumPI] = m_vecNode[in].m_mapArrayRow[a];
			}
			else
				setMissFanIn.insert(in + m_nNumPI);
		}
		if (setMissFanIn.size() >= nBestCopy)
			continue;

		for (int missIn : setMissFanIn)
		{
			Node& missNd = (missIn < m_nNumPI) ? m_vecIn[missIn] : m_vecNode[missIn - m_nNumPI];
			//cout << "Pick row for miss fan-in " << missIn << "\n";
			vector<int> vecResult = PickRow(a, true);
			if (vecResult.size() == 0)
			{
				//cout << "Node " << nIndex + m_nNumPI << " cannot schedule in array " << a << "\n";
				bCanSchedInArray = false;
				break;
			}
			nCopy += (vecResult[vecResult.size() - 2] + 1);
			if (nCopy >= nBestCopy)
			{
				bCanSchedInArray = false;
				break;
			}
			nRlv += (vecResult[vecResult.size() - 1] + CalcRlv(missNd, a, vecResult[0]));
			GetTempArrayRow(missIn)[a] = vecResult[0];
			if (vecResult.size() == 4)
				GetTempArrayRow(vecResult[1]).erase(a);
			m_vecsetBan[a].insert(vecResult[0]);
			mapInRow[missIn] = vecResult[0];
			if (vecResult.size() == 5)
			{
				int nNdIdxInToArray = m_matMemStatus[vecResult[1]][vecResult[2]];
				if (nNdIdxInToArray >= 0)
					GetTempArrayRow(nNdIdxInToArray).erase(vecResult[1]);

				int nNdIdxMove = m_matMemStatus[a][vecResult[0]];
				std::map<int, int>& tmpMapMove = GetTempArrayRow(nNdIdxMove);
				tmpMapMove.erase(a);
				tmpMapMove[vecResult[1]] = vecResult[2];
				m_vecsetBan[vecResult[1]].insert(vecResult[2]);
			}
			vecResult.push_back(missIn);
			vecvecResult.push_back(vecResult);
		}
		if (!bCanSchedInArray)
			continue;
		
		//cout << "Schedule the result \n";
		vector<int> vecResultIn;
		for (auto it = mapInRow.begin(); it != mapInRow.end(); it++)
		{
			int nFanin = it->first;
			Node& inNd = (nFanin < m_nNumPI) ? m_vecIn[nFanin] : m_vecNode[nFanin - m_nNumPI];
			if (inNd.m_nUnschedSucc == 1 && !inNd.m_bPO)//tmp
			{
				//cout << "Have useless fan-in\n";
				vecResultIn.push_back(it->second);
				vecResultIn.push_back(0);
				vecResultIn.push_back(0);
				break;
			}
		}

		vector<int> vecResult;
		if (vecResultIn.size() == 3)
			vecResult = vecResultIn;
		else
			vecResult = PickRow(a, true);
		if (vecResult.size() == 0)
			continue;
		
		vecvecResult.push_back(vecResult);
		nCopy += (vecResult[vecResult.size() - 2]);
		nRlv += (vecResult[vecResult.size() - 1] + CalcRlv(nd, a, vecResult[0]));
		if ((nCopy < nBestCopy) || ((nCopy == nBestCopy) && (nRlv > nBestRlv)))
		{
			vecvecBestResult = vecvecResult;
			nBestArray = a;
			nBestCopy = nCopy;
			nBestRlv = nRlv;
		}
	}

}

void NetList::PrintSchedCrossRlv(Node& nd, vector<vector<int>>& vecvecBestResult, int& nBestArray)
{//print scheduling result for CrossRlv
	int nRes = vecvecBestResult.size();
	//cout << "Debug size = " << nRes << "\n";
	for (int i = 0; i < nRes; i++)
	{
		int nMissIn = -1;
		vector<int>& vecResult = vecvecBestResult[i];
		if (i != (nRes - 1))
		{
			nMissIn = vecResult.back();
			vecResult.pop_back();
		}
		if (vecResult.size() == 3)
			int ddd = 0;
		else if (vecResult.size() == 4)
		{
			//cout << "size = 4\n";
			RemoveFromMem(nBestArray, vecResult[0]);
		}
		else if (vecResult.size() == 5)
		{
			//cout << "size = 5\n";
			if (nBestArray == vecResult[1])
			{
				cout << "same copy array\n";
				getchar();
			}
			m_nCross++;
			//cout << PrintCopy(nBestArray, vecResult[0], vecResult[1], vecResult[2]) << "\n";
			//cout << "1\n";
			PrintCopy(nBestArray, vecResult[0], vecResult[1], vecResult[2]);
			if (m_matMemStatus[vecResult[1]][vecResult[2]] >= 0)
				RemoveFromMem(vecResult[1], vecResult[2]);
			AddToMem(m_matMemStatus[nBestArray][vecResult[0]], vecResult[1], vecResult[2]);
			RemoveFromMem(nBestArray, vecResult[0]);
		}
		else
		{
			cout << "error in size of result\n";
			getchar();
		}
		if (nMissIn != -1)
		{
			m_nCross++;
			Node& ndMiss = (nMissIn < m_nNumPI) ? m_vecIn[nMissIn] : m_vecNode[nMissIn - m_nNumPI];
			//cout << PrintCopy(ndMiss.m_mapArrayRow.begin()->first, ndMiss.m_mapArrayRow.begin()->second, nBestArray, vecResult[0]) << "\n";
			PrintCopy(ndMiss.m_mapArrayRow.begin()->first, ndMiss.m_mapArrayRow.begin()->second, nBestArray, vecResult[0]);
			AddToMem(nMissIn, nBestArray, vecResult[0]);
		}
	}
	vector<int>& vecResult = vecvecBestResult.back();
	//cout << PrintCompute(nd, nBestArray, vecResult[0]) << "\n";
	PrintCompute(nd, nBestArray, vecResult[0]);
	UpdateMem(nd);
	AddToMem(nd.m_nIndex + m_nNumPI, nBestArray, vecResult[0]);
}

void NetList::CrossRlvRA()
{//#copy, relevance row assignment
	m_vecvecClkInst.clear();
	m_nNumPI = m_nOffset - 1;
	m_nArrayBegin = m_nNumPI / m_nArrayRow;
	m_matMemStatus.clear();
	for (int a = 0; a < m_nNumArray; a++)
	{
		vector<int> vecTmp(m_nArrayRow, -1);
		m_matMemStatus.push_back(vecTmp);
	}

	vector<Node> vecPI(m_nNumPI);
	m_vecIn = vecPI;
	for (int i = 0; i < m_nNumPI; i++)
	{
		m_vecIn[i].m_mapArrayRow[i / m_nArrayRow] = i % m_nArrayRow;
		m_matMemStatus[i / m_nArrayRow][i % m_nArrayRow] = -2;
	}

	for (Node& nd : m_vecNode)
	{
		for (int pi : nd.m_vecnPredPI)
			m_vecIn[pi].m_nUnschedSucc++;
	}

	m_nCross = 0;

	for (int i = 0; i < m_vecsetBan.size(); i++)
		m_vecsetBan[i].clear();
	m_vecsetBan.clear();
	set<int> setEpt;
	for (int a = 0; a < m_nNumArray; a++)
		m_vecsetBan.push_back(setEpt);

	for (int nIndex : m_vecnSchedule)
	{
		Node& nd = m_vecNode[nIndex];
		vector<vector<int>> vecvecBestResult;
		int nBestCopy = INT_MAX;
		int nBestRlv = INT_MIN;
		int nBestArray = -1;
		PickCrossRlvForNode(nd, vecvecBestResult, nBestCopy, nBestRlv, nBestArray);

		if (nBestArray == -1)
		{
			cout << "ERROR! Cannot Schedule Node " << nd.m_nIndex << "\n";
			getchar();
			return;
		}
		PrintSchedCrossRlv(nd, vecvecBestResult, nBestArray);
	}
	//cout << "End\n";
}

void NetList::CrossRlvScheduleRand()
{//#copy, relevance random scheduler
	m_vecvecClkInst.clear();
	m_nNumPI = m_nOffset - 1;
	m_nArrayBegin = m_nNumPI / m_nArrayRow;
	m_matMemStatus.clear();
	for (int a = 0; a < m_nNumArray; a++)
	{
		vector<int> vecTmp(m_nArrayRow, -1);
		m_matMemStatus.push_back(vecTmp);
	}

	std::ofstream ofResult;
	
	vector<Node> vecPI(m_nNumPI);
	m_vecIn = vecPI;
	for (int i = 0; i < m_nNumPI; i++)
	{
		m_vecIn[i].m_mapArrayRow[i / m_nArrayRow] = i % m_nArrayRow;
		m_matMemStatus[i / m_nArrayRow][i % m_nArrayRow] = -2;
	}

	for (Node& nd : m_vecNode)
	{
		for (int pi : nd.m_vecnPredPI)
			m_vecIn[pi].m_nUnschedSucc++;
	}

	m_nCross = 0;

	m_vecnSchedule.clear();
	int nScheduledNode = 0;
	int nNumNode = m_vecNode.size();
	while (nScheduledNode < nNumNode)
	{
		if ((clock() - gStart) / CLOCKS_PER_SEC > gTimeOut)
		{
			m_nCross = INT_MAX;
			return;
		}
		//cout << nScheduledNode << "\n";
		vector<vector<vector<int>>> vecvecvecBestResult;
		int nBestCopy = INT_MAX;
		int nBestRlv = INT_MIN;
		vector<int> vecnBestArray;
		vector<int> vecnBestIndex;
		int nTry = 0;
		for (Node& nd : m_vecNode)
		{
			if (nTry > 100)
				break;
			if (nd.m_bScheduled || nd.m_nUnschedPred != 0)
				continue;
			vector<vector<int>> vvBestResult;
			int nC = INT_MAX;
			int nR = INT_MIN;
			int nA = -1;
			PickCrossRlvForNode(nd, vvBestResult, nC, nR, nA);
			//cout << nA << "\n";
			if (nA == -1)
				continue;
			nTry++;
			if ((nC > nBestCopy) || ((nC == nBestCopy) && (nR < nBestRlv)))
				continue;
			if ((nC < nBestCopy) || ((nC == nBestCopy) && (nR > nBestRlv)))
			{
				vecvecvecBestResult.clear();
				nBestCopy = nC;
				nBestRlv = nR;
				vecnBestArray.clear();
				vecnBestIndex.clear();
			}
			vecvecvecBestResult.push_back(vvBestResult);
			vecnBestArray.push_back(nA);
			vecnBestIndex.push_back(nd.m_nIndex);
		}
		//cout << "#Choice = " << vecnBestIndex.size() << "\n";

		int nPick = rand() % vecnBestIndex.size();
		Node& nd = m_vecNode[vecnBestIndex[nPick]];
		//cout << "Scheduled " << nd.m_nIndex << "\n";
		nd.m_bScheduled = true;
		for (int nSucc : nd.m_vecnSucc)
			m_vecNode[nSucc].m_nUnschedPred--;
		PrintSchedCrossRlv(nd, vecvecvecBestResult[nPick], vecnBestArray[nPick]);
		nScheduledNode++;
		nd.m_nES = m_vecnSchedule.size();
		m_vecnSchedule.push_back(nd.m_nIndex);

	}
}

void NetList::PrintCurrentInstr()
{//print current instruction
	std::ofstream ofResult;
	string strResultName = m_strBench + "_out" + ".txt";
	ofResult.open(strResultName.c_str(), std::ofstream::out);
	for (int a = 0; a < m_nNumArray; a++)
	{
		vector<int> vecTmp(m_nArrayRow, -1);
		m_matMemStatus.push_back(vecTmp);
	}
	ofResult << "Init:\n";
	vector<Node> vecPI(m_nNumPI);
	m_vecIn = vecPI;
	for (int i = 0; i < m_nNumPI; i++)
	{
		m_vecIn[i].m_mapArrayRow[i / m_nArrayRow] = i % m_nArrayRow;
		m_matMemStatus[i / m_nArrayRow][i % m_nArrayRow] = -2;
		ofResult << "PI " << i << " in Array " << i / m_nArrayRow << " Row " << i % m_nArrayRow << "\n";
	}
	ofResult << "\n";

	//op: #clk, 0, index, nArray, nRow, fanin1, fanin2, ...
	//cpy: #clk, 1, index, nArrayTo, nRowTo, nArrayFrom, nRowFrom
	for (int t = 0; t < m_vecvecClkInst.size(); t++)
	{
		vector<int>& vecInstr = m_vecvecClkInst[t];
		vecInstr[0] = t;
		if (vecInstr[1] == 0)
		{
			ofResult << "Compute node " << vecInstr[2] << " in Array " << vecInstr[3] << " : ";
			for (int i = 5; i < vecInstr.size(); i++)
				ofResult << vecInstr[i] << " ";
			ofResult << "-> " << vecInstr[4] << "\n";
		}
		else
			ofResult << "Copy Array " << vecInstr[5] << " Row " << vecInstr[6] << " -> Array " << vecInstr[3] << " Row " << vecInstr[4] << "\n";
	}
	ofResult.close();
}
