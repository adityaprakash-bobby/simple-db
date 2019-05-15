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

enum NodeType_t {
    NODE_INTERNAL,
    NODE_LEAF
};

typedef enum NodeType_t NodeType;

/*
    Structures to deal with various implementations
*/
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

// Cursor structure to point to a location in the table
struct Cursor_t {

    Table* table;
    uint32_t row_num;
    bool end_of_table;

};

typedef struct Cursor_t Cursor;

// A small wrapper to interract with getline()
InputBuffer* new_input_buffer() {
    
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;

}

/*
    Compact representation of a Row in the table
*/
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

/* 
    Keeping track of the number of rows
*/
const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

/*
    Common Node Header Layout
*/
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE = 
                NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE; 

/*
    Leaf Node Header Layout
*/
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = 
                COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

/*
    Leaf Node Body Layout
*/
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET = 
                LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = 
                LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = 
                PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS = 
                LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

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

/* Cursor functions are peroformed by the following two funtions 
    - Create a cursor at the beginning of the table
    - Create a cursor at the end of the table
    - Access the elements of the row pointed by the cursor
    - Advance the cursor to the next row
*/
Cursor* table_start(Table* table) {

    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->row_num = 0;
    cursor->end_of_table = (table->num_rows == 0);

    return cursor;

}

Cursor* table_end(Table* table) {

    Cursor* cursor = malloc(sizeof(Cursor);
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;

    return cursor;
    
}

// Get a page and handle a cache miss
void* get_page(Pager* pager, uint32_t page_num) {

    if (page_num > TABLE_MAX_PAGES) {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        // A cache miss. Allocate memory and load from file.
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;
 
        // We might save a partial page at the end of the file
        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }

        if (page_num <= num_pages) {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
        
            if (bytes_read == -1) {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;
    
    }

    return pager->pages[page_num];

}

// Function for pointing the position described by the cursor
void* cursor_value(Cursor* cursor) {

    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    
    void* page = get_page(cursor->table->pager, page_num);

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;

}

// Function to advance the cursor to the next row in the table
void cursor_advance(Cursor* cursor) {

    cursor->row_num += 1;
    if (cursor->row_num >= cursor->table->num_rows) {
        cursor->end_of_table = true;
    }

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

// Function to flush the data at the end of the db file
void* pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {

    if (pager->pages[page_num] == NULL) {

        printf("Tried to flush null page.\n");
        exit(EXIT_FAILURE);

    }

    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if (offset == -1) {

        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);

    }

    ssize_t bytes_written = 
        write(pager->file_descriptor, pager->pages[page_num], size);
    
    if (bytes_written == -1) {

        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    
    }

}

// Flush page cache to disk, close the file, free memory for
// the pager and Table data structures
void db_close(Table* table) {
    
    Pager* pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

    for (uint32_t i = 0; i < num_full_pages; i++) {

        if (pager->pages[i] == NULL) {
            continue;
        }

        pager_flush(pager, i, PAGE_SIZE);
        free(pager->pages[i]);
        pager->pages[i] = NULL;

    }
    
    // There may be a partial page to write to the end of this file
    // This would not be needed after we switch to a B-Tree
    uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0) {

        uint32_t page_num = num_full_pages;

        if (pager->pages[page_num] != NULL) {

            pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
            free(pager->pages[page_num]);
            pager->pages[page_num] = NULL;

        }

    }

    int result = close(pager->file_descriptor);
    if (result == -1) {

        printf("Error closing the db file.\n");
        exit(EXIT_FAILURE);

    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {

        void* page = pager->pages[i];
        if (page) {

            free(page);
            pager->pages[i] = NULL;
        
        }
    
    }

    free(pager);
    free(table);
}

// Wrapper that handles non-SQL commands like '.exit' and leaves room for more 
// such commands
MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        db_close(table);
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

ExecuteResult execute_insert(Statement* statement, Table* table) {

    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Cursor* cursor = table_end(table);
    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, cursor_value(cursor));
    table->num_rows += 1;

    free(cursor);

    return EXECUTE_SUCCESS;

}

ExecuteResult execute_select(Statement* statement, Table* table) {

    Row row;
    Cursor* cursor = table_start(table);
   
    while (!cursor->end_of_table) {

        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);

    }

    free(cursor);

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

    if (fd == -1) {
        printf("Unable to open file.\n");
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