/*
 * PROJECT: Hybrid Wordle Solver & Simulation Engine
 *
 * ARCHITECTURE OVERVIEW:
 * This application is designed as a modular research platform for analyzing Wordle
 * strategies. It separates the core components into distinct layers:
 *
 * 1. Data Layer: Manages the dictionary and "used words" lists. It handles
 * loading, memory management, and data sorting.
 * 2. Logic Layer: Contains the core mathematical engines (Entropy Calculator,
 * Rank/Frequency Analysis) and the Game State logic (filtering words based
 * on constraints).
 * 3. Strategy Layer: The infrastructure allows for "HybridConfig" definitions.
 * Instead of hardcoding bot behavior, we define strategies as data objects
 * containing flags (e.g., use_linguistic_filter, look_ahead_depth). This
 * allows us to run Monte Carlo simulations across dozens of strategy variations
 * without recompiling or rewriting the solver loop.
 *
 * DATA FILE FORMAT (AllWords.txt):
 * The application relies on a specific CSV-like fixed-width format for the
 * dictionary. Each line represents one entry.
 *
 * Domain Values & Offsets:
 * - Offset 0-4 (5 chars): The Word.
 * Example: "SALET", "CRANE"
 * Must be exactly 5 characters, uppercase.
 *
 * - Offset 5-7 (3 chars): Frequency Rank.
 * Example: "100" (Very Common), "000" (Obscure).
 * Used by the Rank Strategy to break entropy ties.
 *
 * - Offset 8   (1 char) : Noun Type.
 * 'P' = Plural Noun    (e.g., "CAKES"). Often bad guesses in Wordle.
 * 'S' = Singular Noun  (e.g., "CAKE"). Good guesses.
 * 'N' = Not a Noun     (e.g., "OF").
 * 'R' = Pronoun        (e.g., "THEIR").
 *
 * - Offset 9   (1 char) : Verb Type.
 * 'T' = Past Tense     (e.g., "BAKED"). Often bad guesses.
 * 'S' = 3rd Person     (e.g., "BAKES"). Often bad guesses.
 * 'P' = Present Tense  (e.g., "BAKE"). Good guesses.
 * 'N' = Not a Verb.
 */

#define MAIN
#include "load_dictionary.h"
#include "duplicate_dictionary.h"
#include "comparators.h"
#include "solver_logic.h" 
#include "entropy_calculator.h" 
#include "monte_carlo.h" 
#include "hybrid_strategies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "wordle_types.h"

 // CONSTANTS: Display formatting limits
const int MAX_ENTRIES_TO_PRINT = 50;
const int MAX_GUESSES = 6;
const int ENTRY_BLOCK_WIDTH = 44;
const int TOTAL_TABLE_WIDTH = 89;
const char* SEPARATOR_TEMPLATE = "------------------------------------------------------------------------------------------------------------------------------------";

// GLOBALS: State flags for the runtime environment
bool g_isHardMode = false;
bool g_isInteractivePlay = true;
int g_tryIdx = 0;

/*
 * FUNCTION: print_final_candidates_aligned_box
 *
 * WHAT:
 * Renders a formatted UI box displaying the top recommendation categories
 * (Entropy Raw, Entropy Filtered, Rank Raw, Rank Filtered) and highlights
 * the specific "Champion Pick" selected by the active bot strategy.
 *
 * WHY:
 * In Interactive Mode, the user needs to see not just the "best" word, but
 * the context of why it was picked. Seeing the Raw vs Filtered split helps
 * the user understand if the bot is making a move based on pure math (Entropy)
 * or linguistic heuristics (Filtered).
 */
