#include "process_queries.h"

#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> output(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), output.begin(),
        [&search_server](const std::string& single_query) {return search_server.FindTopDocuments(single_query); });
    return output;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> temp = ProcessQueries(search_server, queries);
    std::vector<Document> output;
    for (std::vector<Document> single_query_result : temp)
    {
        output.insert(output.end(), single_query_result.begin(), single_query_result.end());
    }
    return output;
}