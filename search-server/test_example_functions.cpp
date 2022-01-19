#include "search_server.h"
#include "test_example_functions.h"
#include "document.h"
#include <vector>
#include <string>

using namespace std;

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const invalid_argument & e)
    {
        cout << "������ ���������� ��������� "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query)
{
    cout << "���������� ������ �� �������: "s << raw_query << endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            cout << document;
        }
    }
    catch (const invalid_argument & e)
    {
        cout << "������ ������: "s << e.what() << endl;
    }
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status)
{
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words)
    {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void MatchDocuments(const SearchServer& search_server, const string& query)
{
    try
    {
        cout << "������� ���������� �� �������: "s << query << endl;
        for (const int document_id : search_server)
        {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const invalid_argument & e)
    {
        cout << "������ �������� ���������� �� ������ "s << query << ": "s << e.what() << endl;
    }
}