static void print_final_candidates_aligned_box(const recommendations_array_t candidates, const dictionary_entry_t* pSmartPick)
{
    // Buffers to hold the formatted strings for the four main categories
    char ent_raw_str[80]; char ent_filt_str[80]; char rank_raw_str[80]; char rank_filt_str[80];

    // Extract pointers for readability
    const dictionary_entry_t* e_raw = candidates[0].pEntry;
    const dictionary_entry_t* e_filt = candidates[1].pEntry;
    const dictionary_entry_t* r_raw = candidates[2].pEntry;
    const dictionary_entry_t* r_filt = candidates[3].pEntry;

    // Format the strings with Word, Entropy Score, and Frequency Rank
    sprintf_s(ent_raw_str, 80, "     Raw: %5.5s E:%.4f R:%03d", e_raw->word, e_raw->entropy, e_raw->frequency_rank);
    sprintf_s(ent_filt_str, 80, "Filtered: %5.5s E:%.4f R:%03d", e_filt->word, e_filt->entropy, e_filt->frequency_rank);
    sprintf_s(rank_raw_str, 80, "     Raw: %5.5s E:%.4f R:%03d", r_raw->word, r_raw->entropy, r_raw->frequency_rank);
    sprintf_s(rank_filt_str, 80, "Filtered: %5.5s E:%.4f R:%03d", r_filt->word, r_filt->entropy, r_filt->frequency_rank);

    // Print the Header Box
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);
    // Complex padding math is used here to center the titles "Top Entropy Choices" and "Top Rank Choices"
    printf("|%*s%s%*s|", (ENTRY_BLOCK_WIDTH - (int)strlen("Top Entropy Choices")) / 2, "", "Top Entropy Choices", (((ENTRY_BLOCK_WIDTH - (int)strlen("Top Entropy Choices")) / 2) - 2) + 1, "");
    printf(" ");
    printf("|%*s%s%*s|\n", ((ENTRY_BLOCK_WIDTH - (int)strlen("Top Rank Choices")) / 2) - 1, "", "Top Rank Choices", (((ENTRY_BLOCK_WIDTH - (int)strlen("Top Rank Choices")) / 2)) - 1, "");
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);

    // Print the Content
    printf("|      %-35s |", ent_raw_str); printf(" "); printf("|      %-35s |\n", rank_raw_str);
    printf("|      %-35s |", ent_filt_str); printf(" "); printf("|      %-35s |\n", rank_filt_str);
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);

    // Highlight the specific word the Bot has chosen (The "Champion")
    if (pSmartPick != NULL)
    {
        char smart_str[100];
        sprintf_s(smart_str, 100, ">>> CHAMPION PICK: %s (R=%03d, H=%.4f) <<<",
            pSmartPick->word, pSmartPick->frequency_rank, pSmartPick->entropy);

        // Center the champion string dynamically based on table width
        int len = (int)strlen(smart_str);
        int padding = (TOTAL_TABLE_WIDTH - len) / 2;
        if (padding < 0) padding = 0;

        printf("|%*s%s%*s|\n", padding, "", smart_str, (TOTAL_TABLE_WIDTH - len - padding - 2), "");
        printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);
    }
}

/*
 * FUNCTION: print_comparison_table_fixed_width
 *
 * WHAT:
 * Prints a detailed side-by-side table comparing the top N words sorted by Entropy
 * against the top N words sorted by Rank.
 *
 * WHY:
 * This visualizes the "trade-off" dilemma. Often, the highest entropy word (best math)
 * is an obscure word (bad rank). This table allows the user to verify if the
 * "Hybrid" logic is correctly identifying words that have a good balance of both.
 */
