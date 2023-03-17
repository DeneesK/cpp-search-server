#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator range_begin, Iterator range_end): begin_(range_begin), end_(range_end)
    {

    }

    auto begin() const {
        return begin_;
    }

    auto end() const {
        return end_;
    }

    friend std::ostream& operator<< (std::ostream &os, const IteratorRange& page) {
        for (auto iter = page.begin(); iter < page.end(); ++iter) {
            os << *iter;
        }
        return os;
    }

private:
    Iterator begin_;
    Iterator end_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, size_t page_size): page_size_(page_size) {
        for(auto it = range_begin; it != range_end;) {
            int dif = std::distance(it + page_size, range_end);
            if(dif > 0 && dif < distance(range_begin, range_end)) {
                pages.push_back(IteratorRange(it, it + page_size));
                it += page_size;
            } else {
                pages.push_back(IteratorRange(it, range_end));
                break;
            }
        }
    }
    
    auto begin() const {
        return pages.begin();
    }

    auto end() const {
        return pages.end();
    }
private:
    size_t page_size_; 
    std::vector<IteratorRange<Iterator>> pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}