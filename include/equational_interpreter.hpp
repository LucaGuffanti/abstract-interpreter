#ifndef EQUATIONAL_INTERPRETER_HPP
#define EQUATIONAL_INTERPRETER_HPP

#include <limits>
#include <concepts>
#include <map>
#include <list>
#include <memory>
#include <queue>

#include "location_base.hpp"

enum LocationType {
    ASSIGNMENT,
    POSTCONDITION,
    IFELSE,
    ENDIF,
    WHILE,
    ENDWHILE
};

template <typename T> requires std::is_arithmetic_v<T>
class EquationalInterpreter
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

    IntervalStore<T> m_precondition_store;

    std::vector<std::unique_ptr<Location<T>>> m_locations;
    std::vector<std::unique_ptr<Location<T>>> m_old_locations;
    std::list<std::string> m_variables;
    size_t m_location_counter = 0;
    std::shared_ptr<IntervalStore<T>> last_interval_store;
    std::shared_ptr<IntervalStore<T>> last_added_store;
    std::queue<std::shared_ptr<IntervalStore<T>>> m_if_body_stores_queue;
    std::queue<std::shared_ptr<IntervalStore<T>>> m_else_body_stores_queue;

    std::queue<std::shared_ptr<IntervalStore<T>>> m_final_if_body_stores_queue;
    std::queue<std::shared_ptr<IntervalStore<T>>> m_final_else_body_stores_queue;

    std::queue<std::shared_ptr<IntervalStore<T>>> m_while_body_stores_queue;
    std::queue<std::shared_ptr<IntervalStore<T>>> m_while_exit_stores_queue;
    std::queue<std::shared_ptr<IntervalStore<T>>> m_final_while_body_stores_queue;

    bool should_evaluate_postcondition = false;
public:
    EquationalInterpreter() = default;
    ~EquationalInterpreter() = default;

    EquationalInterpreter(const std::string& input)
    : m_locations()
    {
        AbstractInterpreterParser AIParser;
        m_ast = AIParser.parse(input);
        last_interval_store = std::make_shared<IntervalStore<T>>(m_precondition_store); 

    }

    EquationalInterpreter(const ASTNode ast)
    : m_ast(ast)
    , m_locations()
    {}

    void print()
    {
        m_ast.print();
    }

    void run()
    {
        std::size_t it_count = 0;
        build_equational_system();

        print_equational_system();

        do {
            std::cout << "===================Iteration " << it_count++ << "===================" << std::endl;
            copy_locations();
            std::cout << "===================OLD LOCATIONS===================" << std::endl;
            for (std::size_t i = 0; i < m_old_locations.size(); ++i)
            {
                std::cout << "Location " << i << std::endl;
                m_old_locations[i]->print();
            }   
            std::cout << "===================JACOBI ITERATION===================" << std::endl;
            solve_system();
            std::cout << "===================NEW LOCATIONS===================" << std::endl;
            for (std::size_t i = 0; i < m_locations.size(); ++i)
            {
                std::cout << "Location " << i << std::endl;
                m_locations[i]->print();
            }

            std::cout << "|CHECKING STABILITY|====================" << std::endl;
        } while (!is_stable());

        should_evaluate_postcondition = true;
        std::cout << "|EVALUATING POSTCONDITIONS|===================" << std::endl;
        evaluate_postconditions();
        std::cout << "===================COMPLETED===================" << std::endl;


    }