void print_comparison_table_fixed_width(const dictionary_pointer_array_t p_entropy_sorted, const dictionary_pointer_array_t p_rank_sorted, int count, int requestedN)
{
    // Determine how many rows to print. Cap at MAX_ENTRIES_TO_PRINT to avoid flooding console.
    int N = count;
    if (N > requestedN) N = requestedN;
    if (N > MAX_ENTRIES_TO_PRINT) N = MAX_ENTRIES_TO_PRINT;

    // Templates for the row data and blank rows (padding)
    const char* DATA_FORMAT = "|%3d | %5.5s | %8.4f | %4d | %1c | %1c | %1s |";
    const char* BLANK_FORMAT = "|%3d | %5s | %8s | %4s | %1s | %1s | %1s |";

    printf("\n%*.*s## Top %d Entries Comparison(Detailed Fixed Width) ##\n", 16, 16, "", N);
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);

    // Print Headers
    printf("|%*s%s%*s|", (ENTRY_BLOCK_WIDTH - (int)strlen("ENTROPY SORTED")) / 2, "", "ENTROPY SORTED", ((ENTRY_BLOCK_WIDTH - (int)strlen("ENTROPY SORTED")) / 2) - 2, "");
    printf(" ");
    printf("|%*s%s%*s|\n", ((ENTRY_BLOCK_WIDTH - (int)strlen("RANK SORTED")) / 2) - 1, "", "RANK SORTED", ((ENTRY_BLOCK_WIDTH - (int)strlen("RANK SORTED")) / 2), "");
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);
    printf("| %2s | %5s | %8s | %4s | %1s | %1s | %1s |", "#", "WORD", "ENTROPY", "RANK", "N", "V", "D");
    printf(" ");
    printf("| %2s | %5s | %8s | %4s | %1s | %1s | %1s |\n", "#", "WORD", "ENTROPY", "RANK", "N", "V", "D");
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);

    // Iterate and print rows
    for (int i = 0; i < N; ++i)
    {
        // Left Column: Entropy Sorted
        if (i < count) { const dictionary_entry_t* e1 = p_entropy_sorted[i]; printf(DATA_FORMAT, i + 1, e1->word, e1->entropy, e1->frequency_rank, e1->noun_type, e1->verb_type, e1->contains_duplicate_letters ? "Y" : "N"); }
        else { printf(BLANK_FORMAT, i + 1, "", "", 0, ' ', ' ', ' '); }

        printf(" "); // Gutter between tables

        // Right Column: Rank Sorted
        if (i < count) { const dictionary_entry_t* e2 = p_rank_sorted[i]; printf(DATA_FORMAT, i + 1, e2->word, e2->entropy, e2->frequency_rank, e2->noun_type, e2->verb_type, e2->contains_duplicate_letters ? "Y" : "N"); }
        else { printf(BLANK_FORMAT, i + 1, "", "", 0, ' ', ' ', ' '); }
        printf("\n");
    }
    printf("%.*s\n", TOTAL_TABLE_WIDTH, SEPARATOR_TEMPLATE);
}

/*
 * FUNCTION: clear_input_buffer
 *
 * WHAT:
 * Flushes the standard input stream (stdin).
 *
 * WHY:
 * When using `fgets` or `scanf`, if the user types more characters than the buffer
 * expects, the "extra" characters remain in the stream. These ghost characters will
 * be automatically consumed by the *next* input prompt, causing the program to skip
 * waiting for user input. This function drains the swamp to ensure a clean state.
 */
static void clear_input_buffer() { int c; while ((c = getchar()) != '\n' && c != EOF) {} }

/*
 * FUNCTION: prompt_and_validate_input
 *
 * WHAT:
 * Handles the user interaction for entering a Guess word and the subsequent
 * Result pattern (e.g., GGBYY). It performs strict validation.
 *
 * WHY:
 * Garbage In, Garbage Out. If we allow the user to enter a 4-letter word or
 * invalid result characters, the solver logic (which assumes fixed arrays of 5)
 * will corrupt memory or produce nonsense. Strict validation at the UI layer
 * protects the core logic.
 */
