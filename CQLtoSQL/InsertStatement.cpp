#include "InsertStatement.h"

InsertStatement::InsertStatement(string _initial_query)
{
	this->initial_query = _initial_query;
	this->insert_into_sub_pos = this->initial_query.find(statements[0]);
	this->values_sub_pos = this->initial_query.find(statements[1]);
}

InsertStatement::~InsertStatement()
{

}

string InsertStatement::get_initial_query()
{
	return this->initial_query;
}

InsertIntoClause InsertStatement::generate_insert_into_clause() 
{
	this->insert_into_sub = this->initial_query.substr(insert_into_sub_pos, values_sub_pos - insert_into_sub_pos);
	return InsertIntoClause(insert_into_sub);
}

ValuesClause InsertStatement::generate_values_clause()
{
	this->values_sub = this->initial_query.substr(values_sub_pos, std::string::npos);
	return ValuesClause(values_sub);
}