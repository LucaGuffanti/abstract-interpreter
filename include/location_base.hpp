#ifndef LOCATION_BASE_HPP
#define LOCATION_BASE_HPP

#include <functional>
#include "interval_store.hpp"
#include "parser.hpp"

template <typename T>
class Location
{

public:
    std::function<void(void)> m_operation;
    std::shared_ptr<Location<T>> m_fallback_location;

    bool ends_if_body = false;
    bool ends_else_body = false;
    // TODO: eventually add te that for while

    Location() = default;
    virtual ~Location() = default;

    virtual void print() const {
        std::cout << "Abstract Location" << std::endl;
    };

    virtual std::shared_ptr<IntervalStore<T>> get_last_store() const {
        return nullptr;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) {
        return;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_if_body_store() const 
    {
        return nullptr;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_else_body_store() const
    {
        return nullptr;
    }

    virtual void set_final_if_body_store(std::shared_ptr<IntervalStore<T>> store)
    {
        return;
    }

    virtual void set_final_else_body_store(std::shared_ptr<IntervalStore<T>> store)
    {
        return;
    }

};

template <typename T>
class AssignmentLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before;
    std::shared_ptr<IntervalStore<T>> m_store_after;
    ASTNode m_code_block;

    virtual void print() const override
    {
        std::cout << "Store before" << std::endl;
        if (m_store_before != nullptr)
        {
            m_store_before->print();
        }
        else
        {
            std::cout << "Empty" << std::endl;
        }

        if (m_store_after != nullptr)
        {
            std::cout << "Store after" << std::endl;
            m_store_after->print();
        } 
        else
        {
            std::cout << "Empty" << std::endl;
        }
    }

    virtual std::shared_ptr<IntervalStore<T>> get_last_store() const override
    {
        return m_store_after;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        std::cout << "Assigned store before" << std::endl;
        m_store_before = store;
    }

};


template <typename T>
class PostConditionLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store;
    ASTNode m_code_block;

    virtual void print() const override
    {
        std::cout << "Postcondition" << std::endl;
        m_store->print();
    }

    virtual std::shared_ptr<IntervalStore<T>> get_last_store() const override
    {
        return m_store;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store = store;
    }
};

template <typename T>
class IfElseLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before_condition;
    std::shared_ptr<IntervalStore<T>> m_store_if_body;
    std::shared_ptr<IntervalStore<T>> m_store_else_body;
    ASTNode m_code_block;

    virtual void print() const override
    {
        std::cout << "If-else location" << std::endl;
        std::cout << "Store before condition" << std::endl;
        if (m_store_before_condition != nullptr) {
            m_store_before_condition->print();
        }
        else
        {
            std::cout << "Empty" << std::endl;
        }

        std::cout << "Store if body" << std::endl;
        if (m_store_if_body != nullptr) {
            m_store_if_body->print();
        }
        else
        {
            std::cout << "Empty" << std::endl;
        }
        std::cout << "Store else body" << std::endl;
        if (m_store_else_body != nullptr) {
            m_store_else_body->print();
        }
        else
        {
            std::cout << "Empty" << std::endl;
        }
    }

    virtual std::shared_ptr<IntervalStore<T>> get_if_body_store() const override
    {
        return m_store_if_body;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_else_body_store() const override
    {
        return m_store_else_body;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_before_condition = store;
    }
};

template<typename T>
class EndIfLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before;
    std::shared_ptr<IntervalStore<T>> m_store_after_body;
    std::shared_ptr<IntervalStore<T>> m_store_after_else;
    std::shared_ptr<IntervalStore<T>> m_store_after;

    virtual void print() const override
    {
        std::cout << "After If Store" << std::endl;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_last_store() const override
    {
        return m_store_after;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_before = store;
    }

    virtual void set_final_if_body_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_after_body = store;
    }

    virtual void set_final_else_body_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_after_else = store;
    }
};

template <typename T>
class WhileLocation : public Location<T>
{
public:

};

template <typename T>
class EndWhileLocation : public Location<T>
{
public:

};

#endif // LOCATION_BASE_HPP