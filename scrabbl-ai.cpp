/**
 * scrabbl-ai.cpp
 *
 * This code is the property of its creator W. Lei.
 *
 * Purpose: I created this project for the IB English Scrabble tournament.
 *          It uses a variant of Appel and Jacobson's algorithm to create a
 *          computer program that can play scrabble. The algorithm is simplified
 *          since it uses a trie rather than a dawg.
 *
 * References: https://pdfs.semanticscholar.org/da31/
 *                  cb24574f7c881a5dbf008e52aac7048c9d9c.pdf
 *             https://web.stanford.edu/class/cs221/2017/restricted/p-final/
 *                  cajoseph/final.pdf
 *
 *
 * Contact Email: leiw9425@gmail.com
 */

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <unordered_map>
#include <regex>
#include <cctype>

#define TILES_FILE_NAME "tiles.txt"
#define WORDS_FILE_NAME "common_1000_words.txt"
#define BOARD_FILE_NAME "board.txt"
#define TESTGAME_FILE_NAME "test_game_across.txt"
#define NUM_BOARD_ROWS 15
#define NUM_BOARD_COLS 15
#define NUM_RACK_TILES 7

using namespace std;

enum SquareType
{
    triple_word,
    double_word,
    triple_letter,
    double_letter,
    regular,
    outside
};

struct Square
{
    SquareType type;
    vector <bool> down_cross_check;
    char letter; // Special values: '.' = empty square and
                 //                 lowercase letter = blank tile
    int row;
    int col;
    int min_across_word_length;
};

struct Tile
{
    char letter;
    int points;
    int total;
};

struct TrieNode
{
    char letter; // Special value: "*" if root node
    bool is_terminal_node;

    vector <TrieNode*> children;

    // Stores the index of each letter of each children node
    // Ex. If a child has a letter 'C' at children [1],
    //     then letters_present['C'-'A'] == letters_present[2] == 1
    vector <int> letter_indexes {vector <int> (26, -1)};
};

typedef vector <Square> SquareRow;
typedef vector <SquareRow> SquareGrid;
typedef unordered_map <string, int> Lexicon;

// Declare functions
Lexicon read_word_data ();
TrieNode* create_word_trie ();
void insert_into_trie (TrieNode* root, string word);
void print_word_trie (TrieNode* node);
vector <Tile> read_tile_data ();
SquareGrid read_board_data ();
void read_test_game_data (SquareGrid &board);
void update_down_cross_checks (SquareGrid &board);
void update_min_across_word_length (SquareGrid &board);
vector <int> fill_rack (string letters);
void find_best_move (SquareGrid board, vector <int> rack,
                     vector <Square> &best_move, int &best_pts);
vector <Square> find_best_across_move (SquareGrid board, vector <int> rack);
vector <Square> find_best_down_move (SquareGrid board, vector <int> rack);
void extend_right (SquareGrid* board, vector <int> rack, TrieNode* node,
                   Square curr_square, int min_word_length,
                   vector <Square> curr_move, vector <Square> &best_move,
                   int &best_pts);
void add_sqr_to_move (int row, int col, char letter, vector <Square> &curr_move);
int calc_across_pts (SquareGrid* board, vector <Square> curr_move);
int calc_col_cross_pts (SquareGrid* board, int row, int col);
int calc_down_pts (SquareGrid* board, vector <Square> down_move);
SquareGrid invert_board (SquareGrid board);
vector <Square> invert_move (vector <Square> across_move);
void output_board (SquareGrid board);

// Get the data for the tiles and words to be stored in global variables
Lexicon global_words = read_word_data();
TrieNode* global_trie_root = create_word_trie();
vector <Tile> global_tiles = read_tile_data();

/**
 * @return  an unordered map of strings containing all the words in the scrabble
 *          dictionary. The key is type string since it is stores the word. The
 *          mapped value is type integer since it stores if the word is worth a
 *          bonus multiplier.
 */
Lexicon read_word_data ()
{
    // Declare vector to store all of the words in the scrabble dictionary
    Lexicon words;

    // Open file containing the word data
    ifstream word_data_file;
    string file_name = WORDS_FILE_NAME;
    word_data_file.open(file_name.c_str(), ifstream::in);

    // Ensure data file is open
    if (!word_data_file.is_open())
    {
        cout << "Could not open " << file_name << endl;
        return words;
    }

    // Loop through all the words and add them to the map
    while (word_data_file.good())
    {
        // IMPORTANT: The words must all be in uppercase.
        string word;
        word_data_file >> word;

        // Insert the word into the unordered map
        words[word] = 1;
    }

    return words;
}

/**
 * @param   words   an unordered map of words retrieved from a text file
 * @return          a pointer to a TrieNode that is the root of the trie
 */
