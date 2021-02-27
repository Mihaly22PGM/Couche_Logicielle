#include <string>
#include <vector>

using namespace std;
using std::string;

#pragma once
class ValuesClause
{
private:
	string values_sub;

	string key;
	vector<string> values;

public:
	ValuesClause(string);
	~ValuesClause();

	string get_values_sub();
	string get_key();
	vector<string> get_values();

	void extract_values_data();
};

