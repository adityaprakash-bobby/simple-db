# simple-db [WIP]

A Sqlite-like database written in pure C. 

#### Execute the program

```bash
# compile the program
gcc main.c -o db

# run the executable
./db <db-filename>

# check the memory pattern in the db file
vim <db-filename>
:%!xxd
```

#### Features worked on till now:

 - Basic REPL design.
 - Execute insert and select operations.
 - A single hard-coded users table.
 - In-memory append-only database structure.
 - Adding persistence to the database file created by user.
 - B+Tree structure for indexing the database.
 - DB now consists of a single leaf B Tree structure with size of 4096 bytes (can hold 13 key-value pairs, after which it throws `Table Full` error)
