#pragma once
#include <vector>
#include <numeric>
#include <iterator>
#include <cmath>
#include <cassert>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange() = default;
    explicit IteratorRange(Iterator begin_page, Iterator end_page)
        : begin_page_(begin_page), end_page_(end_page) {
    }
    Iterator begin() const {
        return begin_page_;
    }
    Iterator end() const {
        return end_page_;
    }
    size_t size() const {
        return distance(begin_page_, end_page_);
    }
private:
    Iterator begin_page_;
    Iterator end_page_;
};

// создает вектор пар итераторов (начало и конец каждой страницы)
template <typename Iterator>
class Paginator {
public:
    explicit Paginator(Iterator begin_documents, Iterator end_documents, size_t page_size) {
        size_t quantity_pages = static_cast<size_t>(std::ceil(static_cast<double>(distance(begin_documents,
            end_documents)) / page_size));
        pages_.reserve(quantity_pages);
        // до (quantity_pages - 1) т.к. последняя страница может быть занята не полностью
        for (size_t i = 0; i < quantity_pages - 1; ++i) {
            assert(end_documents >= begin_documents && page_size > 0); // чтобы избежать возможного зацикливания
            pages_.push_back(IteratorRange(begin_documents, begin_documents + page_size));
            advance(begin_documents, page_size);
        }
        pages_.push_back(IteratorRange(begin_documents, end_documents)); // заполнение последней страницы
    }
    // можно заменить тип возвращаемого значения на auto
    typename std::vector<IteratorRange<Iterator>>::const_iterator begin() const {
        return pages_.begin();
    }
    // вот так
    auto end() const {
        return pages_.end();
    }
private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& container, size_t page_size) {
    return Paginator(begin(container), end(container), page_size);
}

template <typename Iterator>
std::ostream& operator <<(std::ostream& output, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        output << *it;
    }
    return output;
}
