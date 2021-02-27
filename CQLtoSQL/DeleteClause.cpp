#include "DeleteClause.h"

DeleteClause::DeleteClause(string _delete_sub)
{
	this->delete_sub = _delete_sub;
	this->extract_delete_data();
}

DeleteClause::~DeleteClause()
{

}

string DeleteClause::get_delete_sub()
{
	return this->delete_sub;
}

string DeleteClause::get_table()
{
	return this->table;
}

void DeleteClause::extract_delete_data()
{
	string str = this->delete_sub.substr(12, std::string::npos);
	str.erase(remove(str.begin(), str.end(), '\''), str.end());
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
	this->table = str;
}