#include <string>

using std::string;

#pragma once
class WhereClause
{
private:
	string where_sub;

	string key;

public:
	WhereClause(string);
	~WhereClause();

	string get_where_sub();
	string get_key();

	void extract_where_data();
};