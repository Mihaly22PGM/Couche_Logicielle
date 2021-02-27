#include "FromClause.h"

FromClause::FromClause(string _from_sub)
{
	this->from_sub = _from_sub;
	this->extract_from_data();
}

FromClause::~FromClause()
{

}

string FromClause::get_from_sub()
{
	return this->from_sub;
}

string FromClause::get_table()
{
	return this->table;
}

void FromClause::extract_from_data()
{
	string str = this->from_sub.substr(5, std::string::npos);
	str.erase(remove(str.begin(), str.end(), '\''), str.end());
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
	this->table = str;
}