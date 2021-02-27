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
	string initial_query;					//Texte de la requête cql entrante

public:
	InputQuery(string);						//Constructeur de l'objet
	~InputQuery();							//Destructeur de l'objet

	//GET
	string get_initial_query();				//Retourne la propriété string 'initial_query' de l'objet
	//...
	
	void show_full_arborescence();			//Affiche l'arborescence complète
	void generate_dml_statement(string);	//Crée un objet du type de la requête cql entrante
};