static bool prompt_and_validate_input(char* guess_buffer, char* result_input)
{
    char buffer[100]; int size_limit = 100;

    // LOOP 1: Get the Guess Word
    while (1)
    {
        printf("Enter your 5-letter word guess (or 'q' to quit): ");
        if (fgets(buffer, size_limit, stdin) == NULL) return false;

        // Remove trailing newline from fgets
        if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';

        // Check for Quit condition
        if (strcmp(buffer, "q") == 0) { memcpy(guess_buffer, buffer, strlen(buffer) + 1); return false; }

        // Validate Length
        if (strlen(buffer) == WORDLE_WORD_LENGTH)
        {
            // Normalize to Uppercase
            for (int i = 0; i < WORDLE_WORD_LENGTH; ++i) guess_buffer[i] = toupper((unsigned char)buffer[i]);
            guess_buffer[WORDLE_WORD_LENGTH] = '\0'; break;
        }
        else { printf("You must enter exactly 5 letters. Try again!\n"); }
    }

    // LOOP 2: Get the Result Pattern
    while (1)
    {
        printf("Enter the 5-character result (B=Black/Gray, G=Green, Y=Yellow) e.g. 'BGYBB': ");
        if (fgets(result_input, WORDLE_WORD_LENGTH + 2, stdin) == NULL) return false;

        size_t len = strlen(result_input);
        // Handle newline removal
        if (len > 0 && result_input[len - 1] == '\n') { result_input[len - 1] = '\0'; len--; }
        else { clear_input_buffer(); } // If no newline, they typed too much; flush buffer.

        // Validate Length
        if (len != WORDLE_WORD_LENGTH) { printf("The result pattern must be exactly 5 characters long. Try again!\n"); continue; }

        // Validate Characters (B, G, Y only)
        bool valid = true;
        for (int i = 0; i < WORDLE_WORD_LENGTH; i++)
        {
            result_input[i] = toupper((unsigned char)result_input[i]);
            if (result_input[i] != 'B' && result_input[i] != 'G' && result_input[i] != 'Y')
            {
                printf("Invalid character '%c'. Please use only B, G, or Y. Try again!\n", result_input[i]); valid = false; break;
            }
        }
        if (valid) return true;
    }
}

/*
 * FUNCTION: analyze_and_recommend
 *
 * WHAT:
 * A high-level wrapper that triggers the printing of the comparison table and the
 * candidate recommendation box.
 *
 * WHY:
 * Separation of concerns. This function encapsulates the "Reporting" phase of a turn.
 * It calls `get_best_guess_candidates` to identify the top words, then passes them
 * to the rendering functions.
 */
void analyze_and_recommend(const dictionary_pointer_array_t p_possibleAnswers_sorted_by_entropy,
    const dictionary_pointer_array_t p_possibleAnswers_sorted_by_rank,
    const int possibleAnswers_count,
    recommendations_array_t candidates,
    const dictionary_entry_t* pSmartPick)
{
    print_comparison_table_fixed_width(p_possibleAnswers_sorted_by_entropy, p_possibleAnswers_sorted_by_rank, possibleAnswers_count, 25);

    // Identify the top candidates for the 4 standard categories
    if (get_best_guess_candidates(p_possibleAnswers_sorted_by_entropy, p_possibleAnswers_sorted_by_rank, possibleAnswers_count, candidates))
    {
        // Render the recommendation UI
        print_final_candidates_aligned_box(candidates, pSmartPick);
    }
}

/*
 * FUNCTION: run_interactive_mode
 *
 * WHAT:
 * The core gameplay loop for the Interactive Solver. It simulates a full game session:
 * 1. Selects the bot strategy.
 * 2. Loops through 6 turns.
 * 3. Calculates Entropy for valid words.
 * 4. Recommends a guess.
 * 5. Accepts user feedback (result pattern).
 * 6. Filters the dictionary based on that feedback.
 *
 * WHY:
 * This is the "Game Controller." It manages the lifecycle of the dictionary data
 * as the game progresses. It ensures that after every turn, the dictionary is
 * shrunk (filtered) and re-evaluated (entropy calculation) so the next guess is
 * based on the new reality.
 */
