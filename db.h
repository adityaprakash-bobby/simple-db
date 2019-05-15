
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

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

struct InputBuffer_t {

    char* buffer;
    size_t buffer_length;
    ssize_t input_length;

};

typedef struct InputBuffer_t InputBuffer;

// A row data structure to store the data into a hard coded table
struct Row_t {

    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];

};

typedef struct Row_t Row;

struct Statement_t {
  
    StatementType type;
    Row row_to_insert;

};

typedef struct Statement_t Statement;

// A Pager structure to access the page caches and files
struct Pager_t {

    int file_descriptor;
    uint32_t file_length;
    void* pages[TABLE_MAX_PAGES];

};

typedef struct Pager_t Pager;

// Structure to keep track of the pages of the rows
struct Table_t {

    uint32_t num_rows;
    Pager* pager;
};

typedef struct Table_t Table;

// A small wrapper to interract with getline()
InputBuffer* new_input_buffer() {
    
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;

}

// Compact representation of a Row in the table
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

// Keeping track of the number of rows
const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

// Function to serialize the row data 
void serialize_row(Row* source, void* destination) {

    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);

}

// Function to deserialize the row data from memory
void deserialize_row(void* source, Row* destination) {

    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);    

}

// Function to print a row of the table
void print_row(Row* row) {
    printf("( %d, %s, %s )\n", row->id, row->username, row->email);
} 

// Memory page for reading and writing
void* row_slot(Table* table, uint32_t row_num) {

    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = get_page(table->pager, page_num);

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;

}

// Print a prompt for the user
void print_prompt() {
    printf("db>");
}

// Read input from STDIN
void read_input(InputBuffer* input_buffer) {

    ssize_t bytes_read = getline(&(input_buffer->buffer), 
                            &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0) {
        
        printf("Error reading input\n");
        exit(EXIT_FAILURE);

    }

    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

// Function to free allocated memory for the instance of InputBuffer
void close_input_buffer(InputBuffer* input_buffer) {

    free(input_buffer->buffer);
    free(input_buffer);

}

// Wrapper that handles non-SQL commands like '.exit' and leaves room for more 
// such commands
MetaCommandResult do_meta_command(InputBuffer* input_buffer) {

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        exit(EXIT_SUCCESS);
    }   

    else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }

}

/* 
    The SQL compiler
*/
// Function to handle the compiliing of the insert statements
PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement) {

    statement->type = STATEMENT_INSERT;

    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " "); 
    char* username = strtok(NULL, " ");
    char* email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);
    if (id < 0) {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(email) > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;

}

PrepareResult prepare_statement(InputBuffer* input_buffer, 
                                Statement* statement) {
    
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }

    if (strcmp(input_buffer->buffer, "select") == 0) {

        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;

    }

    return PREPARE_UNRECOGNIZED_STATEMENT;

}

// void execute_statement(Statement* statement) {

//     switch(statement->type) {

//         case (STATEMENT_INSERT):
//             printf("Insert funtion will be handled here.\n");
//             break;

//         case (STATEMENT_SELECT):
//             printf("Select funtion will be handled here.\n");
//             break;

//     }

// }

ExecuteResult execute_insert(Statement* statement, Table* table) {

    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;

    return EXECUTE_SUCCESS;

}

ExecuteResult execute_select(Statement* statement, Table* table) {

    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++) {

        deserialize_row(row_slot(table, i), &row);
        print_row(&row);

    }

    return EXECUTE_SUCCESS;

}

ExecuteResult execute_statement(Statement* statement, Table* table) {

    switch(statement->type) {

        case (STATEMENT_INSERT):
            return execute_insert(statement, table);

        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }

}

Pager* pager_open(const char* filename) {

    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);

    if (fd = -1) {
        printf("Unable to open file,\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);

    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {

        pager->pages[i] = NULL;

    }

    return pager;
}

Table* db_open(const char* filename) {

    Pager* pager = pager_open(filename);
    uint32_t num_rows = pager->file_length / ROW_SIZE;
    
    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    table->num_rows = num_rows;

    return table;

}

void free_table(Table* table) {

    for (int i = 0; table->pages[i]; i++) {

        free(table->pages[i]);

    }

    free(table);

}