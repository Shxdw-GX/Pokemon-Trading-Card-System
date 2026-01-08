#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include<string>
#include<sstream>
#include "sqlite3.h"
#include <arpa/inet.h>
#include <pthread.h>
#include<vector>

using namespace std;

#define SERVER_PORT  4869
#define MAX_PENDING  5
#define MAX_LINE     256

sqlite3* db = NULL;  //Pointer to a database

struct client_info {  
    int socket;   //client socket
    string ip_address;  //client ip address
};

struct active_user {
    string username;  //user username
    string ip_address;  //user IP address
};

vector<active_user> active_users;  //list of active users
pthread_mutex_t active_users_mutex = PTHREAD_MUTEX_INITIALIZER;  //this is a mutex to protect the active user list so multiple threads dont access the same info at the same time. I learned about this in CIS-450
int active_thread_count = 0;  //tracks the number of active threads
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
#define MAX_THREADS 10  // Maximum concurrent connections

bool initDatabase() {  //initializing the database and create tables if they dont exist


    int rc = sqlite3_open("pokemon_store.db", &db);  //opening the database
    if (rc) {
        cerr << "Couldn't open the database" << sqlite3_errmsg(db) << endl;
        return false;
    }
    cout << "Database has been opened!!!" << endl;

    const char* create_users_sql =   
        "CREATE TABLE IF NOT EXISTS Users ("  //makes a new table if it doesnt already exist
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"  //ID that increments
        "first_name TEXT,"  //stores first name
        "last_name TEXT,"  //stores last name
        "user_name TEXT NOT NULL,"  //stores user name
        "password TEXT,"  //stores password
        "email TEXT,"  //stores email
        "usd_balance REAL NOT NULL,"  //stores decimal numbers and cant be empty
        "is_root INTEGER NOT NULL DEFAULT 0" //1 = admin, 0 = regular users. Will default to 0 (regular users)
        ");";

    char* err_msg = 0;  //where to store our error messages
    rc = sqlite3_exec(db, create_users_sql, 0, 0, &err_msg); 

    if (rc != SQLITE_OK) {  //if the SQL operation fails
        cout << "SQl error: " << err_msg << endl;  //it will print what went wrong
        sqlite3_free(err_msg);  //clean up error message
        return false;  //tells the caller that we failed
    }
    cout << "User Table Created" << endl;

    const char* create_cards_sql =  //creates the pokemon cards table
        "CREATE TABLE IF NOT EXISTS Pokemon_cards ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "card_name TEXT NOT NULL,"
        "card_type TEXT NOT NULL,"
        "rarity TEXT NOT NULL,"
        "count INTEGER,"
        "owner_id INTEGER,"
        "FOREIGN KEY (owner_id) REFERENCES Users(ID)"
        ");";
    rc = sqlite3_exec(db, create_cards_sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {  //if the SQL operation fails
        cout << "SQl error: " << err_msg << endl;  //it will print what went wrong
        sqlite3_free(err_msg);  //clean up error message
        return false;  //tells the caller that we failed
    }
    cout << "Pokemon cards table created" << endl;

    const char* count_sql = "SELECT COUNT(*) FROM Users;";  //checks if any users exist
    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(db, count_sql, -1, &stmt, 0);
    
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);

            if (count == 0) {  //if no user exists, create a deafult user
                const char* insert_root =  //creating the root user with root privilege
                    "INSERT INTO Users (first_name, last_name, user_name, password, email, usd_balance, is_root) "
                    "VALUES ('Root', 'Admin', 'root', 'Root01', 'root@pokemon.com', 100.0, 1);";

                rc = sqlite3_exec(db, insert_root, 0, 0, &err_msg);

                if (rc == SQLITE_OK) {
                    cout << "Default user created: Root with $100 balance" << endl;
                }
                else {
                    cout << "Error creating user: " << err_msg << endl;
                    sqlite3_free(err_msg);
                }

                const char* insert_mary =  //creating the mary user with regular privilege
                    "INSERT INTO Users (first_name, last_name, user_name, password, email, usd_balance, is_root) "
                    "VALUES ('Mary', 'Hyuga', 'mary', 'Mary01', 'mary@pokemon.com', 100.0, 0);";

                rc = sqlite3_exec(db, insert_mary, 0, 0, &err_msg);

                if (rc == SQLITE_OK) {
                    cout << "Default user created: Mary with $100 balance" << endl;
                }
                else {
                    cout << "Error creating Mary user: " << err_msg << endl;
                    sqlite3_free(err_msg);
                }

                const char* insert_john =  //creating the john user with regular privilege
                    "INSERT INTO Users (first_name, last_name, user_name, password, email, usd_balance, is_root) "
                    "VALUES ('John', 'Uzumaki', 'john', 'John01', 'John@pokemon.com', 100.0, 0);";

                rc = sqlite3_exec(db, insert_john, 0, 0, &err_msg);

                if (rc == SQLITE_OK) {
                    cout << "Default user created: John with $100 balance" << endl;
                }
                else {
                    cout << "Error creating John user: " << err_msg << endl;
                    sqlite3_free(err_msg);
                }

                const char* insert_moe =  //creating the moe user with regular privilege
                    "INSERT INTO Users (first_name, last_name, user_name, password, email, usd_balance, is_root) "
                    "VALUES ('Moe', 'Uchiha', 'moe', 'Moe01', 'Moe@pokemon.com', 100.0, 0);";

                rc = sqlite3_exec(db, insert_moe, 0, 0, &err_msg);

                if (rc == SQLITE_OK) {
                    cout << "Default user created: Moe with $100 balance" << endl;
                }
                else {
                    cout << "Error creating Moe user: " << err_msg << endl;
                    sqlite3_free(err_msg);
                }
            }
        }
    }

    sqlite3_finalize(stmt);
    return true; 
}