void run_interactive_mode(dictionary_entry_t* p_possibleAnswers_data,
    int possibleAnswers_count,
    dictionary_pointer_array_t* pp_possibleAnswersSortedByEntropy,
    dictionary_pointer_array_t* pp_possibleAnswersSortedByRank)
{
    char user_guess[WORDLE_WORD_LENGTH + 2];
    char result_pattern[WORDLE_WORD_LENGTH + 2];
    recommendations_array_t candidates;

    // Temporary array to track pointers to *valid* answers only.
    // We use malloc because the stack might overflow if the dictionary is huge.
    dictionary_entry_t** ppValidAnswers = (dictionary_entry_t**)malloc(sizeof(dictionary_entry_t*) * possibleAnswers_count);
    int total_dictionary_size = possibleAnswers_count;
    int min_required_counts[26] = { 0 }; // Tracks the minimum count of each letter (e.g., "at least 2 'E's")

    // === CONFIGURATION ===
    // 0 = Entropy Linguist (Strict) - THE CHAMPION STRATEGY
    // This strategy uses Entropy to split the list but rejects plural nouns/past tense verbs.
    int selected_strategy_index = 0;

    HybridConfig championConfig = ALL_STRATEGIES[selected_strategy_index];
    printf("Interactive Mode Strategy: %s\n", championConfig.name);

    g_tryIdx = 0;
    // GAME LOOP: Up to 6 guesses
    for (g_tryIdx = 1; g_tryIdx <= MAX_GUESSES; g_tryIdx++)
    {
        // 1. Identify Valid Words
        // Scan the master data and collect pointers to words that haven't been eliminated.
        int validCount = 0;
        for (int i = 0; i < total_dictionary_size; ++i)
        {
            if (!p_possibleAnswers_data[i].is_eliminated) ppValidAnswers[validCount++] = &p_possibleAnswers_data[i];
        }

        // 2. Ask the Bot for the Best Move
        const dictionary_entry_t* pSmartPick = NULL;
        if (!g_isHardMode)
        {
            // Normal Mode: The bot can pick ANY word (even invalid ones) if it gives good info.
            // We pass 'total_dictionary_size' as the candidate pool.
            pSmartPick = get_smart_hybrid_guess(*pp_possibleAnswersSortedByEntropy, *pp_possibleAnswersSortedByRank, total_dictionary_size, &championConfig, min_required_counts, validCount, g_tryIdx);
        }
        else
        {
            // Hard Mode: The bot MUST pick a word that fits the current clues.
            // We pass 'validCount' as the candidate pool.
            pSmartPick = get_smart_hybrid_guess(*pp_possibleAnswersSortedByEntropy, *pp_possibleAnswersSortedByRank, validCount, &championConfig, min_required_counts, validCount, g_tryIdx);
        }

        // 3. Show Recommendations to User
        analyze_and_recommend(*pp_possibleAnswersSortedByEntropy, *pp_possibleAnswersSortedByRank, g_isHardMode ? validCount : total_dictionary_size, candidates, pSmartPick);

        printf("\n--- Turn %d of %d ---\n", g_tryIdx, MAX_GUESSES);

        // 4. Get User Input (Real world feedback)
        if (!prompt_and_validate_input(user_guess, result_pattern))
        {
            if (strcmp(user_guess, "q") == 0) { printf("USER TYPED 'q'! Exiting game loop.\n"); break; }
            g_tryIdx--; continue;
        }

        // 5. Check Win Condition
        if (strcmp(result_pattern, "GGGGG") == 0) { printf("\n*** CONGRATULATIONS! YOU SOLVED IT IN %d GUESSES! ***\n", g_tryIdx); break; }

        printf("Guess: %s, Result: %s. Processing...\n", user_guess, result_pattern);

        // 6. Update Constraints
        // "min_required_counts" tracks if we know there are at least 2 'E's, etc.
        update_min_required_counts(user_guess, result_pattern, min_required_counts);

        // 7. Filter the Dictionary
        // Mark words as "eliminated" if they don't match the result pattern.
        filter_dictionary_by_constraints(p_possibleAnswers_data, possibleAnswers_count, user_guess, result_pattern);

        // 8. Re-calculate Entropy and Sort
        if (!g_isHardMode)
        {
            // NORMAL MODE: We re-scan valid words to know probabilities, but we keep ALL words as candidates.
            validCount = 0;
            for (int i = 0; i < total_dictionary_size; ++i)
            {
                if (!p_possibleAnswers_data[i].is_eliminated) ppValidAnswers[validCount++] = &p_possibleAnswers_data[i];
            }

            printf("Remaining valid words: %d\n", validCount);
            if (validCount == 0) { printf("CRITICAL: No words remaining!\n"); break; }

            printf("Recalculating entropy...\n");
            calculate_entropy_for_candidates(p_possibleAnswers_data, total_dictionary_size, ppValidAnswers, validCount);

            // Re-create sorted views
            free(*pp_possibleAnswersSortedByEntropy); *pp_possibleAnswersSortedByEntropy = NULL;
            duplicate_dictionary_pointers(p_possibleAnswers_data, total_dictionary_size, pp_possibleAnswersSortedByEntropy, compare_dictionary_entries_by_entropy_no_filter_desc);

            free(*pp_possibleAnswersSortedByRank); *pp_possibleAnswersSortedByRank = NULL;
            duplicate_dictionary_pointers(p_possibleAnswers_data, total_dictionary_size, pp_possibleAnswersSortedByRank, compare_dictionary_entries_by_rank_desc);
        }
        else
        {
            // HARD MODE: We physically sort the array to push eliminated words to the end.
            // This makes subsequent operations faster as we only iterate the first 'new_count' elements.
            qsort(p_possibleAnswers_data, possibleAnswers_count, sizeof(dictionary_entry_t), compare_master_entries_eliminated_then_alpha);
            int new_count = possibleAnswers_count;
            for (int i = 0; i < possibleAnswers_count; ++i)
            {
                if (p_possibleAnswers_data[i].is_eliminated) { new_count = i; break; }
            }
            possibleAnswers_count = new_count;
            printf("Remaining valid words: %d\n", possibleAnswers_count);
            if (possibleAnswers_count == 0) { printf("CRITICAL: No words remaining!\n"); break; }

            printf("Recalculating entropy...\n");
            calculate_entropy_on_dictionary(p_possibleAnswers_data, possibleAnswers_count);

            free(*pp_possibleAnswersSortedByEntropy); *pp_possibleAnswersSortedByEntropy = NULL;
            free(*pp_possibleAnswersSortedByRank); *pp_possibleAnswersSortedByRank = NULL;

            duplicate_dictionary_pointers(p_possibleAnswers_data, possibleAnswers_count, pp_possibleAnswersSortedByEntropy, compare_dictionary_entries_by_entropy_desc);
            duplicate_dictionary_pointers(p_possibleAnswers_data, possibleAnswers_count, pp_possibleAnswersSortedByRank, compare_dictionary_entries_by_rank_desc);
        }
    }
    free(ppValidAnswers);
}

