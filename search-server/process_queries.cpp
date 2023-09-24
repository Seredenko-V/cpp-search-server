#include "process_queries.h"

#include <iostream>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
	vector<vector<Document>> results_queries(queries.size());
	transform(execution::par, queries.begin(), queries.end(), results_queries.begin(),
		[&search_server](const string& query) {
			return search_server.FindTopDocuments(query); 
		});
	return results_queries;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
	vector<vector<Document>> all_documents = ProcessQueries(search_server, queries);
	size_t number_all_documents = 0;
	list<Document> documents;
	for (const vector<Document>& documents_one_query : all_documents) {
		number_all_documents += documents_one_query.size();
		for (const Document& document : documents_one_query) {
			documents.push_back(document);
		}
	}
	return documents;
}
