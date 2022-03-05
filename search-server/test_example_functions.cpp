#include "search_server.h"
#include "test_example_functions.h"
#include "document.h"

using namespace std;

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
    const vector<int>& ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const invalid_argument & e)
    {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, string_view raw_query)
{
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            cout << document;
        }
    }
    catch (const invalid_argument & e)
    {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status)
{
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (string_view word : words)
    {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void MatchDocuments(const SearchServer& search_server, string_view query)
{
    try
    {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const int document_id : search_server)
        {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const invalid_argument & e)
    {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}