/*
 * FUNCTION: get_game_setup_input
 *
 * WHAT:
 * Prompts the user for configuration options at runtime:
 * 1. Dictionary Filtering (Use "Used Words" history or not?)
 * 2. Hard Mode vs Normal Mode.
 * 3. Interactive Mode vs Monte Carlo Simulation Mode.
 *
 * WHY:
 * Allows the user to switch between different testing and playing scenarios
 * without needing to recompile.
 *
 * RETURNS:
 * - true if the user wants to filter the dictionary (remove past answers).
 * - false if the user wants the full "Fresh Universe" dictionary.
 */
static bool get_game_setup_input()
{
    char buffer[2048];
    bool filter_history = true; // Default

    printf("\nDo you want to filter out past Wordle answers? (Y/N) (Default: Y): ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) { buffer[0] = 'Y'; }
    if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';

    if (strlen(buffer) > 0 && toupper((unsigned char)buffer[0]) == 'N')
    {
        filter_history = false;
        printf("History Filter DISABLED. Dictionary will include all past answers.\n");
    }
    else
    {
        printf("History Filter ENABLED. Past answers will be removed.\n");
    }

    printf("\nAre you playing Wordle in HARD MODE (Y/N)? (Default: N): ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) { buffer[0] = 'N'; }
    if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';

    if (strlen(buffer) > 0 && toupper((unsigned char)buffer[0]) == 'Y')
    {
        g_isHardMode = true; printf("Solver initialized for HARD MODE.\n");
    }
    else
    {
        g_isHardMode = false; printf("Solver initialized for NORMAL MODE.\n");
    }

    printf("\nAre you wanting to interactively play Wordle (Y/N)? (Default: Y): ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) { buffer[0] = 'Y'; }
    if (strlen(buffer) > 0 && buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';

    if (strlen(buffer) > 0 && toupper((unsigned char)buffer[0]) == 'N')
    {
        g_isInteractivePlay = false; printf("Solver initialized for NON-INTERACTIVE play mode.\n");
    }
    else
    {
        g_isInteractivePlay = true; printf("Solver initialized for INTERACTIVE play mode.\n");
    }

    return filter_history;
}

/*
 * FUNCTION: main
 *
 * WHAT:
 * The application entry point.
 * 1. Gets User Configuration (Filter history? Hard Mode? Sim Mode?).
 * 2. Loads the dictionary from disk based on that config.
 * 3. Creates initial sorted views (Entropy and Rank).
 * 4. Launches either the Interactive Game Loop or the Monte Carlo Simulation.
 * 5. Cleans up allocated memory on exit.
 *
 * WHY:
 * Acts as the bootstrap for the application. It ensures all data structures are
 * initialized and valid before the complex logic begins.
 */
int main(int argc, char* argv[])
{
    // 1. Get Dictionary Configuration First
    // We need to know if we are filtering history BEFORE we load the data.
    bool filter_history = get_game_setup_input();

    // 2. Load the Master Dictionary
    if (load_dictionary(&g_p_dictionary, &g_dictionary_word_count, filter_history))
    {
        dictionary_entry_t* p_possibleAnswers_data = NULL;
        int possibleAnswers_count = g_dictionary_word_count;
        dictionary_pointer_array_t p_possibleAnswersSortedByEntropy = NULL;
        dictionary_pointer_array_t p_possibleAnswersSortedByRank = NULL;

        // 3. Create Working Copy
        // We duplicate the dictionary data because the game logic modifies the 'is_eliminated' flags.
        p_possibleAnswers_data = (dictionary_entry_t*)malloc(sizeof(dictionary_entry_t) * possibleAnswers_count);
        if (p_possibleAnswers_data == NULL) { printf("Failed to allocate memory.\n"); free(g_p_dictionary); return -1; }
        memcpy(p_possibleAnswers_data, g_p_dictionary, sizeof(dictionary_entry_t) * possibleAnswers_count);

        // 4. Create Initial Views
        duplicate_dictionary_pointers(p_possibleAnswers_data, possibleAnswers_count, &p_possibleAnswersSortedByEntropy, compare_dictionary_entries_by_entropy_desc);
        duplicate_dictionary_pointers(p_possibleAnswers_data, possibleAnswers_count, &p_possibleAnswersSortedByRank, compare_dictionary_entries_by_rank_desc);

        // 5. Launch Mode
        if (g_isInteractivePlay)
        {
            printf("\nStarting Interactive Wordle Solver...\n");
            run_interactive_mode(p_possibleAnswers_data, possibleAnswers_count, &p_possibleAnswersSortedByEntropy, &p_possibleAnswersSortedByRank);
        }
        else
        {
            printf("\nStarting Monte Carlo Simulation...\n");
            // Note: Monte Carlo makes its own thread-local copies of the dictionary
            run_monte_carlo_simulation(g_p_dictionary, g_dictionary_word_count);
        }

        // 6. Cleanup
        if (p_possibleAnswersSortedByEntropy != NULL)  free(p_possibleAnswersSortedByEntropy);
        if (p_possibleAnswersSortedByRank != NULL)  free(p_possibleAnswersSortedByRank);
        if (p_possibleAnswers_data != NULL) free(p_possibleAnswers_data);
        if (g_p_dictionary != NULL) free(g_p_dictionary);
    }
    else
    {
        printf("Failed to load dictionary.\n");
    }
    return 0;
}