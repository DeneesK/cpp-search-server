## SearchServer

SearchServer - search engine for documents taking into account negative keywords (documents with these words are not taken into account in search results). The principle of operation is close to search engines from large IT giants (Yandex).

### Main functions:
- ranking search results according to the TF-IDF statistical measure;
- processing of stop words (not taken into account by the search engine and do not affect search results);
- processing of negative keywords (documents containing negative keywords will not be included in search results);
- creating and processing a request queue;
- removal of duplicate documents;
- pagination of search results;
- the ability to work in multithreaded mode;

### Usage:
The code is covered with tests. Tests will help you understand how it works.

### System requirements
-C++17 (STL)
-GCC (MinGW-w64)

### Improvement plans:
-Add the ability to enter documents using a file.
-Implement a graphical application using Qt.