TrieNode* create_word_trie ()
{
    TrieNode* root = new TrieNode;
    root->letter = '*';
    root->is_terminal_node = false;

    for (auto itr = global_words.begin(); itr != global_words.end(); itr++)
    {
        string str = itr->first;
        regex all_uppercase ("[A-Z]+");

        if (str.length() > 2 && regex_match(str, all_uppercase))
        {
            insert_into_trie(root, str);
        }
    }

    return root;
}

/**
 * Inserts TrieNodes into the trie to store the word in the data structure.
 *
 * @param   root    a pointer a TrieNode that is the root of the trie
 * @param   words   the string of letters to be inserted
 */
void insert_into_trie (TrieNode* root, string word)
{
    TrieNode* curr_node = root;

    // Go through each letter in the word -> each letter is word[i]
    for (unsigned int i = 0; i < word.length(); i++)
    {
        // Calculate the index for the letter_indexes property
        // for the letter in the word
        // letter_index allows the program to determine whether a
        // child has the letter word[i] in O(1) time
        int letter_index = word[i] - 'A';

        // Check to see if there are no children with the letter in the word
        if (curr_node->letter_indexes[letter_index] == -1)
        {
            // Create a new node
            TrieNode* new_node = new TrieNode;
            new_node->letter = word[i];
            new_node->is_terminal_node = false;

            // Update the current node by adding new_node as a child
            curr_node->children.push_back(new_node);

            // Also, update the letter_indexes property by storing
            // the index of the child where the letter word[i] can be found
            // It is the index of the last child in the children property
            // since the node was just added
            curr_node->letter_indexes[letter_index] =
                                            curr_node->children.size() - 1;
        }

        // Go to the child of curr_node that contains the letter in the word
        int child_index = curr_node->letter_indexes[letter_index];
        curr_node = curr_node->children[child_index];
    }

    curr_node->is_terminal_node = true;
}

/**
 * Prints a word trie to the console. Uses recursive calls to go down the trie.
 *
 * @param   node    a TrieNode pointer of a node containing the data for a node
 */
void print_word_trie (TrieNode* node)
{
    // Output the node's letter property
    cout << node->letter << endl;

    // Go through all the node's children's letters
    for (unsigned int i = 0; i < node->children.size(); i++)
    {
        cout << node->children[i]->letter << " ";
    }

    cout << endl << endl;

    // Print each child of the node
    for (unsigned int i = 0; i < node->children.size(); i++)
    {
        print_word_trie(node->children[i]);
    }
}

/**
 * @return  a vector of Tiles with each tile object containing the right data.
 */
vector <Tile> read_tile_data ()
{
    // Declare vector to store all the Tiles
    vector <Tile> tiles;

    // Open file containing the letter data
    ifstream letter_data_file;
    string file_name = TILES_FILE_NAME;
    letter_data_file.open(file_name.c_str(), ifstream::in);

    // Ensure data file is open
    if (!letter_data_file.is_open())
    {
        cout << "Could not open " << file_name << endl;
        return tiles;
    }

    // Loop through all 27 possible tiles and add them to the vector
    for (int i = 0; i < 27; i++)
    {
        Tile tile;
        letter_data_file >> tile.letter >> tile.points >> tile.total;
        tiles.push_back(tile);
    }

    return tiles;
}

/**
 * @return  a SquareGrid containing the data for each square on the board.
 *          Key for the text file's characters:
 *              W = Triple Word Score
 *              w = Double Word Score
 *              L = Triple Letter Score
 *              l = Double Letter Score
 *              . = Regular Square
 *              * = Square is out of bounds
 */
SquareGrid read_board_data ()
{
    // Declare board vector
    SquareGrid board;

    // Open file containing the board data
    ifstream in_file;
    string file_name = BOARD_FILE_NAME;
    in_file.open(file_name.c_str(), ifstream::in);

    // Ensure file is open
    if (!in_file.is_open())
    {
        cout << "Could not open " << file_name << endl;
        return board;
    }

    // Declare vectors and variables that will become properties of the board
    vector <bool> all_invalid (26, false);
    vector <bool> all_valid (26, true);
    int row_num = 0;

    // Get all the rows in the board
    // The rows of x's around the actual board are to ensure that
    // tiles are not added outside the board
    while (in_file.good())
    {
        // Read a line from the text file
        string line;
        in_file >> line;

        // Declare a row of Squares to store the data for each row
        SquareRow row;

        // Go through all the characters in each line
        for (unsigned int i = 0; i < line.size(); i++)
        {
            Square sqr;
            sqr.letter = '.';
            sqr.row = row_num;
            sqr.col = i;

            // Assign the Square type to sqr
            switch (line[i])
            {
                case 'W': sqr.type = triple_word;   break;
                case 'w': sqr.type = double_word;   break;
                case 'L': sqr.type = triple_letter; break;
                case 'l': sqr.type = double_letter; break;
                case '.': sqr.type = regular;       break;
                case 'x': sqr.type = outside;       break;
            }

            // Assign the cross_check_letters vector to sqr
            switch (line[i])
            {
                case 'x':
                    sqr.down_cross_check = all_invalid;
                    sqr.letter = '.';
                    break;
                default:
                    sqr.down_cross_check = all_valid;
                    break;
            }

            row.push_back(sqr);
        }

        board.push_back(row);
        row_num++;
    }

    return board;
}

