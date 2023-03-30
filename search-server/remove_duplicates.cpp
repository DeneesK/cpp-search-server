#include "remove_duplicates.h"
#include "document.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    map<set<string>, int> origin;
    vector<int> to_delete;
    for(auto id_: search_server) {
        auto words = search_server.GetWordsById(id_);
        if(origin.count(words)) {
            to_delete.push_back(id_);
            continue;
        }
        origin[words] = id_;
    }

    for(int i: to_delete) {
        cout << "Found duplicate document id " << i << endl;
        search_server.RemoveDocument(i);
    }
}