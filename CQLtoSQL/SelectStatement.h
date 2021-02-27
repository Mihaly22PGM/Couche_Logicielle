#include <string>

#include "SelectClause.h"
#include "FromClause.h"
#include "WhereClause.h"

using std::string;

#pragma once
class SelectStatement
{
private:
	//CONSTANTES
	const string statements[4] = { "SELECT ", "FROM ", "WHERE ", "LIMIT "};

	string initial_query;
	string select_sub, from_sub, where_sub = "";
	size_t select_sub_pos, from_sub_pos, where_sub_pos, limit_sub_pos = 0;

public:
	SelectStatement(string);
	~SelectStatement();

	//GET
	string get_initial_query();
	//...

	SelectClause generate_select_clause();
	FromClause generate_from_clause();
	WhereClause generate_where_clause();
};