/**
 * @param   stream  an output stream object to output to console
 * @param   type    an object of class SquareType
 * @return          an output stream object that is outputted
 */
ostream &operator << (ostream &stream, SquareType type)
{
    switch (type)
    {
        case triple_word:   stream << "triple_word";   break;
        case double_word:   stream << "double_word";   break;
        case triple_letter: stream << "triple_letter"; break;
        case double_letter: stream << "double_letter"; break;
        case regular:       stream << "regular";       break;
        case outside:       stream << "outside";       break;
    }

    return stream;
}

/**
 * Fills the board with letters which are read from a text file.
 *
 * @param   board   a square grid containing the data for the state of the game
 *                  This parameter is passed by reference since the board is
 *                  being modified.
 */
void read_test_game_data (SquareGrid &board)
{
    // Open file containing the data
    ifstream in_file;
    string file_name = TESTGAME_FILE_NAME;
    in_file.open(file_name.c_str(), ifstream::in);

    // Ensure file is open
    if (!in_file.is_open())
    {
        cout << "Could not open " << file_name << endl;
    }

    // Go through all the rows
    for (int row = 0; row < NUM_BOARD_ROWS; row++)
    {
        // Get each row as input
        string input;
        in_file >> input;

        // Go through all the rows
        for (int col = 0; col < NUM_BOARD_COLS; col++)
        {
            // row+1 and row+1 are used since the top row and column
            // (row 0 and column 0) of board are used to mark outside squares
            // Fill in the tiles on the board
            board[row+1][col+1].letter = input[col];
        }
    }
}

/**
 * Updates the down_cross_check property of each square in the board
 * Ex. board[row][col].down_cross_check[3] == true indicates that the letter 'D'
 *     (since 'D' - 'A' == 3) can be placed at board[row][col]
 *
 * @param   board   a SquareGrid containing the data for the state of the game
 *                  This parameter is passed by reference since the board is
 *                  being modified.
 */
void update_down_cross_checks (SquareGrid &board)
{
    // Go through all the squares in the board where tiles can be placed
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        for (int col = 1; col <= NUM_BOARD_COLS; col++)
        {
            // Only check squares on which tiles can be placed
            if (board[row][col].letter == '.')
            {
                string above_square, below_square;
                int check_row = row - 1;

                // Add characters above the cross-check square
                while (board[check_row][col].letter != '.' &&
                       board[check_row][col].type   != outside)
                {
                    above_square = (char) toupper(board[check_row][col].letter)
                                   + above_square;
                    check_row--;
                }

                check_row = row + 1;

                // Add characters below the cross-check square
                while (board[check_row][col].letter != '.' &&
                       board[check_row][col].type   != outside)
                {
                    below_square = below_square +
                                   (char) toupper(board[check_row][col].letter);
                    check_row++;
                }

                // No need to update if there are blanks squares above and below
                if (above_square == "" && below_square == "")
                {
                    continue;
                }

                // Go through all 26 of the letters that could possibly
                // occupy board[row][col]
                for (int test_letter = 'A'; test_letter <= 'Z'; test_letter++)
                {
                    string test_word = above_square + (char)(test_letter)
                                        + below_square;

                    // Find in the words unordered map hash table
                    // If it is found, then make that letter true (or valid)
                    // in the down_cross_check property
                    if (global_words.find(test_word) != global_words.end())
                    {
                        board[row][col].down_cross_check
                                            [test_letter-'A'] = true;
                    }
                    else
                    {
                        board[row][col].down_cross_check
                                            [test_letter-'A'] = false;
                    }
                }
            }
        }
    }
}

/**
 * Updates the min_across_word_length property of every square on the board.
 * This property stores the minimum length of the word going across starting from
 * that square so that the word created connects with pre-existing words.
 * Ex. If board[row][col].min_across_word_length == 4 indicates that a word must
 *     be 4 letters long before it connects with pre-existing words.
 *     Otherwise, the word will be disconnected.
 *
 *
 * @param   board   a SquareGrid containing the data for the state of the game
 *                  This parameter is passed by reference since the board is
 *                  being modified.
 */
