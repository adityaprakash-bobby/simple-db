#include "enumerators.h"

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

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
    uint32_t num_pages;
    void* pages[TABLE_MAX_PAGES];

};

typedef struct Pager_t Pager;

// Structure to keep track of the pages of the rows
struct Table_t {

    uint32_t root_page_num;
    Pager* pager;
};

typedef struct Table_t Table;

// Cursor structure to point to a location in the table
struct Cursor_t {

    Table* table;
    uint32_t page_num;
    uint32_t cell_num;
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
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
/* 
    Function to get the NODE type
*/
NodeType get_node_type(void* node) {

    uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
    return (NodeType)value;

}

/*
    Function to set the NODE type
*/
void set_node_type(void* node, NodeType type) {

    uint8_t value = type;
    *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;

}

/*
    Accessing Leaf Nodes
*/
uint32_t* leaf_node_num_cells(void* node) {
    return node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* leaf_node_cell(void* node, uint32_t cell_num) {
    return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
    return leaf_node_cell(node, cell_num);
}

void* leaf_node_value(void* node, uint32_t cell_num) {
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void* node) {
    set_node_type(node, NODE_LEAF);
    *leaf_node_num_cells(node) = 0;
}

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

        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
    
    }

    return pager->pages[page_num];

}

/* 
    Function to allocate a new page to the nodes. Now,
    allocates the new page at the end of database file.
    After implementing deletion, we can re-allocate 
    unused pages.
*/
uint32_t get_unused_page_num(Pager* pager) {
    return pager->num_pages;
}

// Function to allocate new page to store the left child
void create_new_root(Table* table, uint32_t right_child_page_num) {

    /*
        Handle splitting the root.
        Copying the old root to the left child (to new page).
        Address of right child passed in.
        Re-initialize root page to contain the new root node.
        New root node points to the two childs.
    */

    void* root = get_page(table->pager, table->root_page_num);
    void* right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void* left_child = get_page(table->pager, left_child_page_num);

    // Left child has data copied from the old root
    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    // Set the root as new internal node with two children
    initialize_internal_node(root);
    /* ADD FURTHER LOGIC HERE */

}

// Function for inserying a key-value pair into a leaf node
// in case of a full node
void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {

    /*
        Create a new node and move half the cells over. Insert the 
        new value in one of the two nodes. Update the parent or
        create a new parent.
    */

   void* old_node = get_page(cursor->table->pager, cursor->page_num);
   uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
   void* new_node = get_page(cursor->table->pager, new_page_num);
   initialize_leaf_node(new_node);

    /*
        All existing keys plus new key should be divided evenly between
        old (left) and new (right) nodes. Starting from right, move each
        key to correct position.
    */

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
        
        void* destination_node;

        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
            destination_node = new_node;
        }
        else {
            destination_node = old_node;
        }

        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void* destination = leaf_node_cell(destination_node, index_within_node);

        if (i == cursor->cell_num) {
            serialize_row(value, destination);
        }
        else if (i > cursor->cell_num) {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    
    }

    /*
        Update cell count on both the leaf nodes
    */

    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    if (is_node_root(old_node)) {
        return create_new_root(cursor->table, new_page_num);
    }   
    else {

        printf("Need to implement updating parent after split.\n");
        exit(EXIT_FAILURE);

    }

}

// Function for inserting a key-value pair into a leaf node
void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {

    void* node = get_page(cursor->table->pager,cursor->page_num);

    uint32_t num_cells = *leaf_node_num_cells(node);
    if (num_cells >= LEAF_NODE_MAX_CELLS) {

        // Node is full
        leaf_node_split_and_insert(cursor, key, value);
        return;

    }

    if (cursor->cell_num < num_cells) {

        // Make room for new cell
        for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
            
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), 
                    LEAF_NODE_CELL_SIZE);
        
        }

    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));

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
    cursor->page_num = table->root_page_num;
    cursor->cell_num = 0;

    void* root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;

}

/*
    Function to binary search for a key in the leaf 
    node since internal node has not been implemented 
    yet
*/
Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key) {

    void* node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    // Binary search
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;

    while (min_index != one_past_max_index) {

        uint32_t mid_index = (min_index + one_past_max_index) / 2;
        uint32_t key_at_mid_index = *leaf_node_key(node, mid_index);

        if (key == key_at_mid_index) {
            
            cursor->cell_num = mid_index;
            return cursor;
        
        }

        if (key < key_at_mid_index) {
            one_past_max_index = mid_index;
        } 

        else {
            min_index = mid_index + 1;
        }

    }

    cursor->cell_num = min_index;
    return cursor;

}

/*
    Return the position of the given key
    If the key is not present, return the position
    where it should be inserted
*/
Cursor* table_find(Table* table, uint32_t key) {

    uint32_t root_page_num = table->root_page_num;
    void* root_node = get_page(table->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF) {
        return leaf_node_find(table, root_page_num, key);
    }

    else {
        printf("Need to implement searching an internal node.\n");
        exit(EXIT_FAILURE);
    }

}

// Function for pointing the position described by the cursor
void* cursor_value(Cursor* cursor) {

    uint32_t page_num = cursor->page_num;
    
    void* page = get_page(cursor->table->pager, page_num);

    return leaf_node_value(page, cursor->cell_num);

}

// Function to advance the cursor to the next row in the table
void cursor_advance(Cursor* cursor) {

    uint32_t page_num = cursor->page_num;
    void* node = get_page(cursor->table->pager, page_num);
    
    cursor->cell_num += 1;
    
    if (cursor->cell_num >= *leaf_node_num_cells(node)) {
        cursor->end_of_table = true;
    }

}

// Print a prompt for the user
void print_prompt() {
    printf("db>");
}

// Function to print out the necessary constants
void print_constants() {

    printf("ROW_SIZE: %d\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);

}

// Function to visualize the btree representation
void print_leaf_node(void* node) {

    uint32_t num_cells = *leaf_node_num_cells(node);
    printf("leaf (size %d)\n", num_cells);

    for (uint32_t i = 0; i < num_cells; i++) {

        uint32_t key = *leaf_node_key(node, i);
        printf("  - %d  :  %d\n", i, key);

    }

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
// Takes up the complete page, even it's not full
void* pager_flush(Pager* pager, uint32_t page_num) {

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
        write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
    
    if (bytes_written == -1) {

        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    
    }

}

// Flush page cache to disk, close the file, free memory for
// the pager and Table data structures
void db_close(Table* table) {
    
    Pager* pager = table->pager;

    for (uint32_t i = 0; i < pager->num_pages; i++) {

        if (pager->pages[i] == NULL) {
            continue;
        }

        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;

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

    else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_leaf_node(get_page(table->pager, 0));
        return META_COMMAND_SUCCESS;
    }

    else if (strcmp(input_buffer->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
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

    void* node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = (*leaf_node_num_cells(node));
    
    Row* row_to_insert = &(statement->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;
    Cursor* cursor = table_find(table, key_to_insert);

    if (cursor->cell_num < num_cells) {
        
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert) {
            return EXECUTE_DUPLICATE_KEY;
        }
    
    }

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

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
    pager->num_pages = (file_length / PAGE_SIZE);

    if (file_length % PAGE_SIZE != 0) {
        printf("Db file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {

        pager->pages[i] = NULL;

    }

    return pager;
}

Table* db_open(const char* filename) {

    Pager* pager = pager_open(filename);
    
    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if (pager->num_pages == 0) {
        // New db file. Initialize page 0 as the leaf node
        void* root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
    }

    return table;

}