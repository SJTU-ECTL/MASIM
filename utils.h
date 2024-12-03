#pragma once
#include <iostream>
#include <time.h>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include<string>
#include<fstream>
#include <cmath>
#include<stack>
#include <algorithm>
#include <random>
#include "mockturtle\mockturtle.hpp"
#include "lorina\aiger.hpp"

using namespace mockturtle;
using namespace std;

inline xmg_network Aig2Xmg(string strAigName)
{
	mockturtle::aig_network aig;
	lorina::read_aiger(strAigName, mockturtle::aiger_reader(aig));
	xmg_npn_resynthesis resyn;
	exact_library<xmg_network, xmg_npn_resynthesis> lib(resyn);
	map_params ps;
	ps.skip_delay_round = true;
	ps.required_time = numeric_limits<double>::max();
	xmg_network xmg = mockturtle::map(aig, lib, ps);

	functional_reduction(xmg);
	xmg = cleanup_dangling(xmg);

	if (strAigName == "bench\\sqrt.aig")
		return xmg;

	depth_view xmg_depth{ xmg };
	fanout_view xmg_fanout{ xmg_depth };
	xmg_resubstitution(xmg_fanout);
	xmg_network xmg_res = xmg_fanout;
	xmg_res = cleanup_dangling(xmg);
	if (xmg_res.size() < xmg.size())
		xmg = xmg_res;

	return xmg;
}

inline xmg_network Bliff2Xmg(string strBliffName)
{
	cover_network cover;
	lorina::read_blif(strBliffName, blif_reader(cover));
	aig_network aig;
	convert_cover_to_graph(aig, cover);

	xmg_npn_resynthesis resyn;
	exact_library<xmg_network, xmg_npn_resynthesis> lib(resyn);

	map_params ps;
	ps.skip_delay_round = true;
	ps.required_time = numeric_limits<double>::max();
	xmg_network xmg = mockturtle::map(aig, lib, ps);
	functional_reduction(xmg);
	xmg = cleanup_dangling(xmg);

	depth_view xmg_depth{ xmg };
	fanout_view xmg_fanout{ xmg_depth };
	xmg_resubstitution(xmg_fanout);
	xmg_network xmg_res = xmg_fanout;
	xmg_res = cleanup_dangling(xmg);
	if (xmg_res.size() < xmg.size())
		xmg = xmg_res;

	return xmg;
}

inline bool IsInVector(int n, vector<int>& vecn)
{//whether n is in vecn
	for (int i = 0; i < vecn.size(); i++)
	{
		if (vecn[i] == n)
			return true;
	}
	return false;
}