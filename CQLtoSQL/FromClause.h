#include <string>

using std::string;

#pragma once
class FromClause
{
private:
	string from_sub;

	string table;

public:
	FromClause(string);
	~FromClause();

	string get_from_sub();
	string get_table();

	void extract_from_data();
};

