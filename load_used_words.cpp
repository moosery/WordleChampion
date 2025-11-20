/*
 * FILE: load_used_words.cpp
 *
 * WHAT:
 * Implements the "Live Data" subsystem.
 * This module uses libcurl to download a specific webpage (Rock Paper Shotgun)
 * that maintains a history of all past Wordle answers. It parses the HTML
 * to extract the words and populates a global exclusion list.
 *
 * WHY:
 * To achieve a 100% win rate in the real world, the bot must know which words
 * have already been the "Word of the Day." The NYT (almost) never repeats answers.
 * By downloading this list live, the bot stays current without code updates.
 * It also includes a "Replay" mechanism to manually un-ban words for testing.
 */

#include "load_used_words.h"
#include <errno.h>
#include <curl/curl.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

 /*
  * CONFIGURATION: Replay List
  *
  * WHAT:
  * A whitelist of words that should be IGNORED by the exclusion filter.
  *
  * WHY:
  * Debugging and Testing. If we want to simulate the specific game where "OPINE"
  * was the answer, but "OPINE" was already used in 2022, the loader would normally
  * mark it as "eliminated" immediately. This list forces the loader to skip that
  * exclusion, allowing the word to be a valid answer for the simulation.
  *
  * HOW TO USE:
  * 1. To Replay: Uncomment the words, and switch g_replay_count to use 'sizeof'.
  * 2. To Disable: Keep the dummy "" string (to satisfy compiler), and set g_replay_count = 0.
  */
static const char* g_replay_words[] = {
    "" // Dummy entry to prevent "empty initializer list" error (E1345)
    // "OPINE",
    // "SALET",
};

// METHOD A: Use this when the list is empty (Prevents logic from reading the dummy)
static const int g_replay_count = 0;

// METHOD B: Use this when the list has words (Calculates size automatically)
// static const int g_replay_count = sizeof(g_replay_words) / sizeof(g_replay_words[0]);


/*
 * FUNCTION: write_callback
 *
 * WHAT:
 * A standard cURL callback function handling received data chunks.
 * It dynamically resizes a memory buffer to accommodate incoming HTML.
 *
 * WHY:
 * Web pages vary in size. We cannot allocate a static buffer. This function
 * effectively implements a `realloc` growth strategy to capture the full
 * page content into a single string for parsing.
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    // Calculate the number of bytes in this specific chunk
    size_t realsize = size * nmemb;
    char** memory = (char**)userp;

    if (*memory == NULL)
    {
        // FIRST CHUNK: Allocate initial buffer.
        // We add +1 for the null terminator we will append.
        *memory = (char*)malloc(realsize + 1);
        if (*memory == NULL) return 0; // Signal error to cURL (abort download)
        memcpy(*memory, contents, realsize);
        (*memory)[realsize] = '\0';
    }
    else
    {
        // SUBSEQUENT CHUNKS: Expand the buffer.
        size_t current_size = strlen(*memory);
        char* ptr = (char*)realloc(*memory, current_size + realsize + 1);
        if (ptr == NULL) return 0; // Signal error (Out of Memory)

        *memory = ptr;
        // Append the new data to the end of the existing string
        memcpy(&((*memory)[current_size]), contents, realsize);
        (*memory)[current_size + realsize] = '\0';
    }
    return realsize;
}

/*
 * FUNCTION: get_used_words_webpage
 *
 * WHAT:
 * Establishes the network connection to the target URL and downloads the raw HTML.
 *
 * WHY:
 * Encapsulates the network I/O logic. Returns a raw string buffer so the
 * parsing logic doesn't need to know about sockets or HTTP headers.
 * Uses a generic User-Agent ("Chrome") to avoid being blocked by basic firewalls.
 */
static char* get_used_words_webpage(void)
{
    CURL* curl;
    CURLcode res;
    char* hugeBuffer = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl)
    {
        // The Source of Truth: Rock Paper Shotgun maintains a clean list.
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.rockpapershotgun.com/wordle-past-answers");

        // Register the callback to handle the incoming data stream
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        // Pass the address of our string pointer so the callback can realloc it
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&hugeBuffer);

        // Vital: Follow 301/302 redirects if the URL has moved
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        // Vital: Pretend to be a real browser to avoid anti-bot blocks
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Chrome");

        // Execute the request (Blocking call)
        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            fprintf(stderr, "cURL failed: %s\n", curl_easy_strerror(res));
            if (hugeBuffer) free(hugeBuffer);
            hugeBuffer = NULL;
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    if (hugeBuffer == NULL)
    {
        fprintf(stderr, "Failed to download webpage content.\n");
    }
    else
    {
        printf("Webpage content downloaded successfully.\n");
    }
    return hugeBuffer;
}

/*
 * FUNCTION: compare
 *
 * WHAT:
 * A simple wrapper for `memcmp` compatible with `qsort`.
 *
 * WHY:
 * We sort the "Used Words" list alphabetically. This allows the main dictionary
 * loader to use `strncmp` (or potentially binary search) to check for existence
 * much faster than scanning an unsorted array.
 */
static int compare(const void* arg1, const void* arg2)
{
    return memcmp(arg1, arg2, WORDLE_WORD_LENGTH);
}

/*
 * FUNCTION: load_used_words_from_web
 *
 * WHAT:
 * The specific parser for the Rock Paper Shotgun HTML structure.
 * 1. Downloads the HTML.
 * 2. Finds the section `<h2>All Wordle answers</h2>`.
 * 3. Iterates through `<li>` items.
 * 4. Extracts the 5-letter word.
 * 5. Checks the "Replay List" (Whitelist).
 * 6. Adds the word to the global exclusion buffer.
 *
 * WHY:
 * Screen scraping is brittle, so this function isolates the parsing logic.
 * If the website changes its layout, we only need to update the `sectionHeader`
 * strings here.
 */
