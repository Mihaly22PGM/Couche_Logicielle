#include <string>

using std::string; 

#pragma once
class DeleteClause
{
private:
	string delete_sub;

	string table;

public:
	DeleteClause(string);
	~DeleteClause();

	string get_delete_sub();
	string get_table();

	void extract_delete_data();
};