void update_min_across_word_length (SquareGrid &board)
{
    // Go through all the rows
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        // Set the minimum word length as -1 to signify
        // squares rightward of any adjacent square
        // These squares cannot be used as the leftmost square from which
        // to extend rightwards
        int min_word_length = -1;

        // Go through all the squares in the row from right to left
        for (int col = NUM_BOARD_COLS; col >= 1; col--)
        {
            // If the square to its immediate left is occupied with a letter,
            // then the square at board[row][col] cannot be the left-most square
            // Thus, min_across_word_length == -1
            if (board[row][col-1].letter != '.')
            {
                board[row][col].min_across_word_length = -1;
            }
            // Check to see if there are tiles above, below,
            // right, or on the square
            // If so, then set the min_across_word_length to 1
            else if (board[row-1][col].letter != '.' ||
                     board[row+1][col].letter != '.' ||
                     board[row][col+1].letter != '.' ||
                     board[row][col].letter   != '.' )
            {
                board[row][col].min_across_word_length = 1;
                min_word_length = 1;
            }
            // For squares on the extreme right which cannot be used to
            // build a word since there are no squares to the right from
            // which tiles can be added.
            // Ie. Extending right from this square will always create a word
            // that is separated from the rest of the words already on the board.
            else if (min_word_length == -1)
            {
                board[row][col].min_across_word_length = -1;
            }
            // These squares are not adjacent to any square, but extending right
            // will eventually reach a square
            else
            {
                min_word_length++;
                board[row][col].min_across_word_length = min_word_length;
            }
        }
    }
}

/**
 * Returns a vector of integers which represents the letters on a Scrabble rack.
 * These letters are available to be placed on the board.
 *
 * @param   letters     a string of all the letters in the rack
 * @return              a vector of 26 integers where each element represents
 *                      the number of tiles of that letter.
 *                      Ex. rack[4] == 2 indicates 2 E's are in the rack
 */
vector <int> fill_rack (string letters)
{
    vector <int> rack (27, 0);

    // Set the number of characters to read as
    // the min of NUM_RACK_TILES and the length of the string "letters"
    int num_chars_read = (NUM_RACK_TILES > letters.length()) ?
                         (letters.length()) : (NUM_RACK_TILES);

    // Go through all the necessary characters to read
    for (int i = 0; i < num_chars_read; i++)
    {
        // For regular tiles
        if (isupper(letters[i]))
        {
            rack[letters[i] - 'A']++;
        }
        // For blank tiles
        else if (letters[i] == '*')
        {
            rack[26]++;
        }
    }

    return rack;
}

/**
 * Find the highest scoring possible move and the points obtained based on
 * board and rack.
 *
 * @param   board   stores the state of the Scrabble board
 * @param   rack    stores the number of each possible tile
 * @param   best_move   stores the highest scoring move and is passed by
 *                      reference
 * @param   best_pts    stores the highest scoring points and is passed by
 *                      reference
 */
void find_best_move (SquareGrid board, vector <int> rack,
                     vector <Square> &best_move, int &best_pts)
{
    // Go through all the squares to check for any squares that have tiles
    // It will find the best move and exit the function
    // as soon as it finds a tile
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        for (int col = 1; col <= NUM_BOARD_COLS; col++)
        {
            // Check to see if the square has a tile
            // Only find the best move if a square on the board has a tile
            if (board[row][col].letter != '.')
            {
                // Get the best move for placing tiles across and
                // for placing tiles down
                vector <Square> best_across_move =
                                        find_best_across_move(board, rack);
                vector <Square> best_down_move =
                                        find_best_down_move(board, rack);

                // Get the points for placing the best across move
                // and the best down move
                int best_across_pts = calc_across_pts(&board, best_across_move);
                int best_down_pts = calc_down_pts(&board, best_down_move);

                // Select either the best across move/pts or the down move/pts
                if (best_across_pts > best_down_pts)
                {
                    best_move = best_across_move;
                    best_pts = best_across_pts;
                }
                else
                {
                    best_move = best_down_move;
                    best_pts = best_down_pts;
                }

                // Only find the best move once and exit the function
                return;
            }
        }
    }

    //
    // If the function has reached this line, the board is empty
    // This means the program needs to find the best starting move
    // Scrabble rules dictate that the first move must contain 2 or more tiles
    //

    // Find the middle row and column since these determine the
    int mid_row = NUM_BOARD_ROWS/2 + 1;
    int mid_col = NUM_BOARD_COLS/2 + 1;

    // Go through all the squares left of and including the center square
    for (int col = 1; col <= mid_col; col++)
    {
        // Set the minimum length of the first word to be placed so that
        // it covers the center square
        board[mid_row][col].min_across_word_length = mid_col - col + 1;

        // There is an exception for the center square if it is the
        // leftmost square of the starting move
        // One must place at least 2 tiles to start the game, so its
        // min_across_word_length is 2
        if (col == mid_col)
        {
            board[mid_row][mid_col].min_across_word_length = 2;
        }

        // Declare variables necessary to call the function extend_right()
        vector <Square> curr_move;
        Square sqr = board[mid_row][col];
        int min_word_length = sqr.min_across_word_length;

        // Only call extend_right when necessary
        // Ie. When less than 7 characters are needed to connect to
        // pre-existing words AND it is possible to connect to pre-existing
        // words to the right of the square
        if (min_word_length <= NUM_RACK_TILES && min_word_length != -1)
        {
            extend_right(&board, rack, global_trie_root, sqr,
                         min_word_length, curr_move, best_move, best_pts);
        }
    }
}

