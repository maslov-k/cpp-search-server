#include "document.h"

using namespace std;

Document::Document() = default;
Document::Document(int id0, double relevance0, int rating0)
	: id(id0), relevance(relevance0), rating(rating0)
{
}

ostream& operator<<(ostream& output, const Document& document)
{
	output << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating
		<< " }"s;
	return output;
}