#include <string>

#include "SelectStatement.h"
#include "InsertStatement.h"
#include "UpdateStatement.h"
#include "DeleteStatement.h"

using std::string;

#pragma once
class InputQuery
{
private:
	string initial_query;					//Texte de la requ�te cql entrante

public:
	InputQuery(string);						//Constructeur de l'objet
	~InputQuery();							//Destructeur de l'objet

	//GET
	string get_initial_query();				//Retourne la propri�t� string 'initial_query' de l'objet
	//...
	
	void show_full_arborescence();			//Affiche l'arborescence compl�te
	void generate_dml_statement(string);	//Cr�e un objet du type de la requ�te cql entrante
};