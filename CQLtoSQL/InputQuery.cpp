#include "InputQuery.h"

using namespace std;

InputQuery::InputQuery(string _initial_query) 
{
	this->initial_query = _initial_query;
}

InputQuery::~InputQuery()
{

}

string InputQuery::get_initial_query() 
{
	return this->initial_query;
}

void InputQuery::show_full_arborescence()	//...
{

}

void InputQuery::generate_dml_statement(string _dml)	//...
{

}