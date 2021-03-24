#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Timer.h"
#include "Utils.h"

//#define DEBUG
#include "FPGrowth.h"

const std::size_t MIN_PATTERN_LENGTH = 5;
const std::size_t MIN_OCCURRENCE     = 10;

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#endif

int main(int argc, char** argv)
{
	UNUSED(argc);
	UNUSED(argv);
	Timer t;

	if (argc < 4)
	{
		std::cerr << "Usage: " << argv[0] << " DAT_FILE PATTERN_LEN OCC" << std::endl;
		return -1;
	}

	std::ifstream input(argv[1]);

	if (!input.is_open())
	{
		std::cerr << "Unable to open input file: " << argv[1] << std::endl;
		return -1;
	}

	std::cout << "Dataset: " << argv[1] << std::endl;
	std::cout << "Loading input data ... " << std::flush;
	t.Start();

	std::string line;

	Transactions transactions;

	while (std::getline(input, line))
	{
		Transaction tc;
		std::istringstream iss(line);
		while (iss)
		{
			ItemC v;
			if (iss >> v)
			{
				tc.push_back(v);
			}
		}

		transactions.push_back(tc);
	}

	t.Stop();

	std::cout << "Done after: " << t << std::endl;

	input.close();

	t.Start();
	FPGrowth fp(transactions, atoi(argv[3]), atoi(argv[2]));

	const Pattern* pPattern = fp.Growth();
	if (pPattern == nullptr) return 0;

	std::cout << "Memory Used: " << GetMemString() << std::endl;

	std::vector<const PatternType*> processed;
	//	PostProcessing(pPattern, transactions.size(), fp.GetItemCount(), atoi(argv[2]), 20, fp.GetId2Item(), processed);

	std::vector<PatternPair> closed;
	ClosedDetection(fp, pPattern, closed);

	t.Stop();
	std::cout << " ==== Full Runtime: " << t << " ====" << std::endl;

	return 0;
}
