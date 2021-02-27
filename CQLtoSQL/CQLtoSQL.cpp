#include <string>
#include <iostream>
#include <vector>

#include "InputQuery.h"

//include toutes les classes

using namespace std;
using std::string;

string incoming_cql_query;
string DML_statements[] = { "SELECT ", "INSERT ", "DELETE ", "UPDATE "};
string translated_sql_query;

string create_select_sql_query(string _table, string _key, vector<string> _fields)
{
    //Pour une requête SELECT, on va prendre toutes les données des deux colonnes de la table où le column_name correspond au champ voulu
    //On considère que le nom de la table est juste la key CQL, mais on pourrait imaginer une concaténation de la table CQL et de la key CQL -> le paramètre _table n'est pas utilisé ici
    //Pour la mise en forme de la réponse au format CQL, on va faire ça au retour du RWCS, siinon la requête d'envoi (ici) aurait été trop complexe

    string returned_select_sql_query = "SELECT * FROM " + _key;
    //On regarde si, dans la requête CQL, on voulait prendre * ou des champs particuliers
    if (_fields[0] != "*")
    {
        returned_select_sql_query += " WHERE 'column_name' IN (";
        for (string field : _fields)
        {
            returned_select_sql_query += field + ", ";
        }
        returned_select_sql_query = returned_select_sql_query.substr(0, returned_select_sql_query.length() - 2);
        returned_select_sql_query += ");";
    }
    return returned_select_sql_query;
}

string create_update_sql_query(string _table, string _key, vector<string> _values)
{
    //Comme on update uniquement la colonne value d'une table, il faut utiliser la clause CASE sur la valeur de la colonne column_name
    //Dans _values, les éléments "impairs" (le premier, troisième,...) sont les colonnes à update et les "pairs" sont les valeurs de ces colonnes
    string returned_update_sql_query = "UPDATE " + _key + " SET value = ";
    //On regarde si on a plusieurs champs à update
    if (_values.size() > 2)
    {
        returned_update_sql_query += "(CASE ";
        for (int i = 0; i < _values.size(); i = i + 2)
        {
            //On CASE sur chaque colonne CQL, càd chaque valeur que peut prendre la colonne column_name
            returned_update_sql_query += "when column_name = '" + _values[i] + "' then '" + _values[i+1] + "' ";
        }
        returned_update_sql_query += "END) WHERE column_name IN ('";
        for (int i = 0; i < _values.size(); i = i + 2)
        {
            returned_update_sql_query += _values[i] + "', ";
        }
        returned_update_sql_query = returned_update_sql_query.substr(0, returned_update_sql_query.length() - 3);
        returned_update_sql_query += "');";
    }
    else
    {
        returned_update_sql_query += "'" + _values[0] + "' WHERE column_name = '" + _values[1] + "';";
    }
    return returned_update_sql_query;
}

string create_insert_sql_query(string _table, string _key, vector<string> _columns,  vector<string> _values)
{
    string returned_insert_sql_query =  "START TRANSACTION; "
                                        "CREATE TABLE " + _key + " (column_name string, value string); "
                                        "INSERT INTO " + _key + " (column_name, value) VALUES ";
    for (int i = 0; i < _values.size(); i++)
    {
        returned_insert_sql_query += "(" + _columns[i] + ", '" + _values[i] + "'), ";
    }
    returned_insert_sql_query = returned_insert_sql_query.substr(0, returned_insert_sql_query.length() - 2);
    returned_insert_sql_query +=    "; "
                                    "COMIT;";

    return returned_insert_sql_query;
}

string create_delete_sql_query(string _table, string _key)
{
    string returned_delete_sql_query = "DROP TABLE " + _key + " ;";

    return returned_delete_sql_query;
}

