#ifndef ABSTRACT_INTERPRETER_HPP
#define ABSTRACT_INTERPRETER_HPP

#include <limits>
#include <concepts>
#include <map>

#include "parser.hpp"
#include "interval_store.hpp"

template <typename T> requires std::is_arithmetic_v<T>
class AbstractInterpreter
{
private:

    const std::map<std::string, LogicOp> m_logic_op_map = {
        {"<=", LogicOp::LEQ},
        {">=", LogicOp::GEQ},
        {"==", LogicOp::EQ},
        {"!=", LogicOp::NEQ},
        {"<", LogicOp::LE},
        {">", LogicOp::GE}
    };

    const T min_T = std::numeric_limits<T>::min();
    const T max_T = std::numeric_limits<T>::max();

    ASTNode m_ast;
    IntervalStore<T> m_interval_store;
public:
    AbstractInterpreter() = default;
    ~AbstractInterpreter() = default;

    AbstractInterpreter(const std::string& input)
    {
        AbstractInterpreterParser AIParser;
        m_ast = AIParser.parse(input);
    }

    AbstractInterpreter(const ASTNode ast)
    : m_ast(ast)
    {}

    void print()
    {
        m_ast.print();
    }

    void run()
    {
        for (const auto& node : m_ast.children)
        {
            eval(node);
        }
    }

private:
    bool eval(const ASTNode& node)
    {
        switch (node.type)
        {
            case NodeType::DECLARATION:
            {
                bool res = true;
                for (const ASTNode& child: node.children)
                {
                    res = res && evaluate_variable_declaration(child);
                }
                return res;
            }
            case NodeType::SEQUENCE:
            {
                bool res = true;
                for (const ASTNode& child: node.children)
                {
                    res = res && eval(child);
                }
                return res;
            }
            case NodeType::PRE_CON:
            {
                node.print();
                bool res = true;
                for (const auto& child : node.children)
                {
                    res = res && evaluate_precondition_logic_operation(child);
                }
                return res;
            }
            case NodeType::ASSIGNMENT:
            {
                return evaluate_assignment(node);
            }
            case NodeType::POST_CON:
            {
                return evaluate_postcondition(node);
            }
            case NodeType::IFELSE:
            {
                return evaluate_if_else(node);
            }
            default:
            {
                node.print();
                std::cerr << "Unknown node type" << std::endl;
                exit(1);
                break;
            }

        }
        return true;
    }

    bool evaluate_variable_declaration(const ASTNode& node)
    {
        auto var_name = std::get<std::string>(node.value);
        #ifdef DEBUG
        std::cout << "Variable name: " << var_name << std::endl;
        #endif
        
        m_interval_store.set(var_name, {min_T, max_T});

        #ifdef DEBUG
        auto interval = m_interval_store.get(var_name);
        std::cout << "Interval: [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
        #endif
        return true;
    }

    bool evaluate_precondition_logic_operation(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Looking at logic node" << std::endl;
        node.print();
        #endif

        auto op = std::get<std::string>(node.value);
        
        auto left = node.children[0];
        auto right = node.children[1];
        bool variable_on_left = false;

        T val;
        std::string var;
        if (left.type==NodeType::INTEGER && right.type==NodeType::VARIABLE)
        {
            #ifdef DEBUG
            std::cout << "Left is a constant and right is a variable" << std::endl;
            #endif

            val = std::get<T>(left.value);
            var = std::get<std::string>(right.value);
        }
        else if (left.type==NodeType::VARIABLE && right.type==NodeType::INTEGER)
        {
            variable_on_left = true;
            #ifdef DEBUG
            std::cout << "Left is a variable and right is a value" << std::endl;
            #endif
            val = std::get<T>(right.value);
            var = std::get<std::string>(left.value);
        }
        else {
            std::cout << "UNEXPECTED" << std::endl;
            exit(1);
        }

        switch (m_logic_op_map.at(op))
        {
            case LogicOp::LEQ:
            {
                if (variable_on_left)
                {
                    m_interval_store.get(var).ub() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced upper bound of " << var << " to " << val << std::endl;
                    #endif
                }
                else
                {
                    m_interval_store.get(var).lb() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced lower bound of " << var << " to " << val << std::endl;
                    #endif
                }

                break;
            }
            case LogicOp::GEQ:
            {
                if (variable_on_left)
                {
                    m_interval_store.get(var).lb() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced lower bound of " << var << " to " << val << std::endl;
                    #endif
                }
                else
                {
                    m_interval_store.get(var).ub() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced upper bound of " << var << " to " << val << std::endl;
                    #endif
                }
                break;
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                exit(1);
            }
        }

        #ifdef DEBUG
        auto interval = m_interval_store.get(var);
        std::cout << "Interval of variable " << var <<" : [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
        #endif
        return true;
    }

