#ifndef INTERVAL_STORE_HPP
#define INTERVAL_STORE_HPP

#include <map>
#include <string>
#include <iterator>

#include "interval.hpp"

template <typename T>
class IntervalStore
{
private:
    std::map<std::string, Interval<T>> m_store;

public:
    IntervalStore() = default;
    ~IntervalStore() = default;
    IntervalStore(const IntervalStore<T>& other)
    {
        for (const auto& [key, interval] : other.m_store)
        {
            m_store[key] = interval;
        }
    }

    void set(const std::string& var, const Interval<T>& interval)
    {
        m_store[var] = interval;
    }

    Interval<T>& get(const std::string& var)
    {
        return m_store[var];
    }

    void joinAll(IntervalStore<T>& other)
    {
        for (auto& interval_mapping : other.store())
        {
            auto var = interval_mapping.first;
            auto interval = interval_mapping.second;
            if (auto it = m_store.find(var); it != m_store.end())
            {
                it->second.join(interval);
            }
            else
            {
                m_store[var] = interval;
            }
        }
    }


    std::map<std::string, Interval<T>>& store() 
    {
        return this->m_store;
    }

    void print()
    {
        for (auto& [key, interval] : m_store)
        {
            std::cout << key << ": [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
        }
    }

};
#endif // INTERVAL_STORE_HPP