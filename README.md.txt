Pokémon Trading Card System

Author: Senan Aboluhom (senan@umich.edu)
Overview:
• Platform: Linux (on UMD servers) 
• Programming Language: C++ 
• Database: SQLite3 
• Port Number: 4869 

Compilation Instructions 
• Ensure you have a **g++ compiler** installed. 
• Make sure `sqlite3.c` and `sqlite3.h` are in the project directory. 
• Move into the directory: cd pokemon_trading 
• To compile the server and client we can run make all to compile both 
• To start the server run the following command: ./pokemon_server -Will open or create the pokemon_store.db -Will create Users and Pokemon_cards table if they dont already exist -Creates a 4 users: mary, john, moe, and root with a balance of $100 -Listens for the client on port 4869 
• To start the client run the following command: ./pokemon_client -If the hostname isn't provided, the client defaults to localhost(127.0.0.1) 
• Can connect up to 10 users at the same time 

Implemented Commands 
•BUY: Allows you to purchase pokemon cards
-Format: BUY <card_name> <card_type> <rarity> <price> <count> <owner_id>
-Example: BUY Pikachu Electric Common 19.99 2 1
•SELL: Allows you to sell pokemon cards from your inventory
- Format: SELL <card_name> <quantity> <price> <owner_id>
- Example: SELL Pikachu 1 34.99 1
• LOGIN: Log into the system -Format: LOGIN<username><password> 
-example: LOGIN mary Mary01  
• LOGOUT: Log out of the system -Format: LOGOUT -Example: LOGOUT 
• DEPOSIT: Add money to your account -Format: DEPOSIT <amount> -Example: DEPOSIT 50.00 
• LIST: View your card collection. Has been modified so if the user uses this 
command, it will display all cards that all users have -Format: LIST -Example: LIST 
• WHO: List all active users. Only the root can use this command -Format: WHO -Example: WHO 
• LOOKUP: Search for any cards by name. The name can be a full or partial match -Format: LOOKUP <card_name> -Example: LOOKUP Char 
•BALANCE: Displays the USD balance for a user 
- Format: BALANCE <user_id>
- Example: BALANCE 1
•QUIT: Disconnect client from server
- Format: QUIT
• SHUTDOWN: Shut down the server. Only the root can use this command -Format: SHUTDOWN -Example: SHUTDOWN 
The server provides error handling for the following cases: 
• 400 invalid command – Unrecognized command sent to server 
• 403 message format error – Incorrect command format or missing parameters 
• 403 Insufficient Funds – User does not have enough money to complete purchase 
• 403 Insufficient Cards – User does not have enough cards to sell requested quantity 
• 403 Card not found – User does not own the specified card type 
• 404 User Not Found – User ID does not exist in database 
• 500 Database Error – Database operation failed 
• 503 Server capacity – Limit exceeded for server capacity  
 
Demo Video 
A video demonstration of all commands and error handling: 
https://youtu.be/Y640xIvn12o 
 
 
 
 
 