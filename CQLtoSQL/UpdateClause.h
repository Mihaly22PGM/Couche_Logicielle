#include <string>

using std::string; 

#pragma once
class UpdateClause
{
private:
	string update_sub;

	string table;

public:
	UpdateClause(string);
	~UpdateClause();

	string get_update_sub();
	string get_table();

	void extract_update_data();
};