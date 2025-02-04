/**
 * @file scan.c
 * @author Charles Averill
 * @brief Functions for lexical scanning from input source files
 * @date 08-Sep-2022
 */

#include <ctype.h>

#include "data.h"
#include "scan.h"

/**
 * @brief Get the next valid character from the current input file
 * 
 * @return char Next valid character from the current input file
 */
static char next(void)
{
    // Get next valid character from file

    char c;

    // If we have to put a character back into the stream
    if (D_PUT_BACK) {
        c = D_PUT_BACK;
        D_PUT_BACK = 0;
        return c;
    }

    // Get next character from file
    c = fgetc(D_INPUT_FILE);

    // Check line increment
    if (c == '\n') {
        D_LINE_NUMBER++;
    }

    return c;
}

/**
 * @brief Put a character back into the input stream
 * 
 * @param c Character to be placed into the input stream
 */
static void put_back_into_stream(char c) { D_PUT_BACK = c; }

/**
 * @brief Skips whitespace tokens
 * 
 * @return char The next non-whitespace Token
 */
static char skip(void)
{
    char c;

    // Get next character until not hit space, tab, newline, carriage return, form feed
    do {
        c = next();
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f');

    return c;
}

/**
 * @brief Return the index of char c in string s
 * 
 * @param s String to search in
 * @param c Character to search for
 * @return int Index of c in s if s contains c, else -1
 */
static int index_of(char* s, int c)
{
    char* p = strchr(s, c);

    if (p) {
        return p - s;
    } else {
        return -1;
    }
}

/**
 * @brief Scan and return an integer literal from the input stream
 * 
 * @param c Current character
 * @return int Scanned integer literal
 */
static int scanint(char c)
{
    int k = 0;   // Current digit
    int val = 0; // Output

    while ((k = index_of("0123456789", c)) >= 0) {
        val = val * 10 + k;
        c = next();
    }

    // Loop has terminated at a non-integer value, so put it back
    put_back_into_stream(c);

    return val;
}

/**
 * @brief Determine if a character in an identifier is a permitted character
 * 
 * @param c Character to check
 * @param index Index of character in identifier string
 * @return bool True if character is permitted
 */
static bool is_valid_identifier_char(char c, int index)
{
    return (index != 0 && isdigit(c)) || isalpha(c) || c == '_' || c == '$';
}

/**
 * @brief Scan an alphanumeric identifier
 * 
 * @param c First character in identifier
 * @param buf Buffer to read identifier into
 * @param max_len Maximum length of the identifier
 * @return int Length of the identifier
 */
static int scan_identifier(char c, char* buf, int max_len)
{
    int i = 0;

    while (is_valid_identifier_char(c, i)) {
        if (i >= max_len - 1) {
            syntax_error(D_INPUT_FN, D_LINE_NUMBER,
                         "Identifier name has exceeded maximum length of %d", max_len);
        }

        buf[i++] = c;
        c = next();
    }

    // Loop exits on prohibited identifier character, so put it back
    put_back_into_stream(c);
    buf[i] = '\0';

    return i;
}

/**
 * @brief Retrieve the TokenType value corresponding to a keyword string
 * 
 * @param keyword_string String to convert to TokenType
 * @return TokenType TokenType of the keyword, or 0 if the keyword is not recognized
 */
static TokenType parse_keyword(char* keyword_string)
{
    switch (keyword_string[0]) {
    case 'p':
        if (!strcmp(keyword_string, TTS_PRINT)) {
            return T_PRINT;
        }
    }

    return 0;
}

/**
 * @brief Check if the current character is the start of an integer literal
 * 
 * @param c Character to check
 * @return bool True if the current character is the start of an integer literal
 */
static bool scan_check_integer_literal(char c) { return isdigit(c); }

/**
 * @brief Check if the current character is the start of a keyword or identifier
 * 
 * @param c Character to check
 * @return bool True if the current character is the start of a keyword or identifier
 */
static bool scan_check_keyword_identifier(char c) { return is_valid_identifier_char(c, 0); }

/**
 * @brief Scan tokens into the Token struct
 * 
 * @param t Token to scan data into
 * @return bool Returns true if a Token was scanned successfully
 */
bool scan(Token* t)
{
    char c;
    TokenType temp_type;

    // Skip whitespace
    c = skip();

    bool switch_matched = true;
    switch (c) {
    case EOF:
        t->type = T_EOF;
        return false;
    case '+':
        t->type = T_PLUS;
        break;
    case '-':
        t->type = T_MINUS;
        break;
    case '*':
        if ((c = next()) == '*') {
            t->type = T_EXPONENT;
        } else {
            put_back_into_stream(c);
            t->type = T_STAR;
        }
        break;
    case '/':
        t->type = T_SLASH;
        break;
    case ';':
        t->type = T_SEMICOLON;
        break;
    default:
        switch_matched = false;
        break;
    }

    if (switch_matched) {
        return true;
    }

    bool no_switch_match_output = true;
    // Check if c is an integer
    if (scan_check_integer_literal(c)) {
        t->value = scanint(c);
        t->type = T_INTEGER_LITERAL;
    } else if (scan_check_keyword_identifier(c)) {
        // Scan identifier string into buffer
        scan_identifier(c, D_IDENTIFIER_BUFFER, D_MAX_IDENTIFIER_LENGTH);

        // Check if identifier is a keyword
        if (temp_type = parse_keyword(D_IDENTIFIER_BUFFER)) {
            t->type = temp_type;
        } else {
            syntax_error(D_INPUT_FN, D_LINE_NUMBER, "Unrecognized identifier \"%s\"",
                         D_IDENTIFIER_BUFFER);
        }
    } else {
        no_switch_match_output = false;
        syntax_error(D_INPUT_FN, D_LINE_NUMBER, "Unrecognized token \"%c\"", c);
    }

    return no_switch_match_output;
}
