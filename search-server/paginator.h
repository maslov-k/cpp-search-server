#pragma once
#include <iostream>
#include <vector>
#include <algorithm>


template <typename It>
class IteratorRange
{
private:
	It page_begin_;
	It page_end_;
public:
	explicit IteratorRange(It page_begin, It page_end)
		: page_begin_(page_begin), page_end_(page_end)
	{
	}
	It begin() const
	{
		return page_begin_;
	}
	It end() const
	{
		return page_end_;
	}
};

template <typename It>
class Paginator
{
private:
	std::vector<IteratorRange<It>> pages_;
public:
	explicit Paginator(It begin_it, It end_it, size_t page_size)
	{
		It current_page_begin = begin_it;
		It current_page_end;
		int remaining_docs = std::distance(begin_it, end_it);
		while (remaining_docs > 0)
		{
			int docs_to_add = std::min(static_cast<int>(page_size), remaining_docs);
			current_page_end = current_page_begin + docs_to_add;
			pages_.push_back(IteratorRange<It>(current_page_begin, current_page_end));
			remaining_docs -= docs_to_add;
			std::advance(current_page_begin, docs_to_add);
		}
	}
	auto begin() const
	{
		return pages_.begin();
	}
	auto end() const
	{
		return pages_.end();
	}
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size)
{
	return Paginator(begin(c), end(c), page_size);
}

template <typename It>
std::ostream& operator<<(std::ostream& output, const IteratorRange<It>& iterator_range)
{
	for (auto it = iterator_range.begin(); it != iterator_range.end(); ++it)
	{
		output << *it;
	}
	return output;
}