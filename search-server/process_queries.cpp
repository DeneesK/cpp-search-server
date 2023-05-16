#include "process_queries.h"

#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> res(queries.size());
    std::transform(std::execution::par ,queries.begin(), queries.end(), res.begin(),
                  [&search_server](const std::string& q){return search_server.FindTopDocuments(q);});

    return res;
}

// std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    
//     std::vector<std::vector<Document>> documents = ProcessQueries(search_server, queries);

//     std::list<Document> result;

//     std::for_each(documents.begin(), documents.end(),
//                   [&result](std::vector<Document>& v){result.splice(result.begin(), std::list(v.begin(), v.end()));});

//     return result;
// }


std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    
    std::vector<std::vector<Document>> documents = ProcessQueries(search_server, queries);

    size_t size = std::transform_reduce(documents.begin(), documents.end(), 0, std::plus<>{},
                                        [](const std::vector<Document>& v) { return v.size(); });

    std::vector<Document> result(size);

    auto it_begin = result.begin();
    for (auto& doc: documents) {
        it_begin = transform(std::execution::par, std::make_move_iterator(doc.begin()), std::make_move_iterator(doc.end()), 
                             it_begin, [](const Document& value) { return value;});
    }

    return result;
}