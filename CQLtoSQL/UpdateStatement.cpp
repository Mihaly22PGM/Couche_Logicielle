#include "UpdateStatement.h"

UpdateStatement::UpdateStatement(string _initial_query)
{
	this->initial_query = _initial_query;
	this->update_sub_pos = this->initial_query.find(statements[0]);
	this->set_sub_pos = this->initial_query.find(statements[1]);
	this->where_sub_pos = this->initial_query.find(statements[2]);
}

UpdateStatement::~UpdateStatement()
{

}

string UpdateStatement::get_initial_query()
{
	return this->initial_query;
}

UpdateClause UpdateStatement::generate_update_clause()
{
	this->update_sub = this->initial_query.substr(update_sub_pos, set_sub_pos - update_sub_pos);
	return UpdateClause(update_sub);
}

SetClause UpdateStatement::generate_set_clause()
{
	this->set_sub = this->initial_query.substr(set_sub_pos, where_sub_pos - set_sub_pos);
	return SetClause(set_sub);
}

WhereClause UpdateStatement::generate_where_clause()
{
	this->where_sub = this->initial_query.substr(where_sub_pos, std::string::npos);
	return WhereClause(where_sub);
}