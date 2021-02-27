#include <string>

#include "DeleteClause.h"
#include "WhereClause.h"

using std::string; 

#pragma once
class DeleteStatement
{
private:
	//CONSTANTES
	const string statements[2] = { "DELETE ", "WHERE " };

	string initial_query;
	string delete_sub, where_sub = "";
	size_t delete_sub_pos, where_sub_pos = 0;

public:
	DeleteStatement(string);
	~DeleteStatement();

	//GET
	string get_initial_query();
	//...

	DeleteClause generate_delete_clause();
	WhereClause generate_where_clause();
};