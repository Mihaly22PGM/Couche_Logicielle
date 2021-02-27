#include "DeleteStatement.h"

DeleteStatement::DeleteStatement(string _initial_query)
{
	this->initial_query = _initial_query;
	this->delete_sub_pos = this->initial_query.find(statements[0]);
	this->where_sub_pos = this->initial_query.find(statements[1]);
}

DeleteStatement::~DeleteStatement()
{

}

string DeleteStatement::get_initial_query()
{
	return this->initial_query;
}

DeleteClause DeleteStatement::generate_delete_clause()
{
	this->delete_sub = this->initial_query.substr(delete_sub_pos, where_sub_pos - delete_sub_pos);
	return DeleteClause(delete_sub);
}

WhereClause DeleteStatement::generate_where_clause()
{
	this->where_sub = this->initial_query.substr(where_sub_pos, std::string::npos);
	return WhereClause(where_sub);
}