#include "SelectStatement.h"

SelectStatement::SelectStatement(string _initial_query) 
{
	this->initial_query = _initial_query;
	this->select_sub_pos = this->initial_query.find(statements[0]);
	this->from_sub_pos = this->initial_query.find(statements[1]);
	this->where_sub_pos = this->initial_query.find(statements[2]);
	this->limit_sub_pos = this->initial_query.find(statements[3]);
}

SelectStatement::~SelectStatement()
{

}

string SelectStatement::get_initial_query()
{
	return this->initial_query;
}

SelectClause SelectStatement::generate_select_clause()
{
	this->select_sub = this->initial_query.substr(select_sub_pos, from_sub_pos - select_sub_pos);
	return SelectClause(select_sub);
}

FromClause SelectStatement::generate_from_clause()
{
	this->from_sub = this->initial_query.substr(from_sub_pos, where_sub_pos - from_sub_pos);
	return FromClause(from_sub);
}

WhereClause SelectStatement::generate_where_clause()
{
	this->where_sub = this->initial_query.substr(where_sub_pos, limit_sub_pos - where_sub_pos);
	return WhereClause(where_sub);
}