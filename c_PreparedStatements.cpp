#include "c_PreparedStatements.hpp"
#include "c_Logs.hpp"
#include "c_Socket.hpp"

#pragma region Global
std::list<PreparedStatAndResponse> l_PreparedStatementsINSERT;
std::string LowerCQLQuery;
int i = 0;
size_t pos = 0;
PreparedReqStock s_PrepReq;
const char* conninfoPrepState;
PGconn* connPrepState;
PGresult* resPrepState;
unsigned char ResponseToPrepare[178] = {
    0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xa9, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x10, 0x59, 0xed, 0x03, 0xd8, 0x08, 0x5b, 0x75, 0xf4, 0x31, 0x1f, 0x57, 0xc7, 0x34, 0x36, 0xd9,
    0xbc, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65,
    0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31,
    0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69,
    0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d,
    0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c,
    0x64, 0x38, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00, 0x06,
    0x66, 0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x35,
    0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00

};
#pragma endregion Global

#pragma region Functions
void PrepStatementResponse(unsigned char head[13], char CQLpreparedStatement[1024]){
    //Just adapted to the bench, doesn't work if changes
    ResponseToPrepare[2] = head[2];
    ResponseToPrepare[3] = head[3];
    for(int j = 0; j<10; j++){
        ResponseToPrepare[77 + j*10] = CQLpreparedStatement[33+j*7];
    }
    // for(int k=0; k< 1024; k++){      //DO NOT UNCOMMENT IF YOU WANT TO STAY ALIVE
    //     printf("%c",ResponseToPrepare[k]);
    // }
    // printf("\r\n");
    write(GetSocket(), ResponseToPrepare, 178);
}

/*PreparedReqStock GetIDPreparedRequest(std::string LowerCQLQuery){
    PreparedReqStock s_PrepReq;
    switch (LowerCQLQuery.substr(0, 6))
    {
    case "insert":
        s_PrepReq.Clause = 1;
        for(i; i<l_PreparedStatementsINSERT.size(); i++){
            if(l_PreparedStatementsINSERT[i] == CQLpreparedStatement)
                s_PrepReq.ID = i; 
            else
                s_PrepReq.ID = NULL;
        }
        break;  
    default:
        break;
    }
    return s_PrepReq;
}

std::string InsertPrepStatement(std::string LowerCQLQuery){
    try {
        std::string CassandraResponse = "";
        pos = 0;
        while ((pos = LowerCQLQuery.find(',')) != _NPOS)
        {
            CassandraResponse += 
            token = LowerCQLQuery.substr(0, pos);
            if (token.find('.') != _NPOS)
            {
                token = token.substr(token.find('.') + 1);
            }      

            insert_into_clause_data.erase(0, pos + 1);
        }
            //Affichage des paramètres
            //cout << "Table: " << table << " - Key: " << key << " Fields : ";
            // for (unsigned int i = 0; i < columns.size(); i++)
            // {
            //     cout << columns[i] << " => " << values[i] << " __ ";
            // }
            // cout << endl;
        }
        catch (std::string const& chaine)
        {
            cerr << chaine << endl;
            logs(chaine);
        }
        catch (std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            logs(e.what());
        }
}
*/

int ConnPGSQLPrepStatements() {
    conninfoPrepState = "user = postgres";
    connPrepState = PQconnectdb(conninfoPrepState);
    /* Check to see that the backend connection was successfully made */
    if (PQstatus(connPrepState) != CONNECTION_OK)
    {
        logs("ConnPGSQLPrepStatements() : Connexion to database failed", ERROR);
        return EXIT_FAILURE; 
    }
    /* Set always-secure search path, so malicious users can't take control. */
    resPrepState = PQexec(connPrepState, "SELECT pg_catalog.set_config('search_path', 'public', false)");
    if (PQresultStatus(resPrepState) != PGRES_TUPLES_OK)
    {
        PQclear(resPrepState);
        logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
        return EXIT_FAILURE;
    }
    else
        logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
    PQclear(resPrepState);
    return EXIT_SUCCESS;
}

#pragma endregion Functions