private:
    void copy_locations()
    {
        m_old_locations.clear();
        for (std::size_t i = 0; i < m_locations.size(); ++i)
        {
            std::cout << "  Copying location " << i << std::endl;
            auto& loc = m_locations[i];
            m_old_locations.push_back(copy_location(loc));
        }
    }

    std::unique_ptr<Location<T>> copy_location(std::unique_ptr<Location<T>>& src)
    {
        // copy the internal
        std::unique_ptr<Location<T>> copy;
        copy = std::move(src->copy());

        return copy;
    }

    bool is_stable()
    {
        for (std::size_t i = 0; i < m_locations.size(); ++i)
        {
            std::cout << " Location " << i;
            std::flush(std::cout);
            if (!m_locations[i]->is_stable(m_old_locations[i]))
            {
                std::cout << " not stable" << std::endl;
                return false;
            }
            else 
            {
                std::cout << " stable" << std::endl;
            }
        }
        return true;
    }

    void build_equational_system()
    {

        std::cout << "|VARIABLES AND PRECONDITIONS|========" << std::endl;

        // Get the variable declarations
        assert(m_ast.children[0].type == NodeType::DECLARATION);
        auto code_blocks = m_ast.children;

        auto decl_count = 0;
        auto decl_block_count = 0;
        while(code_blocks[decl_block_count].type == NodeType::DECLARATION)
        {
            for (const auto& new_variable_block : code_blocks[decl_block_count].children)
            {
                add_variable(new_variable_block);
                decl_count++;
            }
            decl_block_count++;
        }
        std::cout << "[INFO] Declared " << decl_count << " variables" << std::endl;

        // Advance to the next block, which should be a sequence block
        auto code_block = m_ast.children[decl_block_count];
        assert(code_block.type == NodeType::SEQUENCE && "Expected a sequence block");
        code_blocks = code_block.children;

        // Get all the preconditions
        std::cout << "[INFO] Introducting preconditions" << std::endl;
        auto prec_count = 0;
        while(code_blocks[prec_count].type == NodeType::PRE_CON)
        {
            for (const auto& prec : code_blocks[prec_count].children)
            {
                add_precondition(prec);
            }
            prec_count++;
        }
        std::cout << "[INFO] Added " << prec_count << " preconditions" << std::endl;

        std::cout << "|CONSTRUCTING LOCATIONS|==============" << std::endl;  
        for (int i = prec_count; i < code_blocks.size(); i++)
        {
            auto block = code_blocks[i];
            manage_block(block);
        }
    }

    void manage_block(ASTNode& block, std::shared_ptr<Location<T>> fallback_location = nullptr)
    {
        switch(block.type)
        {
            case NodeType::ASSIGNMENT:
            {
                std::cout << "[INFO] Assignment block" << std::endl;
                auto assignment_loc = std::make_unique<AssignmentLocation<T>>();

                assignment_loc->m_store_before = std::make_shared<IntervalStore<T>>();
                assignment_loc->m_store_after = std::make_shared<IntervalStore<T>>();
                assignment_loc->m_fallback_location = fallback_location;

                assignment_loc->m_code_block = block;
                
                assignment_loc->m_operation = [ptr=assignment_loc.get(), this](void) -> void {
                    // ptr->m_code_block.print();
                    std::cout << "-------EVALUATING ASSIGNMENT-------" << std::endl;
                    auto store = *(ptr->m_store_before);
                    store.print();

                    auto lhs = ptr->m_code_block.children[0];
                    auto rhs = ptr->m_code_block.children[1];

                    std::string var = std::get<std::string>(lhs.value);
                    auto interval = evaluate_expression(rhs, store);

                    // print the interval 
                    std::cout << "Interval of " << var << ": [" << interval.lb() << ", " << interval.ub() << "]" << std::endl;
                    // Modify the interval in the store
                    ptr->m_store_after = std::make_shared<IntervalStore<T>>(store);
                    ptr->m_store_after->set(var, interval);
                    

                };
            
                std::unique_ptr<Location<T>> loc = std::move(assignment_loc);
                m_locations.push_back(std::move(loc));
                m_location_counter++;
                std::cout << "[INFO] Added Assignment Location. Counter: " << m_location_counter << std::endl;

                break;
            }
            case NodeType::POST_CON:
            {
                std::cout << "[INFO] Postcondition block" << std::endl;
                auto postcondition_loc = std::make_unique<PostConditionLocation<T>>();

                postcondition_loc->m_store = std::make_shared<IntervalStore<T>>();
                
                postcondition_loc->m_code_block = block;

                postcondition_loc->m_fallback_location = fallback_location;

                postcondition_loc->m_operation = [ptr = postcondition_loc.get(), this](void) -> void {

                    std::cout << "-------EVALUATING POSTCONDITION-------" << std::endl;

                    auto store = *(ptr->m_store);
                    auto postcondition = ptr->m_code_block.children[0];

                    auto lhs = postcondition.children[0];
                    auto rhs = postcondition.children[1];

                    auto op = std::get<LogicOp>(postcondition.value);

                    auto left = evaluate_expression(lhs, store);
                    auto right = evaluate_expression(rhs, store);

                    if (should_evaluate_postcondition)
                    {
                        auto eval = evaluate_logic_operation(left, right, op);

                        if (eval)
                        {
                            std::cout << "Postcondition satisfied" << std::endl;
                        }
                        else
                        {
                            std::cerr << "Postcondition not satisfied" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "Postcondition not evaluated" << std::endl;
                    }
                };
                
                std::unique_ptr<Location<T>> loc = std::move(postcondition_loc);
                m_locations.push_back(std::move(loc));
                m_location_counter++;
                std::cout << "[INFO] Added Postcondition Location. Counter: " << m_location_counter << std::endl;
                break;
            }
            case NodeType::IFELSE:
            {
                std::cout << "[INFO] If-else block" << std::endl;
                auto ifelse_loc = std::make_unique<IfElseLocation<T>>();

                ifelse_loc->m_store_before_condition = std::make_shared<IntervalStore<T>>();
                ifelse_loc->m_store_if_body = std::make_shared<IntervalStore<T>>();
                ifelse_loc->m_store_else_body = std::make_shared<IntervalStore<T>>();
                ifelse_loc->m_fallback_location = fallback_location;
                ifelse_loc->m_code_block = block;

                auto m_store_else_body_copy = ifelse_loc->m_store_else_body; 

                auto exists_else_block = block.children.size() == 3;

                ifelse_loc->m_operation = [ptr = ifelse_loc.get(), this](void) -> void {
                    std::cout << "-------EVALUATING IF-ELSE-------" << std::endl;
                    
                    auto empty_if_body = false;
                    auto empty_else_body = false;

                    auto store = *(ptr->m_store_before_condition);
                    
                    auto exists_else = ptr->m_code_block.children.size() == 3;
                    auto condition_block = ptr->m_code_block.children[0].children;
                    auto condition = condition_block[0];
                    auto if_body = ptr->m_code_block.children[1];

                    // Start by evaluating the condition and restricting the store
                    auto lhs = condition.children[0];
                    auto rhs = condition.children[1];
                    auto var = std::get<std::string>(lhs.value);
                    auto op = std::get<LogicOp>(condition.value);

                    assert(lhs.type == NodeType::VARIABLE && "UNEXPECTED EXPRESSION ON LHS");
                    auto rhs_interval = evaluate_expression(rhs, store);

                    std::cout << "If condition: " << var << " " << op << " [" << rhs_interval.lb() << ", " << rhs_interval.ub() << "]" << std::endl; 

                    auto if_body_store = apply_command_to_store(store, var, rhs_interval, op);
                    if (if_body_store.get(var).is_empty())
                    {
                        empty_if_body = true;
                    }
                    else
                    {
                         if_body_store.print();
                    }

                    ptr->m_store_if_body = std::make_shared<IntervalStore<T>>(if_body_store);
                    m_if_body_stores_queue.push(ptr->m_store_if_body);

                    ptr->m_store_if_body->print();

                    auto complementary_op = extract_complementary_op(op);
                    auto rhs_interval_else = evaluate_expression(rhs, store);
                    auto else_body_store = apply_command_to_store(store, var, rhs_interval_else, complementary_op);
                    

                    std::cout << "Else condition: " << var << " " << complementary_op << " [" << rhs_interval_else.lb() << ", " << rhs_interval_else.ub() << "]" << std::endl;
                    else_body_store.print();
                    ptr->m_store_else_body = std::make_shared<IntervalStore<T>>(else_body_store);
                    m_else_body_stores_queue.push(ptr->m_store_else_body);

                    if (!exists_else)
                    {
                        m_final_else_body_stores_queue.push(ptr->m_store_else_body);
                    } 

                    if (else_body_store.get(var).is_empty())
                    {
                        empty_else_body = true;
                    }
                    else
                    {
                        else_body_store.print();
                    }
                    
                    if (empty_if_body && empty_else_body)
                    {
                        std::cerr << "[WARNING] Both branches are empty for variable " << var << std::endl; 
                    }
                    else if (empty_if_body)
                    {
                        std::cerr << "[WARNING] If body branch is empty for variable " << var << std::endl;
                    }
                    else if (empty_else_body)
                    {
                        std::cerr << "[WARNING] Else body branch is empty for variable " << var << std::endl;
                    }

                    std::cout << "If header completed" << std::endl;
                };

                std::unique_ptr<Location<T>> loc = std::move(ifelse_loc);
                m_locations.push_back(std::move(loc));
                m_location_counter++;
                std::cout << "[INFO] Added If Else Location. Counter: " << m_location_counter << std::endl; 


                // Check, recusively, the if branch
                std::cout << "[INFO] Entering the if body" << std::endl;
                auto if_body_blocks = block.children[1].children;
                auto if_body_block = if_body_blocks[0];

                // In the case of a sequence, get to the children
                if (if_body_block.type == NodeType::SEQUENCE)
                    if_body_blocks = if_body_block.children;

                for (std::size_t i = 0; i < if_body_blocks.size(); ++i)
                {
                    manage_block(if_body_blocks[i]);
                }

                auto last_location_offset = m_location_counter;
                std::cout << last_location_offset << std::endl;

                auto last_if_body_location_store = m_locations[last_location_offset - 1]->get_last_store();
                m_locations[last_location_offset - 1]->ends_if_body = true;

                // If it exists, check the if branch                
                std::shared_ptr<IntervalStore<T>> last_else_body_location_store = nullptr;
                if (exists_else_block)
                {
                    std::cout << "[INFO] Checking else block" << std::endl;
                    auto else_body_blocks = block.children[2].children;
                    auto else_body_block = else_body_blocks[0];

                    // else_body_block.print();
                    if (else_body_block.type == NodeType::SEQUENCE)
                        else_body_blocks = else_body_block.children;
                    
                    for (std::size_t i = 0; i < else_body_blocks.size(); ++i)
                    {
                        // else_body_blocks[i].print();
                        manage_block(else_body_blocks[i]);
                    }

                    last_location_offset = m_location_counter;
                    std::cout << last_location_offset << std::endl;
                    last_else_body_location_store = m_locations[last_location_offset - 1]->get_last_store();
                    m_locations[last_location_offset - 1]->ends_else_body = true;
                }

                if (last_else_body_location_store == nullptr)
                {
                    last_else_body_location_store = m_store_else_body_copy;
                }

                std::cout << "[INFO] If closure location" << std::endl;
                auto endif_loc = std::make_unique<EndIfLocation<T>>();
                endif_loc->m_store_before = nullptr;
                endif_loc->m_store_after_body = last_if_body_location_store;
                endif_loc->m_store_after_else = last_else_body_location_store;
                endif_loc->m_store_after = nullptr;

                endif_loc->m_operation = [ptr = endif_loc.get(), this](void) -> void {
                    std::cout << "Finalizing if statement" << std::endl;
                    auto if_body_store = *(ptr->m_store_after_body);
                    auto else_body_store = *(ptr->m_store_after_else);

                    // Copy the if body store to the after store
                    ptr->m_store_after = std::make_shared<IntervalStore<T>>(if_body_store);
                    // And join that with the else store
                    ptr->m_store_after->joinAll(else_body_store);
                    std::cout << "Store after if-else" << std::endl;
                    ptr->m_store_after->print();
                };

                std::unique_ptr<Location<T>> end_loc = std::move(endif_loc);
                m_locations.push_back(std::move(end_loc));
                m_location_counter++;
                std::cout << "[INFO] Added If Closure Location. Counter: " << m_location_counter << std::endl; 
                break;
            }
            case NodeType::WHILELOOP:
            {

                auto while_loc = std::make_unique<WhileLocation<T>>();

                while_loc->m_store_before_condition = std::make_shared<IntervalStore<T>>();
                while_loc->m_store_body = std::make_shared<IntervalStore<T>>();
                while_loc->m_store_exit = std::make_shared<IntervalStore<T>>();

                while_loc->m_code_block = block;

                while_loc->m_operation = [ptr = while_loc.get(), this](void) -> void {
                    std::cout << "-------EVALUATING WHILE-------" << std::endl;
                    
                    // ptr->m_code_block.print();

                    auto store = *(ptr->m_store_before_condition);
                    store.print();
                    auto condition_blocks = ptr->m_code_block.children[0];

                    auto condition = condition_blocks.children[0];

                    auto var = std::get<std::string>(condition.children[0].value);
                    auto op = std::get<LogicOp>(condition.value);
                    auto rhs = condition.children[1];

                    auto rhs_interval = evaluate_expression(rhs, store);
                    std::cout << "While Condition " << var << " " << op << " [" << rhs_interval.lb() << ", " << rhs_interval.ub() << "]" << std::endl;

                    auto while_body_store = IntervalStore<T>(store);
                    if (m_final_while_body_stores_queue.empty())
                    {
                        std::cerr << "No feedback store yet" << std::endl;
                    } 
                    else
                    {
                        auto feedback_store_ptr = m_final_while_body_stores_queue.front();
                        auto feedback_store = *feedback_store_ptr;
                        while_body_store.joinAll(feedback_store);
                        m_final_while_body_stores_queue.pop();
                    }
                    auto while_body_store_2 = apply_command_to_store(while_body_store, var, rhs_interval, op);

                    ptr->m_store_body = std::make_shared<IntervalStore<T>>(while_body_store_2);
                    ptr->m_store_body->print();
                    m_while_body_stores_queue.push(ptr->m_store_body);
            
                    auto complementary_op = extract_complementary_op(op);

                    std::cout << "Complementary while condition " << var << " " << complementary_op << " [" << rhs_interval.lb() << ", " << rhs_interval.ub() << "]" << std::endl;

                    auto while_exit_store = apply_command_to_store(while_body_store, var, rhs_interval, complementary_op);
                    ptr->m_store_exit = std::make_shared<IntervalStore<T>>(while_exit_store);

                    m_while_exit_stores_queue.push(ptr->m_store_exit);

                    std::cout << "Finished while header" << std::endl;
                };

                std::unique_ptr<Location<T>> loc = std::move(while_loc);
                m_locations.push_back(std::move(loc));
                m_location_counter++;
                std::cout << "[INFO] Added While Location. Counter: " << m_location_counter << std::endl;

                // Check, recursively, the while body.
                std::cout << "[INFO] Entering While Body" << std::endl;

                auto while_body_blocks = block.children[1].children;
                auto while_body_first_block = while_body_blocks[0];

                if (while_body_first_block.type == NodeType::SEQUENCE)
                    while_body_blocks = while_body_first_block.children;

                for (std::size_t i = 0; i < while_body_blocks.size(); ++i)
                {
                    manage_block(while_body_blocks[i]);
                }

                auto last_location_offset = m_location_counter;
                std::cout << last_location_offset << std::endl;

                m_locations[last_location_offset - 1]->ends_while_body = true;

                auto end_while_loc = std::make_unique<EndWhileLocation<T>>();

                end_while_loc->m_store_from_while = nullptr;
                end_while_loc->m_store_after = nullptr;

                end_while_loc->m_operation = [ptr = end_while_loc.get(), this](void) -> void {
                    std::cout << "Finalizing while statement" << std::endl;
                    auto while_body_store = *(ptr->m_store_from_while);

                    while_body_store.print();

                    // Copy the while body store to the after store
                    ptr->m_store_after = std::make_shared<IntervalStore<T>>(while_body_store);
                    // And join that with the else store
                    std::cout << "Store after while" << std::endl;
                    ptr->m_store_after->print();
                };
                

                std::unique_ptr<Location<T>> end_loc = std::move(end_while_loc); 
                m_locations.push_back(std::move(end_loc));
                m_location_counter++;
                std::cout << "[INFO] Added End While Location. Counter: " << m_location_counter << std::endl;
                break;
            }
            default:
            {
                std::cerr << "Unknown block type" << std::endl;
                block.print();
                exit(1);
            }
        }
    }

    LogicOp extract_complementary_op(LogicOp op)
    {
        std::cout << "Extracting complementary op of " << op << std::endl;
        switch(op)
        {
            case LogicOp::LEQ:
            {
                return LogicOp::GE;
            }
            case LogicOp::GEQ:
            {
                return LogicOp::LE;
            }
            case LogicOp::EQ:
            {
                return LogicOp::NEQ;
            }
            case LogicOp::NEQ:
            {
                return LogicOp::EQ;
            }
            case LogicOp::LE:
            {
                return LogicOp::GEQ;
            }
            case LogicOp::GE:
            {
                return LogicOp::LEQ;
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                exit(1);
            }
        }
    }

    Interval<T> evaluate_expression(ASTNode& block, IntervalStore<T>& store)
    {
        if (block.type == NodeType::INTEGER)
        {
            return Interval<T>(std::get<T>(block.value), std::get<T>(block.value));
        }
        else if (block.type == NodeType::VARIABLE)
        {
            return store.get(std::get<std::string>(block.value));
        }
        else if (block.type == NodeType::ARITHM_OP)
        {
            auto left = evaluate_expression(block.children[0], store);
            auto right = evaluate_expression(block.children[1], store);

            auto op = std::get<BinOp>(block.value);
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
                        std::cerr << "[WARNING] Division by 0" << std::endl;
                    }
                    return left / right;
                }
                default:
                {
                    // block.print();
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

    IntervalStore<T> apply_command_to_store(IntervalStore<T> store, std::string& var, Interval<T>& interval, LogicOp& op)
    {
        switch(op)
        {
            case LogicOp::LEQ:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();

                Interval<T> new_interval = {lb, ub};
                Interval<T> original_unbounded = {min_T, interval.ub()};
                new_interval.meet(original_unbounded);

                store.set(var, new_interval);
                return store;
            }
            case LogicOp::LE:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();

                Interval<T> new_interval = {lb, ub};
                Interval<T> original_unbounded = {min_T, interval.ub() - 1};
                new_interval.meet(original_unbounded);

                store.set(var, new_interval);
                return store;
            }
            case LogicOp::GEQ:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();

                Interval<T> new_interval = {lb, ub};
                Interval<T> original_unbounded = {interval.lb(), max_T};
                new_interval.meet(original_unbounded);
                
                store.set(var, new_interval);
                return store;
            }
            case LogicOp::GE:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();

                Interval<T> new_interval = {lb, ub};
                Interval<T> original_unbounded = {interval.lb() + 1, max_T};
                new_interval.meet(original_unbounded);
                
                store.set(var, new_interval);
                return store;
            }
            case LogicOp::EQ:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();

                Interval<T> new_interval = {lb, ub};
                new_interval.meet(interval);

                store.set(var, new_interval);
                return store;
            }
            case LogicOp::NEQ:
            {
                auto lb = store.get(var).lb();
                auto ub = store.get(var).ub();
                // if the interval to be removed is on the lower side
                if (interval.lb() <= lb)
                {
                    if (lb + 1 <= ub)
                    {
                        store.set(var, {lb + 1, ub});
                    }
                    else
                    {
                        store.get(var).is_empty() = true;
                    }
                    return store;
                }

                // If the interval to be removed is on the upper side
                if (interval.ub() >= ub)
                {
                    if (ub - 1 >= lb)
                    {
                        store.set(var, {lb, ub - 1});
                    }
                    else
                    {
                        store.get(var).is_empty() = true;
                    }
                    return store;
                }

                // If the interval to be removed is in the middle
                if (interval.ub() < ub && interval.lb() > lb)
                {
                    return store;
                }

                // If the interval to be removed is exactly equal to the interval
                if (interval.ub() == ub && interval.lb() == lb)
                {
                    store.get(var).is_empty() = true;
                    return store;
                }

                exit(1);
                
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                exit(1);
            }
        }
        return store;
    }

    bool evaluate_logic_operation(Interval<T> left, Interval<T> right, LogicOp op)
    {
        // TODO: Verify that this is correct
        switch(op)
        {
            case LogicOp::LEQ:
            {
                return left.ub() <= right.ub() && left.lb() <= right.lb();
            }
            case LogicOp::GEQ:
            {
                return left.lb() >= right.lb() && left.ub() >= right.ub();
            }
            case LogicOp::EQ:
            {
                return left.lb() == right.lb() && left.ub() == right.ub();
            }
            case LogicOp::NEQ:
            {
                return !(left.lb() == right.lb() && left.ub() == right.ub());
            }
            case LogicOp::LE:
            {
                return left.lb() < right.lb() && left.ub() < right.ub();
            }
            case LogicOp::GE:
            {
                return left.ub() > right.ub() && left.lb() > right.lb();
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                return false;
            }
        }
    }

    void print_equational_system()
    {
        std::cout << "|PRECONDITIONS|========================" << std::endl;   
        for (auto &prec_element : m_precondition_store.store())
        {
            std::cout << "[INFO] " << prec_element.first << ": [" << prec_element.second.lb() << ", " << prec_element.second.ub() << "]" << std::endl;
        }
        std::cout << "=======================================" << std::endl;
    }

    void add_variable(const ASTNode& node)
    {
        auto var_name = std::get<std::string>(node.value);
        std::cout << "[INFO] Adding variable " << var_name << std::endl;
        m_precondition_store.set(var_name, {min_T, max_T});
        m_variables.push_back(var_name);
    }
    
    void add_precondition(const ASTNode& node)
    {
        auto op = std::get<std::string>(node.value);
        
        auto left = node.children[0];
        auto right = node.children[1];
        bool variable_on_left = false;

        T val;
        std::string var;
        if (left.type==NodeType::INTEGER && right.type==NodeType::VARIABLE)
        {
            val = std::get<T>(left.value);
            var = std::get<std::string>(right.value);
        }
        else if (left.type==NodeType::VARIABLE && right.type==NodeType::INTEGER)
        {
            variable_on_left = true;
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
                    m_precondition_store.get(var).ub() = val;
                }
                else
                {
                    m_precondition_store.get(var).lb() = val;
                }

                break;
            }
            case LogicOp::GEQ:
            {
                if (variable_on_left)
                {
                    m_precondition_store.get(var).lb() = val;
                }
                else
                {
                    m_precondition_store.get(var).ub() = val;
                }
                break;
            }
            default:
            {
                std::cerr << "Unknown logic operation" << op << std::endl;
                exit(1);
            }
        }
    }

    void construct_locations(const ASTNode& node)
    {
        
    }

    void solve_system()
    {
        last_interval_store = std::make_shared<IntervalStore<T>>(m_precondition_store);
        auto last_location_type = LocationType::ASSIGNMENT;
        std::unique_ptr<Location<T>> last_location = nullptr;
        std::size_t counter = 0;
        for (auto& loc : m_locations)
        {
            auto current_location_type = identify_type(loc);

            if (last_location_type == LocationType::IFELSE)
            {
                auto last_if_body_store = m_if_body_stores_queue.front();
                m_if_body_stores_queue.pop();

                std::cout << "Setting previous store to if body store" << std::endl;

                loc->set_previous_store(last_if_body_store);
            }
            else 
            {
                if (current_location_type == LocationType::ENDIF)
                {
                    auto last_if_body_store = m_final_if_body_stores_queue.front();
                    m_final_if_body_stores_queue.pop();

                    auto last_else_body_store = m_final_else_body_stores_queue.front();
                    m_final_else_body_stores_queue.pop();

                    loc->set_final_if_body_store(last_if_body_store);
                    loc->set_final_else_body_store(last_else_body_store);
                }
                else if (counter != 0 && last_location->ends_if_body)
                {
                // This means that the next node is the else body
                    auto last_else_body_store = m_else_body_stores_queue.front();
                    m_else_body_stores_queue.pop();

                    std::cout << "Setting previous store to else body store" << std::endl;
                    // print the if body store
                    last_else_body_store->print();

                    loc->set_previous_store(last_else_body_store);
                } 
                else if (counter != 0 && current_location_type == LocationType::ENDWHILE)
                {
                    auto last_while_body_store = m_while_exit_stores_queue.front();
                    m_while_exit_stores_queue.pop();

                    loc->set_final_while_body_store(last_while_body_store);
                }
                else if (counter != 0 && last_location_type == LocationType::WHILE)
                {
                    auto last_while_body_store = m_while_body_stores_queue.front();
                    m_while_body_stores_queue.pop();

                    loc->set_previous_store(last_while_body_store);
                }
                else
                {
                    loc->set_previous_store(last_interval_store);
                }
            }

            
            loc->m_operation();
            last_interval_store = loc->get_last_store();

            if (loc->ends_if_body)
            {
                m_final_if_body_stores_queue.push(last_interval_store);
            }
            if (loc->ends_else_body)
            {
                m_final_else_body_stores_queue.push(last_interval_store);
            }
            if (loc->ends_while_body)
            {
                m_final_while_body_stores_queue.push(last_interval_store);
            }

            last_location = std::make_unique<Location<T>>(*loc);
            last_location_type = identify_type(loc);
            counter++;
        }
    }

    LocationType identify_type(std::unique_ptr<Location<T>>& loc)
    {
        if (dynamic_cast<AssignmentLocation<T>*>(loc.get()))
        {
            return LocationType::ASSIGNMENT;
        }
        else if (dynamic_cast<PostConditionLocation<T>*>(loc.get()))
        {
            return LocationType::POSTCONDITION;
        }
        else if (dynamic_cast<IfElseLocation<T>*>(loc.get()))
        {
            return LocationType::IFELSE;
        }
        else if (dynamic_cast<EndIfLocation<T>*>(loc.get()))
        {
            return LocationType::ENDIF;
        }
        else if(dynamic_cast<WhileLocation<T>*>(loc.get()))
        {
            return LocationType::WHILE;
        }
        else if(dynamic_cast<EndWhileLocation<T>*>(loc.get()))
        {
            return LocationType::ENDWHILE;
        }
        else
        {
            std::cerr << "Unknown location type" << std::endl;
            exit(1);
        }
    }

    void evaluate_postconditions()
    {
        for (auto& loc : m_locations)
        {
            if (dynamic_cast<PostConditionLocation<T>*>(loc.get()))
            {
                loc->m_operation();
            }
        }
    }
};



#endif