#include <string>
#include <vector>

using namespace std;
using std::string;

#pragma once
class SelectClause
{
private:
	string select_sub;

	vector<string> fields;

public:
	SelectClause(string);
	~SelectClause();

	string get_select_sub();
	vector<string> get_fields();

	void extract_select_data();
};