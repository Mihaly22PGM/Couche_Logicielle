#include "WhereClause.h"

WhereClause::WhereClause(string _where_sub)
{
	this->where_sub = _where_sub;
	this->extract_where_data();
}

WhereClause::~WhereClause()
{

}

string WhereClause::get_where_sub()
{
	return this->where_sub;
}

string WhereClause::get_key()
{
	return this->key;
}

void WhereClause::extract_where_data()
{
	rsize_t pos = 0;
	string str = this->where_sub.substr(6, std::string::npos);

	str.erase(remove(str.begin(), str.end(), '\''), str.end());
	str.erase(remove(str.begin(), str.end(), ';'), str.end());
	str.erase(remove(str.begin(), str.end(), ' '), str.end());

	while ((pos = str.find('=')) != std::string::npos)
	{
		str.erase(0, pos + 1);
	}

	this->key = str;
}