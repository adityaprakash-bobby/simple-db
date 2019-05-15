# simple-db [WIP]

An in-memory database created in C. 

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