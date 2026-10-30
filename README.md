# Dictionary with Auto-Suggestions

A simple dictionary application developed using **C++**, **HTML**, **CSS**, and **JavaScript**. The project uses the **Trie data structure** to provide word searching and auto-suggestions based on the prefix entered by the user.

## Features
- Search for words and view their meanings.
- Get auto-suggestions while typing.
- Insert new words along with their meanings.
- View all the words stored in the dictionary.
- Interactive and user-friendly interface.
- C++ backend connected to the frontend through a local HTTP server.

## Technologies Used
- **C++** – Backend logic and Trie data structure.
- **HTML** – Structure of the web interface.
- **CSS** – Styling and layout.
- **JavaScript** – Frontend interactions and API communication.

## Data Structure Used
**Trie (Prefix Tree):** Used to store words character by character, making prefix-based searching and auto-suggestions efficient.

## How to Run
1. Compile the C++ file:
   ```bash
   g++ dictionary.cpp -o dictionary
   ```
   On Windows using MinGW:
   ```bash
   g++ dictionary.cpp -o dictionary -lws2_32
   ```
2. Run the executable:
   ```bash
   ./dictionary
   ```
3. Open `http://localhost:8080` in your browser.

## Project Objective
The objective of this project is to understand the practical implementation of the Trie data structure and demonstrate how data structures can be integrated with web technologies to create an interactive application.
