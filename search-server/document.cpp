#include "document.h"

std::ostream& operator<<(std::ostream& os, const Document& doc) {
    os << "{ " << "document_id = " << doc.id << ", " << "relevance = " << doc.relevance << ", " << "rating = " << doc.rating << " }";
    return os;
}

Document::Document(int id, double relevance, int rating): id(id), relevance(relevance), rating(rating) 
{
}