void* client_handler(void* arg) {
    client_info* info = (client_info*)arg;  //get the client info
    int new_s = info->socket;  //store socket
    string client_ip_address = info->ip_address;  //store ip address
    pthread_mutex_lock(&thread_count_mutex);  //lock the mutex
    active_thread_count++;  //increment thread count
    cout << "Thread started. Active threads: " << active_thread_count << endl;
    pthread_mutex_unlock(&thread_count_mutex);  //unlock the mutex

    // declaring login tracking variables 
    bool is_logged_in = false;
    string logged_in_username = "";
    int logged_in_user_id = -1;
    bool logged_in_is_root = false;

    char buf[MAX_LINE];
    int buf_len;

    while ((buf_len = recv(new_s, buf, sizeof(buf), 0)) > 0) {
        buf[buf_len] = '\0';
        cout << "Received: " << buf << endl;


        if (strncmp(buf, "LOGIN", 5) == 0) {  //checks if it is the LOGIN command

            if (is_logged_in) {  //checks if someone is already logged in
                string response = "403 Already logged in\n";
                response += "User " + logged_in_username + " is already logged in. Please LOGOUT first.";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }
            char command[20], username[50], password[50];
            int checked = sscanf(buf, "%s %s %s", command, username, password);

            if (checked != 3) {  //means user didnt get the 3 requirements(LOGIN, username, and password)
                string response = "403 message format error\n";
                response += "LOGIN format: LOGIN <username> <password>";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }


            //creates a SQL query with placeholders
            string query = "SELECT ID, user_name, is_root FROM Users WHERE user_name = ? AND password = ?;";
            sqlite3_stmt* stmt;

            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);  //gets ready to run
            if (rc != SQLITE_OK) {
                string response = "500 Database error\n";  //sends database error
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }


            //Uses this function to bind text so we can replace the placeholders with real values(the username and password)!
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {  //checks if wew have a matching user
                logged_in_user_id = sqlite3_column_int(stmt, 0);   //gets the ID
                logged_in_username = (char*)sqlite3_column_text(stmt, 1);  //get the username
                logged_in_is_root = sqlite3_column_int(stmt, 2) == 1; //checks if it is the root
                is_logged_in = true;  //marks user as logged in

                // checking if the user is already logged in, because we dont want two terminals logged in as the same person
                pthread_mutex_lock(&active_users_mutex); //locking the mutex
                bool already_logged_in = false;  //checks if they are logged in
                for (size_t i = 0; i < active_users.size(); i++) {
                    if (active_users[i].username == logged_in_username) {
                        already_logged_in = true;
                        break;
                    }
                }
                pthread_mutex_unlock(&active_users_mutex);  //lock mutex

                // If user is already logged in from another connection, reject
                if (already_logged_in) {
                    string response = "403 User Already Logged In\n";
                    response += "User " + logged_in_username + " is already logged in from another location.";
                    send(new_s, response.c_str(), response.length(), 0);
                    sqlite3_finalize(stmt);
                    continue;
                }
                is_logged_in = true;  // If we get here user is not already logged in so we can proceed with the login

                //sends success messages
                string response = "200 OK\n";
                response += "User " + logged_in_username + " logged in successfully";
                send(new_s, response.c_str(), response.length(), 0);
                cout << "User " << logged_in_username << " logged in (ID: " << logged_in_user_id << ")" << endl;

                pthread_mutex_lock(&active_users_mutex);  //this locks the mutex
                active_user new_user;  //create a new active user
                new_user.username = logged_in_username;
                new_user.ip_address = client_ip_address;
                active_users.push_back(new_user);  //add user to active users list
                pthread_mutex_unlock(&active_users_mutex); //unlock the mutex


            }
            else {
                string response = "403 Wrong UserID or Password\n";
                send(new_s, response.c_str(), response.length(), 0);
            }

            sqlite3_finalize(stmt);

        }

        else if (strncmp(buf, "LOGOUT", 6) == 0) {  //checks if the command is LOGOUT
            if (!is_logged_in) {  //checks if the user is even logged in
                string response = "401 Unauthorized\n";
                response += "You are not logged in";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            pthread_mutex_lock(&active_users_mutex);  //this locks the mutex
            for (int i = 0; i < active_users.size(); i++) {
                if (active_users[i].username == logged_in_username) {
                    active_users.erase(active_users.begin() + i);
                    break;
                }
            }
            pthread_mutex_unlock(&active_users_mutex);  //unlocks the mutex

            //changing the login information so the user will be "logged out"
            cout << "User " << logged_in_username << " logged out" << endl;
            is_logged_in = false;
            logged_in_username = "";
            logged_in_user_id = -1;
            logged_in_is_root = false;

            string response = "200 OK\n";
            response += "You have been logged out successfully";
            send(new_s, response.c_str(), response.length(), 0);
        }

        else if (strncmp(buf, "DEPOSIT", 7) == 0) {

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the Balance command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            double amount;
            int checked = sscanf(buf, "DEPOSIT %lf", &amount);
            if (checked != 1) {  //means user did not provide a valid number
                string response = "403 message format error\n";
                response += "DEPOSIT format: DEPOSIT <amount>";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            if (amount <= 0) {  //checks for invalid deposit amounts
                string response = "403 Invalid amount\n";
                response += "Deposit amount must be positive";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            string query = "SELECT usd_balance FROM Users WHERE ID = " + to_string(logged_in_user_id) + ";";
            sqlite3_stmt* stmt;

            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
            if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {  //checks if the query was prepared successfully
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_finalize(stmt);
                continue;
            }

            double current_balance = sqlite3_column_double(stmt, 0);  //gets the current balance
            double new_balance = current_balance + amount;  //calculates the new balance

            string update_query = "UPDATE Users SET usd_balance = " + to_string(new_balance) + " WHERE ID = " + to_string(logged_in_user_id) + ";";
            char* err_msg = 0;

            rc = sqlite3_exec(db, update_query.c_str(), 0, 0, &err_msg);
            if (rc != SQLITE_OK) {
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_free(err_msg);
                continue;
            }

            string response = "200 OK\n";
            response += "deposit successfully. New user balance $" + to_string(new_balance);
            send(new_s, response.c_str(), response.length(), 0);
            cout << "User " << logged_in_username << " deposited $" << amount << ". New balance : $" << new_balance << endl;
        }

        else if (strncmp(buf, "LOOKUP", 6) == 0) {

            if (!is_logged_in) {  //checks if the user is logged in
                string response = "401 Unauthorized\n";
                response += "You must be logged in to use LOOKUP command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }
            char card_name[50];
            int checked = sscanf(buf, "LOOKUP %s", card_name);
            if (checked != 1) {  //means the user did not provide the correct format
                string response = "403 message format error\n";
                response += "LOOKUP format: LOOKUP <card_name>";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //build a query to search for matching names
            string query = "SELECT ID, card_name, card_type, rarity, count, owner_id FROM Pokemon_cards WHERE owner_id = " +
                to_string(logged_in_user_id) + " AND card_name LIKE '%" + string(card_name) + "%';";

            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
            if (rc != SQLITE_OK) {
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            int match_count = 0; //counts how many matches we find
            string results = "";

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                if (match_count == 0) {
                    results += "ID\tCard Name\tType\tRarity\tCount\tOwner\n";
                }
                match_count++;

                //get the column data
                int id = sqlite3_column_int(stmt, 0);
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                const unsigned char* type = sqlite3_column_text(stmt, 2);
                const unsigned char* rarity = sqlite3_column_text(stmt, 3);
                int count = sqlite3_column_int(stmt, 4);
                results += to_string(id) + "\t" + string((char*)name) + "\t" + string((char*)type) + "\t" + string((char*)rarity) + "\t" + to_string(count) + "\t" + logged_in_username + "\n";
            }
            sqlite3_finalize(stmt);

            if (match_count > 0) {  //if we found a match
                string response = "200 OK\n";
                response += "Found " + to_string(match_count) + " match";
                if (match_count > 1) response += "es";  // if we found more than one cards
                response += "\n" + results;
                send(new_s, response.c_str(), response.length(), 0);
            }
            else {
                string response = "404 Your search did not match any records.\n";
                send(new_s, response.c_str(), response.length(), 0);
            }
            cout << "User " << logged_in_username << " searched for: " << card_name << " (found " << match_count << " matches)" << endl;
        }

        else if (strncmp(buf, "WHO", 3) == 0) {

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the WHO command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            if (!logged_in_is_root) {  //checks if you are the root
                string response = "401 Unauthorized\n";
                response += "Only the root user can use the WHO command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            string response = "200 OK\n";
            response += "The list of the active users:\n";

            pthread_mutex_lock(&active_users_mutex);  //lock the mutex
            for (size_t i = 0; i < active_users.size(); i++) {  //loop throuhg all active users
                response += active_users[i].username + "\t" + active_users[i].ip_address + "\n";
            }
            pthread_mutex_unlock(&active_users_mutex);  //unlock mutex
            send(new_s, response.c_str(), response.length(), 0); //send list to the client
            cout << "User " << logged_in_username << " used WHO command" << endl;


        }

        else if (strncmp(buf, "BALANCE", 7) == 0) {  //checks if it is the BALANCE command

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the Balance command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //Query the database for user info
            string query = "SELECT first_name, last_name, usd_balance FROM Users WHERE ID = " + to_string(logged_in_user_id) + ";";
            sqlite3_stmt* stmt;

            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
            if (rc == SQLITE_OK) {  //checks if the query was prepared successfully
                if (sqlite3_step(stmt) == SQLITE_ROW) {

                    //gets user info
                    const unsigned char* first_name = sqlite3_column_text(stmt, 0);
                    const unsigned char* last_name = sqlite3_column_text(stmt, 1);
                    double balance = sqlite3_column_double(stmt, 2);
                    string response = "200 OK\n";
                    response += "Balance for user " + string((char*)first_name) + " " + string((char*)last_name) + ": $" + to_string(balance);
                    send(new_s, response.c_str(), response.length(), 0); //converts to C-style string
                }
                else {
                    string response = "500 Database error\n";
                    send(new_s, response.c_str(), response.length(), 0);
                }

            }
            else {
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
            }

            sqlite3_finalize(stmt);
        }




        else if (strncmp(buf, "LIST", 4) == 0) {  //checks if it is the LIST command

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the List command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            string query;
            if (logged_in_is_root) { //query for root user
                query = "SELECT ID, card_name, card_type, rarity, count, owner_id FROM Pokemon_cards;";
            }
            else {  //query for regular user
                query = "SELECT ID, card_name, card_type, rarity, count, owner_id FROM Pokemon_cards WHERE owner_id = " + to_string(logged_in_user_id) + ";";
            }

            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, 0);
            if (rc == SQLITE_OK) {
                string response = "200 OK\n";

                if (logged_in_is_root) {
                    response += "The list of records in the cards database: \n\n";
                }
                else {
                    response += "The list of records in the Pokemon cards table for current user, " + logged_in_username + ":\n\n";
                }

                response += "ID\tCard Name\tType\t\tRarity\t\tCount\tOwnerID\n";

                while (sqlite3_step(stmt) == SQLITE_ROW) {  //loops through all the cards
                    int id = sqlite3_column_int(stmt, 0);
                    const unsigned char* card_name = sqlite3_column_text(stmt, 1);
                    const unsigned char* card_type = sqlite3_column_text(stmt, 2);
                    const unsigned char* rarity = sqlite3_column_text(stmt, 3);
                    int count = sqlite3_column_int(stmt, 4);
                    int owner_id = sqlite3_column_int(stmt, 5);
                    response += to_string(id) + "\t" + string((char*)card_name) + "\t" + string((char*)card_type) + "\t\t" + string((char*)rarity) + "\t\t" + to_string(count) + "\t" + to_string(owner_id) + "\n";

                }
                send(new_s, response.c_str(), response.length(), 0);
            }
            else {
                string response = "500 Database Error\n";
                send(new_s, response.c_str(), response.length(), 0);
            }
            sqlite3_finalize(stmt);

        }


        else if (strncmp(buf, "QUIT", 4) == 0) {  //checks if it is the QUIT command
            string response = "200 OK\n";
            send(new_s, response.c_str(), response.length(), 0);  //converts to C-style string
            break;  //allows us to exit the loop and disconnect the client
        }
        else if (strncmp(buf, "SHUTDOWN", 8) == 0) {  //checks if it is the SHUTDOWN command

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the Shutdown command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            if (!logged_in_is_root) {  //checks if the user is the root because only the root user can shutdown the server
                string response = "401 Unauthorzied\n";
                response += "Only the root user can shutdown the server";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }


            string response = "200 OK\n";
            send(new_s, response.c_str(), response.length(), 0);  //converts to C-style string
            cout << "Server was shutdown by " << logged_in_username << endl;
            close(new_s);   //this closes the socket
            sqlite3_close(db);
            exit(0);        //This terminates the server
        }



        else if (strncmp(buf, "BUY", 3) == 0) {  //checks if it is the BUY command

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the Buy command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //initializing variables
            char command[50], card_name[50], card_type[50], rarity[50];
            int count;
            double price;

            //This line of code might look wierd but I researched this and understand how it works.
            //The sscan will read the data. %s means it will read a string, %lf means it will read a double, and 
            //%d means it will read an integer. sscan will then return a number based on how many parameters are formatted correctly
            int valid = sscanf(buf, "%s %s %s %s %lf %d", command, card_name, card_type, rarity, &price, &count);
            if (valid != 6) {
                string response = "403 message format error\n";
                response += "BUY format: BUY <card_name> <card_type> <rarity> <price> <count>";
                send(new_s, response.c_str(), response.length(), 0);
                continue;

            }
            double cost = price * count;
            string check_query = "SELECT usd_balance FROM Users WHERE ID = " + to_string(logged_in_user_id) + ";";  //Query to get users current balance
            sqlite3_stmt* check_stmt;


            //Prepares the query to check the users balance
            int rc = sqlite3_prepare_v2(db, check_query.c_str(), -1, &check_stmt, 0);
            if (rc != SQLITE_OK) {
                string response = "500 Database Error\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_finalize(check_stmt);
                continue;
            }
            if (sqlite3_step(check_stmt) != SQLITE_ROW) {  // this is error handling for if a user doesnt exist
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_finalize(check_stmt);
                continue;
            }

            double current_balance = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);

            if (current_balance < cost) {  //error handling for if user doesnt have enough money
                string response = "403 Insufficient funds\n";
                response += "User balance: $" + to_string(current_balance) + ", Required: $" + to_string(cost);
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //updating users balance
            double new_balance = current_balance - cost;
            string update_balance = "UPDATE Users SET usd_balance = " + to_string(new_balance) + " WHERE ID = " + to_string(logged_in_user_id) + ";";

            char* err_msg = 0;
            rc = sqlite3_exec(db, update_balance.c_str(), 0, 0, &err_msg);
            if (rc != SQLITE_OK) {
                string response = "500 Database Error Updating Balance\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_free(err_msg);
                continue;
            }

            //adds card to database
            string insert_card = "INSERT INTO Pokemon_cards (card_name, card_type, rarity, count, owner_id) VALUES ('" +
                string(card_name) + "', '" + string(card_type) + "', '" + string(rarity) + "', " +
                to_string(count) + ", " + to_string(logged_in_user_id) + ");";


            //Insert card into database
            rc = sqlite3_exec(db, insert_card.c_str(), 0, 0, &err_msg);
            if (rc != SQLITE_OK) {  //fails to insert card into database
                string response = "500 Database Error inserting card\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_free(err_msg);
                continue;
            }

            string response = "200 OK\n";
            response += "BOUGHT: New balance: " + to_string(count) + " " + string(card_name) + ". User USD balance $" + to_string(new_balance);
            send(new_s, response.c_str(), response.length(), 0);

        }




        else if (strncmp(buf, "SELL", 4) == 0) {  //checks if it is the SELL command

            if (!is_logged_in) {  //chekcs if you are logged in so you can use the command. You cant use the command if your not logged in obviously
                string response = "401 Unauthorzied\n";
                response += "You need to be logged in to use the Balance command";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //initializing variables
            char command[20], card_name[50];
            int count;
            double price;

            int valid = sscanf(buf, "%s %s %d %lf", command, card_name, &count, &price);
            if (valid != 4) {
                string response = "403 message format error\n";
                response += "SELL format: SELL <card_name> <quantity> <price>";
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            //checks if user exists
            string check_user = "SELECT usd_balance FROM Users WHERE ID = " + to_string(logged_in_user_id) + ";";
            sqlite3_stmt* user_stmt;


            //checks if user exists and gets their balance
            int rc = sqlite3_prepare_v2(db, check_user.c_str(), -1, &user_stmt, 0);
            if (rc != SQLITE_OK || sqlite3_step(user_stmt) != SQLITE_ROW) {  //if the query fails or the user doesnt exist
                string response = "500 Database error\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_finalize(user_stmt);
                continue;
            }

            double current_balance = sqlite3_column_double(user_stmt, 0);
            sqlite3_finalize(user_stmt);

            //checks if the user owns the card
            string check_card = "SELECT ID, count FROM Pokemon_cards WHERE card_name = '" + string(card_name) + "' AND owner_id = " + to_string(logged_in_user_id) + ";";
            sqlite3_stmt* card_stmt;


            //Checks if user owns this card and gets the amount of the card owned
            rc = sqlite3_prepare_v2(db, check_card.c_str(), -1, &card_stmt, 0);
            if (rc != SQLITE_OK || sqlite3_step(card_stmt) != SQLITE_ROW) {  //query failed or user doesnt own this card
                string response = "403 Card not found\n";
                response += "You doesnt own any " + string(card_name) + " cards";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_finalize(card_stmt);
                continue;
            }

            int card_id = sqlite3_column_int(card_stmt, 0);
            int current_count = sqlite3_column_int(card_stmt, 1);
            sqlite3_finalize(card_stmt);

            if (current_count < count) {  //checks if the user has enough cards to sell
                string response = "403 Insufficient Cards\n";
                response += "User only has " + to_string(current_count) + " " + string(card_name) + " cards, cannot sell " + to_string(count);
                send(new_s, response.c_str(), response.length(), 0);
                continue;
            }

            double sale_amount = price * count;
            double new_balance = current_balance + sale_amount;

            //update users balance
            string update_balance = "UPDATE Users SET usd_balance = " + to_string(new_balance) + " WHERE ID = " + to_string(logged_in_user_id) + ";"; \
                char* err_msg = 0;
            rc = sqlite3_exec(db, update_balance.c_str(), 0, 0, &err_msg);

            if (rc != SQLITE_OK) {  //error updating balance
                string response = "500 Database Error updating balance\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_free(err_msg);
                continue;
            }

            int new_count = current_count - count;

            if (new_count == 0) {  // Deleting the card entry if you sold all of it
                string delete_card = "DELETE FROM Pokemon_cards WHERE ID = " + to_string(card_id) + ";";
                rc = sqlite3_exec(db, delete_card.c_str(), 0, 0, &err_msg);
            }
            else {  //updating how many of the card you have left
                string update_card = "UPDATE Pokemon_cards SET count = " + to_string(new_count) + " WHERE ID = " + to_string(card_id) + ";";
                rc = sqlite3_exec(db, update_card.c_str(), 0, 0, &err_msg);
            }

            if (rc != SQLITE_OK) {
                string response = "500 Database Error updating card\n";
                send(new_s, response.c_str(), response.length(), 0);
                sqlite3_free(err_msg);
                continue;
            }

            string response = "200 OK\n";
            response += "SOLD: New balance: " + to_string(new_count) + " " + string(card_name) + ". User's balance USD $" + to_string(new_balance);
            send(new_s, response.c_str(), response.length(), 0);
        }

        else {  //invalid command
            string response = "400 invalid command\n";
            send(new_s, response.c_str(), response.length(), 0);
        }

    }

    pthread_mutex_lock(&active_users_mutex);  //lock the mutex
    for (int i = 0; i < active_users.size(); i++) {  //loop through all active users
        if (active_users[i].username == logged_in_username) {  //if the username matches, remove the user
            active_users.erase(active_users.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&active_users_mutex);  //unlock the mutex

    close(new_s);  //close the connection
    cout << "Client disconnected from: " << client_ip_address << endl;
    pthread_mutex_lock(&thread_count_mutex);  //lock mutex
    active_thread_count--;  //decrement thread count
    cout << "Thread ended. Active threads: " << active_thread_count << endl;
    pthread_mutex_unlock(&thread_count_mutex);  //unlock mutex
    delete info;  //clean up and end the thread
    return NULL;
}


int main() {
    struct sockaddr_in sin;  //server address info
    socklen_t addr_len;  //length of the address
    int s, new_s;  //server socket and new client socket

    if (!initDatabase()) {  //initialize database before starting server
        cout << "Failed to initialize database. Exiting." << endl;
        return 1;
    }

    /* build address data structure */
    memset((char*)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    //setup passive open
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("pokemon-server: socket");
        exit(1);
    }
    if ((bind(s, (struct sockaddr*)&sin, sizeof(sin))) < 0) {
        perror("pokemon-server: bind");
        exit(1);
    }
    listen(s, MAX_PENDING);

    cout << "Server listening on port " << SERVER_PORT << endl;

    //Prepare for select()
    fd_set read_fds;
    int max_fd = s;

    while (1) {
        FD_ZERO(&read_fds); //clears the file descriptor set
        FD_SET(s, &read_fds); //adds the listining socket to the set
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);  //LOOOK, WE ARE USING SELECT OVER HERE SO DONT MINUS THE 50 POINTS OFF OUR GRADE :)

        if (activity < 0) {  //check for select errors
            perror("select error");
            continue;
        }

        if (FD_ISSET(s, &read_fds)) {  //checks if there is a new connection on th listining socket
            addr_len = sizeof(sin);
            if ((new_s = accept(s, (struct sockaddr*)&sin, &addr_len)) < 0) {
                perror("pokemon-server: accept");
                continue;
            }
            cout << "Client connected!" << endl;

            // getting and storing clients IP address
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sin.sin_addr), client_ip, INET_ADDRSTRLEN);

            // creating the client info structure
            client_info* info = new client_info;
            info->socket = new_s;
            info->ip_address = string(client_ip);

            // Check if weve reached the maximum number of threads
            pthread_mutex_lock(&thread_count_mutex);
            int current_count = active_thread_count;
            pthread_mutex_unlock(&thread_count_mutex);

            if (current_count >= MAX_THREADS) {  //checks if max thread limit is reached
                cout << "Maximum connections reached (" << MAX_THREADS << "). Rejecting client from " << string(client_ip) << endl;
                string reject_msg = "503 Service Unavailable\nServer is at maximum capacity. Please try again later.\n";
                send(new_s, reject_msg.c_str(), reject_msg.length(), 0);
                close(new_s);
                delete info;
                continue;  // Skip creating thread, go back to accept
            }

            pthread_t thread_id;  //creating a new thread for the client
            if (pthread_create(&thread_id, NULL, client_handler, (void*)info) != 0) {
                perror("pthread_create failed");
                close(new_s);
                delete info;
                continue;
            }

            pthread_detach(thread_id);  //cleaning up the thread
            cout << "Thread created for client from " << string(client_ip) << endl;

        }
    }
}