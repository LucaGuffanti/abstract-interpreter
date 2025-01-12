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
    ASTNode m_code_block;


    bool ends_if_body = false;
    bool ends_else_body = false;
    bool ends_while_body = false;

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

    virtual void set_final_while_body_store(std::shared_ptr<IntervalStore<T>> store)
    {
        return;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_while_body_store() const
    {
        return nullptr;
    }

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const
    {
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const
    {
        return nullptr;
    }
};

template <typename T>
class AssignmentLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before;
    std::shared_ptr<IntervalStore<T>> m_store_after;

    virtual void print() const override
    {
        std::cout << "(ASSIGNMENT LOCATION)" << std::endl;
        std::cout << "Store before assignment" << std::endl;
        if (m_store_before != nullptr)
        {
            m_store_before->print();
        }
        else
        {
            std::cout << "Empty" << std::endl;
        }

        std::cout << "Store after assignment" << std::endl;
        if (m_store_after != nullptr)
        {
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
        m_store_before = store;
    }

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        if (auto old = dynamic_cast<AssignmentLocation<T>*>(old_location.get()))
        {
            if (old != nullptr)
            {
                return m_store_after->equals(*old->m_store_after);
            }
        }
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<AssignmentLocation<T>>();
        if (m_store_before != nullptr)
        {
            copy->m_store_before = std::make_shared<IntervalStore<T>>(*m_store_before);
        }
        else
        {
            copy->m_store_before = nullptr;
        }
    
        if (m_store_after != nullptr)
        {
            copy->m_store_after = std::make_shared<IntervalStore<T>>(*m_store_after);
        }
        else 
        {
            copy->m_store_after = nullptr;
        }
        return copy;
    }
};


template <typename T>
class PostConditionLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store;

    virtual void print() const override
    {
        std::cout << "(POSTCONDITION LOCATION)" << std::endl;
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

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        return true;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<PostConditionLocation<T>>();
        if (m_store != nullptr)
        {
            copy->m_store = std::make_shared<IntervalStore<T>>(*m_store);
        }
        else
        {
            copy->m_store = nullptr;
        }
        return copy;
    }
};

template <typename T>
class IfElseLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before_condition;
    std::shared_ptr<IntervalStore<T>> m_store_if_body;
    std::shared_ptr<IntervalStore<T>> m_store_else_body;

    virtual void print() const override
    {
        std::cout << "(IF-ELSE LOCATION)" << std::endl;
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

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        if (auto old = dynamic_cast<IfElseLocation<T>*>(old_location.get()))
        {
            return m_store_if_body->equals(*old->m_store_if_body) && m_store_else_body->equals(*old->m_store_else_body);
        }
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<IfElseLocation<T>>();
        if (m_store_before_condition != nullptr)
        {
            copy->m_store_before_condition = std::make_shared<IntervalStore<T>>(*m_store_before_condition);
        }
        else
        {
            copy->m_store_before_condition = nullptr;
        }

        if (m_store_if_body != nullptr)
        {
            copy->m_store_if_body = std::make_shared<IntervalStore<T>>(*m_store_if_body);
        }
        else
        {
            copy->m_store_if_body = nullptr;
        }

        if (m_store_else_body != nullptr)
        {
            copy->m_store_else_body = std::make_shared<IntervalStore<T>>(*m_store_else_body);
        }
        else
        {
            copy->m_store_else_body = nullptr;
        }
        return copy;
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
        std::cout << "(END-IF LOCATION)" << std::endl;

        std::cout << "Store after body" << std::endl;
        if (m_store_after_body != nullptr) {
            m_store_after_body->print();
        } else {
            std::cout << "Empty" << std::endl;
        }

        std::cout << "Store after else" << std::endl;
        if (m_store_after_else != nullptr) {
            m_store_after_else->print();
        } else {
            std::cout << "Empty" << std::endl;
        }

        std::cout << "Store after join" << std::endl;
        if (m_store_after != nullptr) {
            m_store_after->print();
        } else {
            std::cout << "Empty" << std::endl;
        }
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

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        if (auto old = dynamic_cast<EndIfLocation<T>*>(old_location.get()))
        {
            return m_store_after_body->equals(*old->m_store_after_body) && m_store_after_else->equals(*old->m_store_after_else);
        }
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<EndIfLocation<T>>();
        if (m_store_before != nullptr) {
            copy->m_store_before = std::make_shared<IntervalStore<T>>(*m_store_before);
        } else {
            copy->m_store_before = nullptr;
        }

        if (m_store_after_body != nullptr) {
            copy->m_store_after_body = std::make_shared<IntervalStore<T>>(*m_store_after_body);
        } else {
            copy->m_store_after_body = nullptr;
        }

        if (m_store_after_else != nullptr) {
            copy->m_store_after_else = std::make_shared<IntervalStore<T>>(*m_store_after_else);
        } else {
            copy->m_store_after_else = nullptr;
        }

        if (m_store_after != nullptr) {
            copy->m_store_after = std::make_shared<IntervalStore<T>>(*m_store_after);
        } else {
            copy->m_store_after = nullptr;
        }
        return copy;
    }
};

