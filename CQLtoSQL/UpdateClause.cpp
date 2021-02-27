#include "UpdateClause.h"

UpdateClause::UpdateClause(string _update_sub)
{
	this->update_sub = _update_sub;
	this->extract_update_data();
}

UpdateClause::~UpdateClause()
{

}

string UpdateClause::get_update_sub()
{
	return this->update_sub;
}

string UpdateClause::get_table()
{
	return this->table;
}

void UpdateClause::extract_update_data()
{
	string str = this->update_sub.substr(7, std::string::npos);
	str.erase(remove(str.begin(), str.end(), '\''), str.end());
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
	this->table = str;
}