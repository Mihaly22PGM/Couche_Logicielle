#include <string>
#include <vector>

using namespace std;
using std::string; 

#pragma once
class SetClause
{
private:
	string set_sub;

	vector<string> values;

public:
	SetClause(string);
	~SetClause();

	string get_set_sub();
	vector<string> get_values();

	void extract_set_data();
};