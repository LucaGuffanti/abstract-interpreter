#ifndef INTERVAL_HPP
#define INTERVAL_HPP

#include <limits>

template <typename T>
class Interval{
private:
    T m_lb;
    T m_ub;

    static const T min_T = std::numeric_limits<T>::min();
    static const T max_T = std::numeric_limits<T>::max();

public: 
    // Default constructor
    Interval():
    m_lb(0),
    m_ub(0)
    {}    

    // Constructor with two values
    Interval(const T& lb, const T& ub)
    : m_lb(lb)
    , m_ub(ub)
    {}

    // Copy constructor
    Interval(const Interval<T>& other)
    : m_lb(other.m_lb)
    , m_ub(other.m_ub)
    {}

    // Default destructor
    ~Interval() = default;

    // Getters and Setters
    T& lb() { return m_lb; }
    T& ub() { return m_ub; }


    // ===============================================================================
    // LATTICE OPERATIONS
    // ===============================================================================

    // Join operation - Union of the two intervals
    void join(Interval<T>& other)
    {
        m_lb = std::min(m_lb, other.lb());
        m_ub = std::max(m_ub, other.ub());
    }

    // Meet operation - Intersection of the two intervals
    void meet(Interval<T>& other)
    {
        m_lb = std::max(m_lb, other.lb());
        m_ub = std::min(m_ub, other.ub());
    }

    // Equality operator overloading to compare two intervals
    bool operator==(Interval<T>& other) const
    {
        return m_lb == other.lb() && m_ub == other.ub();
    }

    // Less than operator overloading to compare two intervals. The less than operator
    // introduces a partial order, and is here used with the semantics of the inclusion
    bool operator<(Interval<T>& other) const
    {
        return m_lb > other.lb() && m_ub < other.ub();
    }


    // ===============================================================================
    // ABSTRACT SEMANTICS OF EXPRESSIONS
    // ===============================================================================

    Interval<T> constant_interval(T& value) const
    {
        return Interval<T>(value, value);
    }

    Interval<T> operator+(Interval<T>& other) const
    {
        if (m_lb > max_T - other.ub() || m_ub > max_T - other.lb())
        {
            std::cerr << "Overflow Encountered in evaluating addition" << std::endl;
        }
        return Interval<T>(m_lb + other.lb(), m_ub +  other.ub());
    }

    Interval<T> operator-(Interval<T>& other) const
    {
        if (m_lb < min_T + other.ub() || m_ub < min_T + other.lb())
        {
            std::cerr << "Overflow Encountered in evaluating subtraction" << std::endl;
        }
        return Interval<T>(m_lb - other.ub(), m_ub - other.lb());
    }

    Interval<T> operator-() const 
    {
        if (m_lb == min_T)
        {
            std::cerr << "Overflow Encountered in evaluating negation" << std::endl;
        }
        return Interval<T>(-m_ub, -m_lb);
    }

    Interval<T> operator*(Interval<T>& other) const
    {
        
        if (m_lb > max_T / other.ub() || m_ub > max_T / other.lb())
        {
            std::cerr << "Overflow Encountered in evaluating multiplication" << std::endl;
        }

        T other_lb = other.lb();
        T other_ub = other.ub();
        T new_lb = std::min({m_lb * other_lb, m_lb * other_ub, m_ub * other_lb, m_ub * other_ub});
        T new_ub = std::max({m_lb * other_lb, m_lb * other_ub, m_ub * other_lb, m_ub * other_ub});
        
        return Interval<T>(new_lb, new_ub);
    }

    Interval<T> operator/(Interval<T>& other) const
    {

        T other_lb = other.lb();
        T other_ub = other.ub();
        if (other_lb <= 0 && other_ub >= 0)
        {
            return Interval<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        }
        else
        {
            T new_lb = std::min({m_lb / other_lb, m_lb / other_ub, m_ub / other_lb, m_ub / other_ub});
            T new_ub = std::max({m_lb / other_lb, m_lb / other_ub, m_ub / other_lb, m_ub / other_ub});
            return Interval<T>(new_lb, new_ub);
        }
    }

    Interval<T> normalize() const
    {
        return Interval<T>(std::min(m_lb, m_ub), std::max(m_lb, m_ub));
    }

    bool contains(Interval<T>& other) const
    {
        return m_lb <= other.lb() && m_ub >= other.ub();
    }

    bool contains(const T& value) const
    {
        return m_lb <= value && m_ub >= value;
    }

};

#endif // INTERVAL_HPP