/**
 * Returns a vector of Squares that is the move that scores the most possible
 * points by placing tiles horizontally for a given Scrabble board and a rack.
 *
 * @param   board   stores the state of the Scrabble board
 * @param   rack    stores the number of each possible tile
 * @return          a vector of Squares storing the highest scoring move
 *                  involving tiles placed horizontally
 */
vector <Square> find_best_across_move (SquareGrid board, vector <int> rack)
{
    // Declare a vector and a variable to store
    // the best move and highest number of points
    vector <Square> best_move;
    int best_pts = 0;

    // Go through all the squares in the board
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        for (int col = 1; col <= NUM_BOARD_COLS; col++)
        {
            // Declare variables necessary to call the function extend_right()
            vector <Square> curr_move;
            Square sqr = board[row][col];
            int min_word_length = sqr.min_across_word_length;

            // Only call extend_right when necessary
            // Ie. When less than 7 characters are needed to connect to
            // pre-existing words AND it is possible to connect to pre-existing
            // words to the right of the square
            if (min_word_length <= NUM_RACK_TILES && min_word_length != -1)
            {
                extend_right(&board, rack, global_trie_root, sqr,
                             min_word_length, curr_move, best_move, best_pts);
            }
        }
    }

    return best_move;
}

/**
 * Returns a vector of Squares that is the move that scores the most possible
 * points by placing tiles vertically for a given Scrabble board and a rack.
 *
 * @param   board   stores the state of the Scrabble board
 * @param   rack    stores the number of each possible tile
 * @return          a vector of Squares storing the highest scoring move
 *                  involving tiles placed vertically
 */
vector <Square> find_best_down_move (SquareGrid board, vector <int> rack)
{
    SquareGrid inverted_board = invert_board(board);

    // Find the best down move by calling the find_best_across_move() function
    // on the inverted board
    vector <Square> best_down_move = find_best_across_move(inverted_board, rack);
    return invert_move(best_down_move);
}

/**
 * Finds the best move by extending rightwards from a given square
 *
 * @param   board               a pointer to the SquareGrid storing the state of
 *                              the board
 * @param   rack                a vector of integers storing the number of each
 *                              type of tile
 * @param   node                the node in the trie storing the last letter added
 *                              to partial_word and the next possible letters
 * @param   curr_square         the square on which a new tile may be placed
 *                              for the current move
 * @param   min_word_length     the minimum word length of the word to be created
 *                              so that it connects with pre-existing words
 * @param   curr_move           the Squares on which tiles have been placed of the
 *                              current move that is being attempted
 * @param   best_move           the best possible move thus far represented by
 *                              a vector of squares
 * @param   best_pts            the greatest number of points achievable by
 *                              a move (ie. best_move) thus far
 */
void extend_right (SquareGrid* board, vector <int> rack, TrieNode* node,
                   Square curr_square, int min_word_length,
                   vector <Square> curr_move, vector <Square> &best_move,
                   int &best_pts)
{
    Square sqr = (*board)[curr_square.row][curr_square.col];

    // If the square is empty then simply return and do nothing
    if (sqr.type == outside)
    {
        return;
    }
    // If the current square is empty
    else if (sqr.letter == '.')
    {
        // Determine if a legal move has been found ie. a word is created and
        // the word is long enough so that it can connect with pre-existing tiles
        if (node->is_terminal_node == true &&
            curr_move.size() >= (unsigned int) min_word_length)
        {
            int curr_pts = calc_across_pts(board, curr_move);

            if (curr_pts > best_pts)
            {
                best_pts = curr_pts;
                best_move = curr_move;
            }
        }
        // Go through all the children of the node
        for (unsigned int i = 0; i < node->children.size(); i++)
        {
            char child_letter = node->children[i]->letter ;
            int child_letter_index = child_letter - 'A';

            // Check to see if the letter of the child is in our rack AND
            // it is in the down_cross_check set of the square
            if (rack[child_letter_index] > 0 &&
                sqr.down_cross_check[child_letter_index])
            {
                // Remove the tile from the rack
                rack[child_letter_index]--;

                // Add the square onto the current move
                add_sqr_to_move(sqr.row, sqr.col,
                                child_letter, curr_move);

                // Move rightwards to the next square
                curr_square = (*board)[sqr.row][sqr.col+1];

                // Recursively call itself to continued extending right
                extend_right(board, rack, node->children[i], curr_square,
                             min_word_length, curr_move, best_move, best_pts);

                // Remove the square from the current move
                curr_move.pop_back();

                // Place tile back in the rack
                rack[child_letter_index]++;

                // Move back to the original square
                curr_square = sqr;
            }
            // Otherwise try using a blank tile
            else if (rack[26] > 0 && sqr.down_cross_check[child_letter_index])
            {
                // Remove the tile from the rack
                rack[26]--;

                // Add the square onto the current move
                add_sqr_to_move(sqr.row, sqr.col,
                                tolower(child_letter), curr_move);

                // Move rightwards to the next square
                curr_square = (*board)[sqr.row][sqr.col+1];

                // Recursively call itself to continued extending right
                extend_right(board, rack, node->children[i], curr_square,
                             min_word_length, curr_move, best_move, best_pts);

                // Remove the square from the current move
                curr_move.pop_back();

                // Place tile back in the rack
                rack[26]++;

                // Move back to the original square
                curr_square = sqr;
            }
        }
    }
    // The square contains a letter
    else
    {
        int sqr_letter_index = toupper(sqr.letter) - 'A';
        int child_index = node->letter_indexes[sqr_letter_index];

        // Check to see if node has a child with the letter occupying the square
        if (child_index != -1)
        {
            // Move rightwards to the next square
            curr_square = (*board)[curr_square.row][curr_square.col+1];

            // Recursively call itself to continued extending right
            extend_right(board, rack, node->children[child_index], curr_square,
                         min_word_length, curr_move, best_move, best_pts);
        }
    }
}