template <typename T>
class WhileLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_before_condition;
    std::shared_ptr<IntervalStore<T>> m_store_body;
    std::shared_ptr<IntervalStore<T>> m_store_exit;

    virtual void print() const override
    {
        std::cout << "(WHILE LOCATION)" << std::endl;
        if (m_store_before_condition != nullptr) {
            std::cout << "Store before condition" << std::endl;
            m_store_before_condition->print();
        } else {
            std::cout << "Empty" << std::endl;
        }

        if (m_store_body != nullptr) {
            std::cout << "Store after condition" << std::endl;
            m_store_body->print();
        } else {
            std::cout << "Empty" << std::endl;
        }

        if (m_store_exit != nullptr) {
            std::cout << "Store exit condition" << std::endl;
            m_store_exit->print();
        } else {
            std::cout << "Empty" << std::endl;
        }
    }
    
    virtual std::shared_ptr<IntervalStore<T>> get_while_body_store() const override
    {
        m_store_body->print();
        return m_store_body;
    }

    virtual std::shared_ptr<IntervalStore<T>> get_exit_store() const
    {
        return m_store_exit;
    }

    virtual void set_previous_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_before_condition = store;
    }

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        if (auto old = dynamic_cast<WhileLocation<T>*>(old_location.get()))
        {
            return m_store_body->equals(*old->m_store_body) && m_store_exit->equals(*old->m_store_exit);
        }
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<WhileLocation<T>>();
        if (m_store_before_condition != nullptr) {
            copy->m_store_before_condition = std::make_shared<IntervalStore<T>>(*m_store_before_condition);
        } else {
            copy->m_store_before_condition = nullptr;
        }

        if (m_store_body != nullptr) {
            copy->m_store_body = std::make_shared<IntervalStore<T>>(*m_store_body);
        } else {
            copy->m_store_body = nullptr;
        }

        if (m_store_exit != nullptr) {
            copy->m_store_exit = std::make_shared<IntervalStore<T>>(*m_store_exit);
        } else {
            copy->m_store_exit = nullptr;
        }
        return copy;
    }

};

template <typename T>
class EndWhileLocation : public Location<T>
{
public:
    std::shared_ptr<IntervalStore<T>> m_store_from_while;
    std::shared_ptr<IntervalStore<T>> m_store_after;

    virtual void print() const override
    {
        std::cout << "(END WHILE LOCATION)" << std::endl;
        if (m_store_from_while != nullptr) {
            std::cout << "Store from while" << std::endl;
            m_store_from_while->print();
        } else {
            std::cout << "Empty" << std::endl;
        }

        if (m_store_after != nullptr) {
            std::cout << "Store after" << std::endl;
            m_store_after->print();
        } else {
            std::cout << "Empty" << std::endl;
        }
    }

    virtual void set_final_while_body_store(std::shared_ptr<IntervalStore<T>> store) override
    {
        m_store_from_while = store;
    }

    virtual bool is_stable(std::unique_ptr<Location<T>>& old_location) const override
    {
        if (auto old = dynamic_cast<EndWhileLocation<T>*>(old_location.get()))
        {
            return m_store_after->equals(*old->m_store_after);
        }
        return false;
    }

    virtual std::unique_ptr<Location<T>> copy() const override
    {
        auto copy = std::make_unique<EndWhileLocation<T>>();
        if (m_store_from_while != nullptr) {
            copy->m_store_from_while = std::make_shared<IntervalStore<T>>(*m_store_from_while);
        } else {
            copy->m_store_from_while = nullptr;
        }
        if (m_store_after != nullptr) {
            copy->m_store_after = std::make_shared<IntervalStore<T>>(*m_store_after);
        } else {
            copy->m_store_after = nullptr;
        }
        return copy;
    }
};

#endif // LOCATION_BASE_HPP