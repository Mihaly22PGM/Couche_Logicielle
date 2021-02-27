#include "SelectClause.h"

SelectClause::SelectClause(string _select_sub)
{
	this->select_sub = _select_sub;
	this->extract_select_data();
}

SelectClause::~SelectClause()
{
	
}

string SelectClause::get_select_sub()
{
	return this->select_sub;
}

vector<string> SelectClause::get_fields()
{
	return this->fields;
}

void SelectClause::extract_select_data()
{
	string s = this->select_sub.substr(6, std::string::npos);
	size_t pos = 0;
	string token;

	while ((pos = s.find(',')) != std::string::npos) 
	{
		token = s.substr(0, pos);
		if ( token.find('.') != std::string::npos)
		{
			token = token.substr(token.find('.')+1);
		}
		token.erase(remove(token.begin(), token.end(), '\''), token.end());
		token.erase(remove(token.begin(), token.end(), ';'), token.end());
		token.erase(remove(token.begin(), token.end(), ' '), token.end());
		this->fields.push_back(token);
		s.erase(0, pos + 1);
	}

	if (s.find('.') != std::string::npos)
	{
		s = s.substr(s.find('.') + 1);
	}
	s.erase(remove(s.begin(), s.end(), '\''), s.end());
	s.erase(remove(s.begin(), s.end(), ';'), s.end());
	s.erase(remove(s.begin(), s.end(), ' '), s.end());
	this->fields.push_back(s);
}