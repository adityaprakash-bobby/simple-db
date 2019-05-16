/*
    Enumerators to handle various status codes related to the different
    funtions and implementations
*/
enum MetaCommandResult_t {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
};

typedef enum MetaCommandResult_t MetaCommandResult;

enum PrepareResult_t {
    PREPARE_SUCCESS,
    PREPARE_NEGATIVE_ID,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
    PREPARE_UNRECOGNIZED_STATEMENT
};

typedef enum PrepareResult_t PrepareResult;

enum StatementType_t {
    STATEMENT_INSERT,
    STATEMENT_SELECT
};

typedef enum StatementType_t StatementType;

enum ExecuteResult_t {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
};

typedef enum ExecuteResult_t ExecuteResult;

enum NodeType_t {
    NODE_INTERNAL,
    NODE_LEAF
};

typedef enum NodeType_t NodeType;