/**
 * Adds the square, on which a tile has just been placed, onto the current move.
 *
 * @param   row         the row of the square to be added
 * @param   col         the column of the square to be added
 * @param   letter      the letter of the square to be added
 * @param   curr_move   the vector storing the current move. This vector is
 *                      passed by reference since it is directly modified.
 */
void add_sqr_to_move (int row, int col, char letter, vector <Square> &curr_move)
{
    Square sqr;
    sqr.row = row;
    sqr.col = col;
    sqr.letter = letter;
    curr_move.push_back(sqr);
}

/**
 * Calculates the number of points obtained for a given across move.
 *
 * @param   board           a pointer to the SquareGrid containing all the data
 *                          for a Scrabble Board
 * @param   across_move     a vector of Squares storing all the squares on which
 *                          a tile has been placed for a given across move
 * @return                  the number of points obtained from an across move
 */
int calc_across_pts (SquareGrid* board, vector <Square> across_move)
{
    // If no squares are in the current move, then no points are awards
    if (across_move.size() == 0)
    {
        return 0;
    }

    // Set the variables that will increment at 0
    int row_pts = 0;
    int total_cross_pts = 0;
    int num_double_word = 0;
    int num_triple_word = 0;

    // Go through all the squares in the current move
    for (unsigned int i = 0; i < across_move.size(); i++)
    {
        // Store the square in the move, its row, column,
        // and number of letter points obtained without any bonuses
        Square sqr = across_move[i];
        int row = sqr.row;
        int col = sqr.col;
        int letter_pts = 0;
        int col_cross_pts = 0;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(sqr.letter))
        {
            letter_pts = global_tiles[sqr.letter - 'A'].points;
        }

        // Account for double letter, or triple letter bonuses
        // by multiplying the points obtained by the letter by 2 or 3
        if ((*board)[row][col].type == double_letter)
        {
            letter_pts *= 2;
        }
        else if ((*board)[row][col].type == triple_letter)
        {
            letter_pts *= 3;
        }

        row_pts += letter_pts;

        // Calculate the number of cross points
        // Column cross points are points obtained by forming vertical words
        // when playing a horizontal word across the board
        if ((*board)[row-1][col].letter != '.' ||
            (*board)[row+1][col].letter != '.')
        {
            col_cross_pts += calc_col_cross_pts(board, row, col);
            col_cross_pts += letter_pts;
        }

        // Account for double or triple word bonuses
        // by recording the number of word bonuses for the row points
        // and multiplying the column cross points by 2 or 3
        if ((*board)[row][col].type == double_word)
        {
            num_double_word++;
            col_cross_pts *= 2;
        }
        else if ((*board)[row][col].type == triple_word)
        {
            num_triple_word++;
            col_cross_pts *= 3;
        }

        total_cross_pts += col_cross_pts;
    }

    // Prepare to go through all the squares left of the move
    int row = across_move[0].row;
    int col = across_move[0].col - 1;

    // Go through all the squares left of the first tile
    // placed in the row for the move
    while ((*board)[row][col].type != outside &&
           (*board)[row][col].letter != '.')
    {
        char letter = (*board)[row][col].letter;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(letter))
        {
            row_pts += global_tiles[letter - 'A'].points;
        }

        col--;
    }

    // Prepare to go through all the squares in between the move
    col = across_move[0].col;

    // Go through all the squares in between the first and last tile
    // placed in the row for the move
    while (col <= across_move[across_move.size()-1].col)
    {
        char letter = (*board)[row][col].letter;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(letter))
        {
            row_pts += global_tiles[letter - 'A'].points;
        }

        col++;
    }

    // Prepare to go through all the squares right of the move
    col = across_move[across_move.size()-1].col + 1;

    // Go through all the squares right of the last tile
    // placed in the row for the move
    while ((*board)[row][col].type != outside &&
           (*board)[row][col].letter != '.')
    {
        char letter = (*board)[row][col].letter;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(letter))
        {
            row_pts += global_tiles[letter - 'A'].points;
        }

        col++;
    }

    // Double the row points for a double word bonus
    for (int i = 1; i <= num_double_word; i++)
    {
        row_pts *= 2;
    }

    // Triple the row points for a double word bonus
    for (int i = 1; i <= num_triple_word; i++)
    {
        row_pts *= 3;
    }

    // If you use 7 tiles in your move, you get a bingo of 50 points
    if (across_move.size() >= 7)
    {
        return row_pts + total_cross_pts + 50;
    }
    else
    {
        return row_pts + total_cross_pts;
    }

}