string CQLtoSQL(string _incoming_cql_query)
{
    //On en crée un objet InputQuery
    InputQuery query_to_parse = InputQuery(_incoming_cql_query);

    //On crée un objet du type de la requête CQL entrante
    //Si requête SELECT:
    if (query_to_parse.get_initial_query().find("SELECT ") != std::string::npos)
    {
        cout << "Type de la requete : SELECT" << endl;
        SelectStatement select_statement = SelectStatement(_incoming_cql_query);
            //On génère les clauses de la requête
            SelectClause select_clause = select_statement.generate_select_clause();
            FromClause from_clause = select_statement.generate_from_clause();
            WhereClause where_clause = select_statement.generate_where_clause();
                //On extrait les paramètres des clauses
                string table = from_clause.get_table();
                string key = where_clause.get_key();
                vector<string> fields = select_clause.get_fields();
        //result

        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << " Fields : ";
        for (string field : fields)
        {
            cout << field << " __ ";
        }
        cout << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_select_sql_query(table, key, fields);
    }

    //Si requête SCAN:
    if (query_to_parse.get_initial_query().find("SCAN ") != std::string::npos)
    {
        cout << "Type de la requete : SCAN" << endl;

    }

    //Si requête UPDATE:
    if (query_to_parse.get_initial_query().find("UPDATE ") != std::string::npos)
    {
        cout << "Type de la requete : UPDATE" << endl;
        UpdateStatement update_statement = UpdateStatement(_incoming_cql_query);
            //On génère les clauses de la requête
            UpdateClause update_clause = update_statement.generate_update_clause();
            SetClause set_clause = update_statement.generate_set_clause();
            WhereClause where_clause = update_statement.generate_where_clause();
                //On extrait les paramètres des clauses
                string table = update_clause.get_table();
                string key = where_clause.get_key();
                vector<string> values = set_clause.get_values();

        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << " Fields : ";
        for (int i = 0; i < values.size(); i = i + 2)
        {
            cout << values[i] << " => " << values[i+1] << " __ ";
        }
        cout << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_update_sql_query(table, key, values);
    }

    //Si requête INSERT:
    if (query_to_parse.get_initial_query().find("INSERT ") != std::string::npos)
    {
        cout << "Type de la requete : INSERT" << endl;
        InsertStatement insert_statement = InsertStatement(_incoming_cql_query);
            //On génère les clauses de la requête
            InsertIntoClause insert_into_clause = insert_statement.generate_insert_into_clause();
            ValuesClause values_clause = insert_statement.generate_values_clause();
                //On extrait les paramètres des clauses
                string table = insert_into_clause.get_table();
                string key = values_clause.get_key();
                vector<string> columns = insert_into_clause.get_columns();
                vector<string> values = values_clause.get_values();

        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << " Fields : ";
        for (int i = 0; i < columns.size(); i++)
        {
            cout << columns[i] << " => " << values[i] << " __ ";
        }
        cout << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_insert_sql_query(table, key, columns, values);
    }

    //Si requête DELETE:
    if (query_to_parse.get_initial_query().find("DELETE ") != std::string::npos)
    {
        cout << "Type de la requete : DELETE" << endl;
        DeleteStatement delete_statement = DeleteStatement(_incoming_cql_query);
            //On génère les clauses de la requête
            DeleteClause delete_clause = delete_statement.generate_delete_clause();
            WhereClause where_clause = delete_statement.generate_where_clause();
                //On extrait les paramètres des clauses
                string table = delete_clause.get_table();
                string key = where_clause.get_key();
                
        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_delete_sql_query(table, key);
    }
}

int main()
{
    //On reçoit la requête CQL entrante
    //cin >> incoming_cql_query;
    //cout << incoming_cql_query << endl;

    //incoming_cql_query = "SELECT * FROM table WHERE id = 'key' LIMIT 1;";
    //incoming_cql_query = "SELECT* FROM table WHERE 'id' >= token(startkey) LIMIT recordcount;";
    //incoming_cql_query = "UPDATE table SET colonne_1 = 'valeur_1', colonne_2 = 'valeur 2', colonne_3 = 'valeur 3' WHERE 'id' = key;";
    incoming_cql_query = "INSERT INTO table (nom_colonne_1, nom_colonne_2, ...) VALUES ('valeur_1', 'valeur_2', ...);";
    //incoming_cql_query = "DELETE FROM table WHERE id = 'key';";

    //On la parse et on la convertit pour le RWCS
    translated_sql_query = CQLtoSQL(incoming_cql_query);
    cout << translated_sql_query << endl;
}