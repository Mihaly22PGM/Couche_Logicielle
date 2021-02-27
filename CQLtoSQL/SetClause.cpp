#include "SetClause.h"

SetClause::SetClause(string _set_sub)
{
	this->set_sub = _set_sub;
	this-> extract_set_data();
}

SetClause::~SetClause()
{

}

string SetClause::get_set_sub()
{
	return this->set_sub;
}

vector<string> SetClause::get_values()
{
	return this->values;
}

void SetClause::extract_set_data()
{
	string s = this->set_sub.substr(3, std::string::npos);
	size_t pos = 0;
	size_t pos_bis = 0;
	string token;
	string token_bis;

	while ((pos = s.find(',')) != std::string::npos)
	{
		token = s.substr(0, pos);

		while ((pos_bis = token.find('=')) != std::string::npos)
		{
			token_bis = token.substr(0, pos_bis);
			if (token_bis.find('.') != std::string::npos)
			{
				token_bis = token_bis.substr(token.find('.') + 1);
			}
			token_bis.erase(remove(token_bis.begin(), token_bis.end(), '\''), token_bis.end());
			token_bis.erase(remove(token_bis.begin(), token_bis.end(), ';'), token_bis.end());
			token_bis.erase(remove(token_bis.begin(), token_bis.end(), ' '), token_bis.end());
			this->values.push_back(token_bis);		
			token.erase(0, pos_bis + 1);
			token.erase(remove(token.begin(), token.end(), '\''), token.end());
			token.erase(remove(token.begin(), token.end(), ';'), token.end());
			token.erase(remove(token.begin(), token.end(), ' '), token.end());
			this->values.push_back(token);
		}
		s.erase(0, pos + 1);
	}

	while ((pos = s.find('=')) != std::string::npos)
	{
		token = s.substr(0, pos);
		if (token.find('.') != std::string::npos)
		{
			token = token.substr(token.find('.') + 1);
		}
		token.erase(remove(token.begin(), token.end(), '\''), token.end());
		token.erase(remove(token.begin(), token.end(), ';'), token.end());
		token.erase(remove(token.begin(), token.end(), ' '), token.end());
		this->values.push_back(token);
		s.erase(0, pos + 1);
		s.erase(remove(s.begin(), s.end(), '\''), s.end());
		s.erase(remove(s.begin(), s.end(), ';'), s.end());
		s.erase(remove(s.begin(), s.end(), ' '), s.end());
		this->values.push_back(s);
	}
}