/**
 * Calculates the number of points obtained from tiles above and below a square.
 * Ex. Hypothetically, if the word "DRAG" and the tiles "M" and "O" are on the
 *     board and then the word "cake" is create horizontally by adding the tiles
 *     "C", "K", and "E". The function
 *     returns the points obtained by placing the tiles "C", "K", and "e" only.
 *
 *     . D . . .          . D . . .
 *     . R . M .   -->    . R . M .
 *     * * * * *          C A K E D
 *     . G . . O          . G . . O
 *
 * @param   board   a SquareGrid containing all the data for a Scrabble Board
 *                  that is being played
 * @param   row     the row number of the square above and below the function
 *                  must calculate the number of column cross points
 * @param   col     the column number of the square above and below the function
 *                  must calculate the number of column cross points
 * @return          the number of points obtained from tiles directly above
 *                  and below a square
 */
int calc_col_cross_pts (SquareGrid* board, int row, int col)
{
    int col_cross_pts = 0;
    int row_original = row;

    // Start one row above the square
    row = row_original - 1;

    // Calculate points formed by letters above the square
    while ((*board)[row][col].type != outside &&
           (*board)[row][col].letter != '.')
    {
        char letter = (*board)[row][col].letter;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(letter))
        {
            col_cross_pts += global_tiles[letter - 'A'].points;
        }

        row--;
    }

    // Now, start on the row below the square
    row = row_original + 1;

    // Calculate points formed by letters below the square
    while ((*board)[row][col].type != outside &&
           (*board)[row][col].letter != '.')
    {
        char letter = (*board)[row][col].letter;

        // Only add points if the letter is uppercase (lowercase = blanks)
        if (isupper(letter))
        {
            col_cross_pts += global_tiles[letter - 'A'].points;
        }

        row++;
    }

    return col_cross_pts;
}

/**
 * Calculates the number of points obtained for a given down move.
 *
 * @param   board       a pointer to the SquareGrid containing all the data for
 *                      a Scrabble Board
 * @param   down_move   a vector of Squares storing all the squares on which a
 *                      tile has been placed for a given down move
 * @return              the number of points obtained from a down move
 */
int calc_down_pts (SquareGrid* board, vector <Square> down_move)
{
    // Invert the board and the move
    SquareGrid inverted_board = invert_board(*board);
    down_move = invert_move(down_move);


    return calc_across_pts(&inverted_board, down_move);
}

/**
 * Inverts a board so that for each board[row][col] == inverted_board[col][row].
 * In other words, it swaps rows and columns.
 *
 * @param   board       a pointer to the SquareGrid containing all the data for
 *                      a Scrabble Board
 * @return              the inverted board
 */
SquareGrid invert_board (SquareGrid board)
{
    // Invert the board by changing rows to columns and vice verse,
    // so that for each Square in board,
    // board[row][col] == inverted_board[col][row]
    SquareGrid inverted_board = board;

    // Fill through all the squares in the inverted board
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        for (int col = 1; col <= NUM_BOARD_COLS; col++)
        {
            inverted_board[row][col] = board[col][row];
            inverted_board[row][col].row = row;
            inverted_board[row][col].col = col;
        }
    }

    // Update the properties of the inverted board
    update_down_cross_checks(inverted_board);
    update_min_across_word_length(inverted_board);

    return inverted_board;
}

/**
 * Inverts a move so that for each Square in the move, it swaps the row and col.
 * In other words, if the row and column of a square are 5 and 8 respectively,
 * the row and column will become 8 and 5 .
 *
 * @param   across_move     a vector of Squares storing the squares on which
 *                          a tile has been placed for a given move
 * @return                  a move with the rows and columns swapped for each
 *                          square
 */
vector <Square> invert_move (vector <Square> across_move)
{
    vector <Square> down_move = across_move;

    for (unsigned int i = 0; i < down_move.size(); i++)
    {
        down_move[i].row = across_move[i].col;
        down_move[i].col = across_move[i].row;
    }

    return down_move;
}

/**
 * Adds a move to the board by placing the appropriate tiles.
 *
 * @param   board   the state of the Scrabble board which is passed by reference
 *                  since it is modified
 * @param   _move   the move containing the Squares upon which have new tiles
 *                  have been placed
 *
 */