    bool evaluate_assignment(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Evaluating assignment" << std::endl;
        node.print();
        #endif

        auto var = std::get<std::string>(node.children[0].value);
        auto expr = node.children[1];

        auto result = evaluate_expression(expr);
        m_interval_store.get(var) = result;
        // if (respects_precondition(var, result))
        // {
        //     m_interval_store.get(var) = result;
        // }
        // else
        // {
        //     std::cerr << "Assignment does not respect precondition" << std::endl;
        //     return false;
        // }

        #ifdef DEBUG
        std::cout << "Interval of variable " << var <<" : [" << m_interval_store.get(var).lb() << ", " << m_interval_store.get(var).ub() << "]" << std::endl;
        #endif
        return true;
    }

    bool respects_precondition(const std::string& var, Interval<T>& interval)
    {
        return interval.lb() >= m_interval_store.get(var).lb() && interval.ub() <= m_interval_store.get(var).ub();
    }

    bool evaluate_postcondition(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Evaluating postcondition" << std::endl;
        node.print();
        #endif

        auto child = node.children[0];

        auto op = std::get<LogicOp>(child.value);

        auto left_expr = child.children[0];
        auto right_expr = child.children[1];

        auto left_interval = evaluate_expression(left_expr);
        auto right_interval = evaluate_expression(right_expr);

        switch (op)
        {
            case LogicOp::LEQ:
            {
                if (left_interval.ub() <= right_interval.lb())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            case LogicOp::GEQ:
            {
                if (left_interval.lb() >= right_interval.ub())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            case LogicOp::EQ:
            {
                if (left_interval.lb() == right_interval.lb() && left_interval.ub() == right_interval.ub())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            case LogicOp::NEQ:
            {
                if (left_interval.lb() != right_interval.lb() || left_interval.ub() != right_interval.ub())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            case LogicOp::LE:
            {
                if (left_interval.ub() < right_interval.lb())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            case LogicOp::GE:
            {
                if (left_interval.lb() > right_interval.ub())
                {
                    #ifdef DEBUG
                    std::cout << "Postcondition satisfied" << std::endl;
                    #endif
                }
                else
                {
                    std::cerr << "Postcondition not satisfied" << std::endl;
                    exit(1);
                }
                break;
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                exit(1);
            }
        }
        return true;
    }

    bool evaluate_if_else(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Evaluating if else" << std::endl;
        node.print();
        #endif

        auto condition = node.children[0];
        auto if_body = node.children[1]; 
        
        auto op = std::get<LogicOp>(condition.children[0].value);
        std::cout << "operation " << op << std::endl;

        if (op != LogicOp::EQ) {
            std::cerr << "Only equality is supported in if else statements" << std::endl;
            exit(1);
        }

        std::cout << "Looking at if body " << std::endl;
        auto [if_body_interval, changed_var] = evaluate_logic_expression(condition.children[0]);

        auto original_interval = m_interval_store.get(changed_var);
        auto original_store = IntervalStore<T>(m_interval_store);


        bool body_admitted = true;
        if (original_interval.contains(if_body_interval))
        {
            std::cout << "Condition is respected for " << changed_var << " [" << if_body_interval.lb() << "," << if_body_interval.ub() << "]" << std::endl;
            m_interval_store.get(changed_var) = if_body_interval;

            for (const auto& child : if_body.children)
            {
                eval(child);
            }
        } 
        else {
            body_admitted = false;
            std::cout << "Condition is not respected for " << changed_var << " [" << if_body_interval.lb() << "," << if_body_interval.ub() << "]" << std::endl;
        }

        auto if_body_store = IntervalStore<T>(m_interval_store);
        m_interval_store = original_store;

        std::cout << "Continuing. Looking at possible else case" << std::endl;

        if (node.children.size() == 3)
        {
            bool left_admitted = true;
            bool right_admitted = true;

            auto else_body = node.children[2];
            // Build the complementary interval of the condition of the if body

            auto left_interval = Interval<T>(original_interval.lb(), if_body_interval.lb() - 1).normalize();
            auto right_interval = Interval<T>(if_body_interval.ub() + 1, original_interval.ub()).normalize();

            std::cout << "Left interval for "<< changed_var<<" [" << left_interval.lb() << ", " << left_interval.ub() << "]" << std::endl;
            std::cout << "Right interval for"<< changed_var<<" [" << right_interval.lb() << ", " << right_interval.ub() << "]" << std::endl;
            
            auto left_store = IntervalStore<T>(m_interval_store);
            auto right_store = IntervalStore<T>(m_interval_store);

            // Evaluate the case for the left interval if it's in the original
            if (original_interval.contains(left_interval))
            {
                m_interval_store.get(changed_var) = left_interval;
                for (const auto& child : else_body.children)
                {
                    eval(child);
                }
                left_store = IntervalStore<T>(m_interval_store);
            }
            else
            {
                left_admitted = false;
                std::cout << "Left interval not admitted [" << left_interval.lb() << ", " << left_interval.ub() << "] not in [" << original_interval.lb() << ", " << original_interval.ub() << "]" << std::endl;
            }

            if (original_interval.contains(right_interval))
            {
                m_interval_store.get(changed_var) = right_interval;
                for (const auto& child : else_body.children)
                {
                    eval(child);
                }
                right_store = IntervalStore<T>(m_interval_store);
            }
            else
            {
                right_admitted = false;
                std::cout << "Right interval not admitted [" << right_interval.lb() << ", " << right_interval.ub() << "] not in [" << original_interval.lb() << ", " << original_interval.ub() << "]" << std::endl;
            }

            if (left_admitted && right_admitted)
            {
                right_store.joinAll(left_store);
                m_interval_store = right_store;
            }
            else if (left_admitted)
            {
                m_interval_store = left_store;
            }
            else if (right_admitted)
            {
                m_interval_store = right_store;
            }
            else
            {
                std::cerr << "No case admitted" << std::endl;
                exit(1);
            }
        } else {
            std::cout << "No else case" << std::endl;
        }

        if (body_admitted)
        {
            m_interval_store.joinAll(if_body_store);
        }

        m_interval_store.print();

        return true;
    }

    std::pair<Interval<T>, std::string> evaluate_logic_expression(const ASTNode& node)
    {
        // access the left and right sons of the logic operation
        node.print();
        auto left_expr = node.children[0];
        auto right_expr = node.children[1];
        auto op = std::get<LogicOp>(node.value);
        
        left_expr.print();

        if (left_expr.type != NodeType::VARIABLE)
        {
            std::cerr << "Only variables on the left are supported" << std::endl;
            exit(1);
        }
        auto var = std::get<std::string>(left_expr.value);
        auto res = evaluate_expression(right_expr);

        if (res.lb() != res.ub())
        {
            std::cerr << "Unexpected non constant value in if statement right side" << std::endl;
            exit(1);
        }        

        return {res, var};
    }

    Interval<T> evaluate_expression(const ASTNode& node)
    {
        if (node.type == NodeType::INTEGER)
        {
            return Interval<T>(std::get<T>(node.value), std::get<T>(node.value));
        }
        else if (node.type == NodeType::VARIABLE)
        {
            return m_interval_store.get(std::get<std::string>(node.value));
        }
        else if (node.type == NodeType::ARITHM_OP)
        {
            auto left = evaluate_expression(node.children[0]);
            auto right = evaluate_expression(node.children[1]);

            auto op = std::get<BinOp>(node.value);
            switch(op)
            {
                case BinOp::ADD:
                {
                    return left + right;
                }
                case BinOp::SUB:
                {
                    return left - right;
                }
                case BinOp::MUL:
                {
                    return left * right;
                }
                case BinOp::DIV:
                {
                    return left / right;
                }
                default:
                {
                    node.print();
                    std::cerr << "Unknown binary operation" << std::endl;
                    exit(1);
                }
            }
        }
        else
        {
            std::cerr << "Unexpected expression" << std::endl;
            exit(1);
        }
    }

};

#endif