bool  load_used_words_from_web(char** pp_used_words, int* p_used_word_count)
{
    // HTML Markers specific to the target website's layout
    const char* sectionHeader = "<h2>All Wordle answers</h2>";
    const char* wordStartTag = "<li>";
    const char* wordEndTag = "</li>";
    const char* listEndTag = "</ul>";
    const char* nonWordChars = " \t\n\r\v";

    char* pUsedWords_webpage = NULL;
    char* pTmp;
    char* pNextPlaceToStoreAWord;

    char* p_used_words = NULL;
    int  used_word_count = 0;
    // Safety limit: The dictionary is only 10k words, so used words can't exceed that.
    int  max_used_word_count = MAX_DICTIONARY_WORDS;

    printf("Loading Used Words from web ...\n");
    p_used_words = (char*)malloc(max_used_word_count * WORDLE_WORD_LENGTH);

    if (p_used_words == NULL)
    {
        fprintf(stderr, "ERROR: Could not allocate space for used words!\n");
        return false;
    }

    pNextPlaceToStoreAWord = p_used_words;


    // 1. Perform the Download
    pUsedWords_webpage = get_used_words_webpage();

    if (pUsedWords_webpage != NULL)
    {
        // 2. Locate the specific section containing the Wordle answers
        // We use strstr to jump to the header, avoiding the nav/sidebar content.
        pTmp = strstr(pUsedWords_webpage, sectionHeader);
        if (pTmp == NULL) { return false; } // Website layout changed?

        // Find the first list item after the header
        pTmp = strstr(pTmp, wordStartTag);
        if (pTmp == NULL) { return false; }

        // Define the end boundary so we don't parse the footer
        char* pListEnd = strstr(pTmp, listEndTag);
        if (pListEnd == NULL) { pListEnd = pTmp + strlen(pTmp); }

        // 3. Iterate through the List Items
        while (pTmp != NULL && pTmp < pListEnd)
        {
            // Advance past the opening <li> tag
            pTmp += strlen(wordStartTag);
            // Skip any whitespace/newlines between tag and word
            pTmp += strspn(pTmp, nonWordChars);

            // Find the closing tag to ensure we are inside a valid item
            char* pWordEndTag = strstr(pTmp, wordEndTag);
            if (pWordEndTag == NULL || pWordEndTag >= pListEnd) break;

            // Handle nested HTML (e.g., if they bold the word: <li><b>WORD</b></li>)
            if (*pTmp == '<')
            {
                char* pContentStart = strchr(pTmp, '>');
                // Jump inside the inner tag if valid
                if (pContentStart != NULL && pContentStart < pWordEndTag) { pTmp = pContentStart + 1; }
            }

            pTmp += strspn(pTmp, nonWordChars);

            // 4. Validate and Parse the Word
            // We only care about the first 5 alphabetic characters.
            size_t wordLen = 0;
            while (wordLen < WORDLE_WORD_LENGTH && isalpha((unsigned char)*(pTmp + wordLen))) { wordLen++; }

            if (wordLen == WORDLE_WORD_LENGTH)
            {
                // Normalize to Uppercase
                char tempWord[WORDLE_WORD_LENGTH + 1];
                for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
                {
                    tempWord[i] = toupper((unsigned char)pTmp[i]);
                }
                tempWord[WORDLE_WORD_LENGTH] = '\0';

                // 5. Check Replay Whitelist
                // If we are debugging a specific word, we don't want to exclude it.
                bool is_replay = false;
                for (int k = 0; k < g_replay_count; ++k)
                {
                    if (strncmp(tempWord, g_replay_words[k], WORDLE_WORD_LENGTH) == 0)
                    {
                        is_replay = true;
                        printf("Skipping used word '%s' (Found in Replay List)\n", tempWord);
                        break;
                    }
                }

                // 6. Store the Word
                if (!is_replay)
                {
                    // Copy bytes directly to our packed array
                    memcpy(pNextPlaceToStoreAWord, tempWord, WORDLE_WORD_LENGTH);
                    pNextPlaceToStoreAWord += WORDLE_WORD_LENGTH;
                    used_word_count++;
                }
            }

            // 7. Move to the next list item
            char* pNextListItem = strstr(pTmp + 1, wordStartTag);
            char* pEndList = strstr(pTmp + 1, listEndTag);

            // Determine if we hit the next item or the end of the list
            if (pNextListItem != NULL && (pEndList == NULL || pNextListItem < pEndList))
            {
                pTmp = pNextListItem;
            }
            else if (pEndList != NULL)
            {
                pTmp = pListEnd; // Finish loop
            }
            else
            {
                pTmp = NULL; // Error/End
            }
        }
        free(pUsedWords_webpage);
    }

    // 8. Sort the result for fast lookups
    if (p_used_words != NULL)
    {
        qsort(p_used_words, used_word_count, WORDLE_WORD_LENGTH, compare);
    }

    *pp_used_words = p_used_words;
    *p_used_word_count = used_word_count;
    printf("Loaded %d used words from web\n", used_word_count);

    return true;
}


/*
 * FUNCTION: load_used_words
 *
 * WHAT:
 * The public interface for loading the used words list.
 *
 * WHY:
 * Currently, this simply wraps the web scraper. However, keeping this abstraction
 * allows us to swap in a local file loader or a database connection later without
 * breaking the rest of the application code.
 */
bool  load_used_words(char** pp_used_words, int* p_used_word_count)
{
    return load_used_words_from_web(pp_used_words, p_used_word_count);
}