void add_move_to_board (SquareGrid &board, vector <Square> _move)
{
    for (unsigned int i = 0; i < _move.size(); i++)
    {
        board[_move[i].row][_move[i].col] = _move[i];
    }

    update_down_cross_checks(board);
    update_min_across_word_length(board);
}

/**
 * Outputs the scrabble board onto the console.
 * Only outputs the letters with heading, row numbers, and column numbers
 *
 * @param   board  the variable storing all the data for the board
 */
void output_board (SquareGrid board)
{
    // String storing the row header that is displayed vertically
    string row_num_header = "    ROW NUMBER        ";

    // Column header
    cout << "            COLUMN NUMBER         " << endl;
    cout << "       2   4   6   8  10  12  14    " << endl;

    // Go through all rows of the scrabble board (usually 15)
    for (int row = 1; row <= NUM_BOARD_ROWS; row++)
    {
        // Output a letter if necessary of the row header
        cout << row_num_header[row] << " ";

        // Output the row number only if it is even
        if (row%2 == 0)
        {
            // Output an extra space if i is only 1 digit
            if (row <= 9)
            {
                cout << " ";
            }

            cout << row << " ";
        }
        else
        {
            cout << "   ";
        }

        // Output every letter on the board (period or . means an empty square)
        for (int col = 1; col <= NUM_BOARD_COLS; col++)
        {
            cout << board[row][col].letter << " ";
        }

        cout << endl;
    }
}

/**
 * Function that is called that allows the user to execute the code which
 * find the best move based on a board and a rack.
 * This function allows the user to change the tiles on the board, change
 * the tiles on the rack, find the best move, and exit.
 */
void run_scrabble ()
{
    // Get the data for the board
    SquareGrid board = read_board_data();
    read_test_game_data(board);
    string rack_str = "ENTIREE";
    vector <int> rack = fill_rack(rack_str);

    // Loop infinitely until the user decides to exit
    while (true)
    {
        // Update the state of the board
        update_down_cross_checks(board);
        update_min_across_word_length(board);

        // Output the board and the rack
        output_board(board);
        cout << endl;
        cout << "RACK TILES: " << rack_str << endl;
        cout << endl;

        // Find the best move
        vector <Square> best_move;
        int best_pts = 0;
        find_best_move(board, rack, best_move, best_pts);

        // Output the best move
        cout << endl;
        cout << "BEST MOVE" << endl;
        cout << "Points: "    << best_pts << endl;

        // Only output the specifics of the move if it exists
        if (best_move.size() > 0)
        {
            cout << "Tiles: " << endl;
            cout << "Start Row: " << best_move[0].row << endl;
            cout << "Start Col: " << best_move[0].col << endl;

            for (unsigned int i = 0; i < best_move.size(); i++)
            {
                cout << best_move[i].letter << " "
                     << best_move[i].row << " "
                     << best_move[i].col << endl;
            }
        }

        cout << endl;

        // Output what the best move would look like
        SquareGrid new_board = board;
        add_move_to_board(new_board, best_move);
        output_board(new_board);

        // Get the next user input
        while (true)
        {
            bool invalid_tile = false;

            // Give the user options
            string input;
            cout << endl;
            cout << "Enter 't' to change a tile on the board." << endl;
            cout << "Enter 'r' to change the tiles in the rack." << endl;
            cout << "Enter 'f' to find the best move." << endl;
            cout << "Enter another key to exit." << endl;
            cin >> input;

            // If the user decides to add a tile to the board
            if (input == "t" || input == "T")
            {
                char letter;
                int row, col;

                // Get input
                cout << "Enter a tile's letter, row, and column "
                     << "separated by spaces:  " << endl;
                cout << "Ex. \"E 4 7\" indicates an 'E' at row 4, col 7." << endl;
                cin >> letter >> row >> col;

                // Ensure the letter is uppercase and the row and col are
                // the right size
                if ( (isalpha(letter) || letter == '.')
                      && 1 <= row && row <= NUM_BOARD_ROWS
                      && 1 <= col && col <= NUM_BOARD_COLS)
                {
                    board[row][col].letter = letter;
                }
                else
                {
                    invalid_tile = true;
                }
            }
            // If the user decides to change the tiles in the rack
            else if (input == "r" || input == "R")
            {
                cout << "Enter the tiles in the rack"
                     << "in uppercase letters and no spaces: ";
                cin >> rack_str;
                rack = fill_rack(rack_str);
            }
            // If the user decides to find the best move
            else if (input == "f" || input == "F")
            {
                system("CLS");
                break;
            }
            // Exits the program
            else
            {
                return;
            }

            // Output the board and the rack
            system("CLS");
            output_board(board);
            cout << endl;
            cout << "RACK TILES: " << rack_str << endl;
            cout << endl;

            // Account for invalid tile input
            if (invalid_tile)
            {
                cout << "Invalid tile input" << endl;
            }
        }
    }

}

int main()
{
    run_scrabble();

    return 0;
}
