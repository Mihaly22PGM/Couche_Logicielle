#include <string>

#include "UpdateClause.h"
#include "SetClause.h"
#include "WhereClause.h"

using std::string;

#pragma once
class UpdateStatement
{
private:
	//CONSTANTES
	const string statements[3] = { "UPDATE ", "SET ", "WHERE " };

	string initial_query;
	string update_sub, set_sub, where_sub = "";
	size_t update_sub_pos, set_sub_pos, where_sub_pos = 0;

public:
	UpdateStatement(string);
	~UpdateStatement();

	//GET
	string get_initial_query();
	//...

	UpdateClause generate_update_clause();
	SetClause generate_set_clause();
	WhereClause generate_where_clause();
};