using namespace std;



#pragma once

#include "AST.hpp"

#include <memory>
#include <string>


namespace db {

class CatalogManager;
class StorageManager;

class ExecutionEngine {
public:
    ExecutionEngine(shared_ptr<CatalogManager> catalog,
                    shared_ptr<StorageManager> storage);

    string execute(const Statement& statement);

private:
    shared_ptr<CatalogManager> c_;
    shared_ptr<StorageManager> s_;

    string handle_create_table(const CreateTableStatement& statement);
    string handle_drop_table(const DropTableStatement& statement);
    string handle_alter_table(const AlterTableStatement& statement);
    string handle_insert(const InsertStatement& statement);
    string handle_update(const UpdateStatement& statement);
    string handle_delete(const DeleteStatement& statement);
    string handle_select(const SelectStatement& statement);
};

using ExecutionEnginePtr = shared_ptr<ExecutionEngine>;

} 
