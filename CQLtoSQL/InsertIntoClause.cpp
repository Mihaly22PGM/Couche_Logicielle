#include "InsertIntoClause.h"

InsertIntoClause::InsertIntoClause(string _insert_into_sub)
{
	this->insert_into_sub = _insert_into_sub;
	this->extract_insert_into_data();
}

InsertIntoClause::~InsertIntoClause()
{

}

string InsertIntoClause::get_insert_into_sub()
{
	return this->insert_into_sub;
}

string InsertIntoClause::get_table()
{
	return this->table;
}

vector<string> InsertIntoClause::get_columns()
{
	return this->columns;
}

void InsertIntoClause::extract_insert_into_data()
{
	string str = this->insert_into_sub.substr(12, std::string::npos);
	string str_bis = str.substr(0, str.find("("));
	str_bis.erase(remove(str_bis.begin(), str_bis.end(), '\''), str_bis.end());
	str_bis.erase(remove(str_bis.begin(), str_bis.end(), ';'), str_bis.end());
	str_bis.erase(remove(str_bis.begin(), str_bis.end(), ' '), str_bis.end());
	this->table = str_bis;
	str.erase(0, str.find("("));
	
	size_t pos = 0;
	string token;

	while ((pos = str.find(',')) != std::string::npos)
	{
		token = str.substr(0, pos);
		if (token.find('.') != std::string::npos)
		{
			token = token.substr(token.find('.') + 1);
		}
		token.erase(remove(token.begin(), token.end(), '('), token.end());
		token.erase(remove(token.begin(), token.end(), '\''), token.end());
		token.erase(remove(token.begin(), token.end(), ';'), token.end());
		token.erase(remove(token.begin(), token.end(), ' '), token.end());
		this->columns.push_back(token);
		str.erase(0, pos + 1);
	}

	if (str.find('.') != std::string::npos)
	{
		str = str.substr(str.find('.') + 1);
	}
	str.erase(remove(str.begin(), str.end(), ')'), str.end());
	str.erase(remove(str.begin(), str.end(), '\''), str.end());
	str.erase(remove(str.begin(), str.end(), ';'), str.end());
	str.erase(remove(str.begin(), str.end(), ' '), str.end());
	this->columns.push_back(str);
}