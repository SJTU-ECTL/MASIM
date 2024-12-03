#include "Scheduler.h"
int main()
{
	ifstream fin;
	fin.open("benchmarks.txt", ios::in);
	std::ofstream ofCSV;
	string strResultName = "result.csv";
	ofCSV.open(strResultName.c_str(), std::ofstream::out, _SH_DENYWR);
	string strBench, strLine;
	int nArrayRow;
	istringstream sstream;
	bool bOK = true;
	while (getline(fin, strLine))
	{
		sstream.clear();
		sstream.str(strLine);
		sstream >> strBench >> nArrayRow;
		Scheduler MyScheduler;
		MyScheduler.m_netlist.m_strBench = strBench;
		//MyScheduler.m_netlist.ReadFromFile(strBench);  //XMG netlist (.v / .bliff / .aig)
		MyScheduler.m_netlist.ReadFromFile("OPP\\" + strBench + ".v");
		cout << strBench << " " << MyScheduler.m_netlist.m_vecNode.size() << "\n";
		MyScheduler.m_netlist.m_nArrayRow = nArrayRow;
		MyScheduler.m_netlist.m_nNumArray = 8;
		string strSched = strBench + "_out.txt";

		MyScheduler.CRScheduleMult();
		int nImproveIter = 0;
		while (MyScheduler.FurImproveMult())
		{
			//cout << "Improve" << MyScheduler.m_nBestCopy << "\n";
			nImproveIter++;
		}
		MyScheduler.m_bestNet.PrintCurrentInstr();
		if (MyScheduler.CheckMultiArrayResultSimp(MyScheduler.m_bestNet, strSched))
			cout << "Checked OK\n";
		else
		{
			cout << "ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
			bOK = false;
			getchar();
		}

		ofCSV << strBench << "," << MyScheduler.m_bestNet.m_vecNode.size() << "," << MyScheduler.m_bestNet.m_nCross << "\n";
		ofCSV.flush();
	}
	if (bOK)
		cout << "Everything OK\n";
	getchar();
	getchar();
	fin.close();
	ofCSV.close();
	return 0;
}