#include "ValuesClause.h"

ValuesClause::ValuesClause(string _values_sub)
{
	this->values_sub = _values_sub;
	this->extract_values_data();
}

ValuesClause::~ValuesClause()
{

}

string ValuesClause::get_values_sub()
{
	return this->values_sub;
}

string ValuesClause::get_key()
{
	return this->key;
}

vector<string> ValuesClause::get_values()
{
	return this->values;
}

void ValuesClause::extract_values_data()
{
	string str = this->values_sub.substr(7, std::string::npos);
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
		this->values.push_back(token);
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
	this->values.push_back(str);
}