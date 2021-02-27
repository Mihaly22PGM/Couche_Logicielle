#include <string>

#include "InsertIntoClause.h"
#include "ValuesClause.h"

using std::string;

#pragma once
class InsertStatement
{
private:
	//CONSTANTES
	const string statements[2] = { "INSERT INTO ", "VALUES " };

	string initial_query;
	string insert_into_sub, values_sub = "";
	size_t insert_into_sub_pos, values_sub_pos = 0;

public:
	InsertStatement(string);
	~InsertStatement();

	//GET
	string get_initial_query();
	//...

	InsertIntoClause generate_insert_into_clause();
	ValuesClause generate_values_clause();
};

