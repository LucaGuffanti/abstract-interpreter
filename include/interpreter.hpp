#ifndef ABSTRACT_INTERPRETER_HPP
#define ABSTRACT_INTERPRETER_HPP

#include <limits>
#include <concepts>
#include <map>

#include "parser.hpp"
#include "interval_store.hpp"

/**
 * @brief The AbstractInterpreter class represents an abstract interpreter for the simple C language demo.
 * 
 * @tparam T 
 */
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

    const T null_T = static_cast<T>(0);
    const T min_T = std::numeric_limits<T>::min();
    const T max_T = std::numeric_limits<T>::max();

    ASTNode m_ast;
    IntervalStore<T> m_interval_store;
    IntervalStore<T> m_precondition_store;
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

    /**
     * @brief Based on the current node that is encountered during the traversal, evaluates the semantics of the statements, acting on variables. 
     * 
     * @param node 
     */
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
                // node.print();
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

    /**
     * @brief Evaluates the declaration of a variable adding it to the store
     * 
     * @param node 
     * @return true 
     * @return false 
     */
    bool evaluate_variable_declaration(const ASTNode& node)
    {
        auto var_name = std::get<std::string>(node.value);
        #ifdef DEBUG
        std::cout << "Variable name: " << var_name << std::endl;
        #endif
        
        m_interval_store.set(var_name, {min_T, max_T});
        m_precondition_store.set(var_name, {min_T, max_T});

        #ifdef DEBUG
        auto interval = m_interval_store.get(var_name);
        std::cout << "Interval: [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
        #endif
        return true;
    }

    /**
     * @brief Evaluates the logic operation embedded into preconditions
     * 
     * @param node 
     * @return true 
     * @return false 
     */
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
                    m_precondition_store.get(var).ub() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced upper bound of " << var << " to " << val << std::endl;
                    #endif
                }
                else
                {
                    m_interval_store.get(var).lb() = val;
                    m_precondition_store.get(var).lb() = val;
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
                    m_precondition_store.get(var).lb() = val;
                    #ifdef DEBUG
                    std::cout << "Reduced lower bound of " << var << " to " << val << std::endl;
                    #endif
                }
                else
                {
                    m_interval_store.get(var).ub() = val;
                    m_precondition_store.get(var).ub() = val;
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
        std::cout << "Precondition of variable " << var <<" : [" << m_precondition_store.get(var).lb() << ", " << m_precondition_store.get(var).ub() << "]" << std::endl;
        #endif
        return true;
    }

    /**
     * @brief Evaluates an assignment node, changing the interval associated to the chosen variable
     * 
     * @param node 
     * @return true 
     * @return false 
     */
    bool evaluate_assignment(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Evaluating assignment" << std::endl;
        node.print();
        #endif

        auto var = std::get<std::string>(node.children[0].value);
        auto expr = node.children[1];

        auto result = evaluate_expression(expr);
        // if (respects_precondition(var, result))
        // {
        //     #ifdef DEBUG
        //     std::cout << "Assignment respects precondition" << std::endl;
        //     #endif
        // }
        // else
        // {
        //     // std::cerr << "Assignment does not respect precondition for " << var << std::endl;

        // }
        m_interval_store.get(var) = result;

        #ifdef DEBUG
        std::cout << "Interval of variable " << var <<" : [" << m_interval_store.get(var).lb() << ", " << m_interval_store.get(var).ub() << "]" << std::endl;
        #endif
        return true;
    }

    // bool respects_precondition(const std::string& var, Interval<T>& interval)
    // {

    //     #ifdef DEBUG
    //     std::cout << "Checking if assignment respects precondition" << std::endl;
    //     std::cout << "Variable: " << var << std::endl;
    //     std::cout << "Interval: [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
    //     std::cout << "Precondition: [" << m_precondition_store.get(var).lb() << ", " << m_precondition_store.get(var).ub() << "]" << std::endl;
    //     #endif
    //     return interval.lb() >= m_precondition_store.get(var).lb() && interval.ub() <= m_precondition_store.get(var).ub();
    // }

    /**
     * @brief Evaluates a postcondition found in the AST by considering the node and its children. The postcondition
     * is evaluated by comparing the left and right expressions with the logic operation, and checking if the generated 
     * intervals respect the logic operation.
     * 
     * @param node 
     * @return true 
     * @return false 
     */
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
            }
        }
        return true;
    }

    /**
     * @brief Evaluates an if-else statement found in the AST by considering the node and its children.
     * 
     * @param node 
     */
    bool evaluate_if_else(const ASTNode& node)
    {
        #ifdef DEBUG
        std::cout << "Evaluating if else" << std::endl;
        node.print();
        #endif

        auto condition = node.children[0];
        auto if_body = node.children[1]; 
        
        auto op = std::get<LogicOp>(condition.children[0].value);
        #ifdef DEBUG
        std::cout << "operation " << op << std::endl;
        #endif

        if (op != LogicOp::EQ) {
            std::cerr << "Only equality is supported in if else statements" << std::endl;
            exit(1);
        }

        auto [if_body_interval, changed_var] = evaluate_logic_expression(condition.children[0]);

        auto original_interval = m_interval_store.get(changed_var);
        auto original_store = IntervalStore<T>(m_interval_store);


        bool body_admitted = true;
        if (original_interval.contains(if_body_interval))
        {
            #ifdef DEBUG
            std::cout << "Condition is respected for " << changed_var << " [" << if_body_interval.lb() << "," << if_body_interval.ub() << "]" << std::endl;
            #endif

            m_interval_store.get(changed_var) = if_body_interval;

            for (const auto& child : if_body.children)
            {
                eval(child);
            }
        } 
        else {
            body_admitted = false;
            #ifdef DEBUG
            std::cout << "Condition is not respected for " << changed_var << " [" << if_body_interval.lb() << "," << if_body_interval.ub() << "]" << std::endl;
            #endif
        }

        auto if_body_store = IntervalStore<T>(m_interval_store);
        m_interval_store = original_store;

        #ifdef DEBUG
        std::cout << "Continuing. Looking at possible else case" << std::endl;
        #endif

        if (node.children.size() == 3)
        {
            bool left_admitted = true;
            bool right_admitted = true;

            auto else_body = node.children[2];
            // Build the complementary interval of the condition of the if body

            if (if_body_interval.lb() == min_T || if_body_interval.ub() == max_T)
            {
                std::cerr << "Overflow Encountered in evaluating if statement" << std::endl;
            }

            auto left_interval = Interval<T>(original_interval.lb(), if_body_interval.lb() - 1).normalize();
            auto right_interval = Interval<T>(if_body_interval.ub() + 1, original_interval.ub()).normalize();


            #ifdef DEBUG
            std::cout << "Left interval for "<< changed_var<<" [" << left_interval.lb() << ", " << left_interval.ub() << "]" << std::endl;
            std::cout << "Right interval for"<< changed_var<<" [" << right_interval.lb() << ", " << right_interval.ub() << "]" << std::endl;
            #endif

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
                #ifdef DEBUG
                std::cout << "Left interval not admitted [" << left_interval.lb() << ", " << left_interval.ub() << "] not in [" << original_interval.lb() << ", " << original_interval.ub() << "]" << std::endl;
                #endif
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
                #ifdef DEBUG
                std::cout << "Right interval not admitted [" << right_interval.lb() << ", " << right_interval.ub() << "] not in [" << original_interval.lb() << ", " << original_interval.ub() << "]" << std::endl;
                #endif
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

        #ifdef DEBUG
        m_interval_store.print();
        #endif

        return true;
    }

    /**
     * @brief Evaluates a logic expression in the AST by performing a depth traversal of the syntactic tree representing the expression.
     * 
     * @param node 
     * @return std::pair<Interval<T>, std::string> 
     */
    std::pair<Interval<T>, std::string> evaluate_logic_expression(const ASTNode& node)
    {
        // access the left and right sons of the logic operation
        //node.print();
        auto left_expr = node.children[0];
        auto right_expr = node.children[1];
        auto op = std::get<LogicOp>(node.value);
        
        //left_expr.print();

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

    /**
     * @brief Evaluates an expression in the AST by performing a depth traversal of the syntactic tree representing the expression, computing
     * the resulting interval
     * 
     * @param node 
     * @return Interval<T> 
     */
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
                    if (right.contains(null_T))
                    {
                        std::cerr << "!WARNING!: DIVISION BY ZERO" << std::endl;
                    }
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