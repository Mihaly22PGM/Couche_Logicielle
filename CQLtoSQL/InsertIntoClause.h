#include <string>
#include <vector>

using namespace std;
using std::string;

#pragma once
class InsertIntoClause
{
private:
	string insert_into_sub;

	string table;
	vector<string> columns;

public:
	InsertIntoClause(string);
	~InsertIntoClause();

	string get_insert_into_sub();
	string get_table();
	vector<string> get_columns();

	void extract_insert_into_data();
};