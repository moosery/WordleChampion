/*
 * FILE: monte_carlo.h
 *
 * WHAT:
 * Defines the interface for the Simulation Engine.
 * This module contains the logic to run mass simulations ("Tournaments")
 * where specific bot strategies play against every single word in the
 * dictionary to determine their Win Rate and Average Guess Count.
 *
 * WHY:
 * "Gut feeling" is not enough in Wordle optimization. To prove that
 * "Strategy A" is better than "Strategy B", we must run them both against
 * the full dataset (approx 5,000 - 12,000 words). This module orchestrates
 * that heavy workload, often utilizing multi-threading for speed.
 */

#pragma once
#ifndef MONTE_CARLO_H
#define MONTE_CARLO_H
#include "wordle_types.h"

 /*
  * FUNCTION: run_monte_carlo_simulation
  *
  * WHAT:
  * The entry point for the Simulation Mode.
  * 1. Selects a roster of strategies to test (defined internally or via args).
  * 2. Iterates through the entire Master Dictionary.
  * 3. For each word in the dictionary, it treats it as the "Secret Answer".
  * 4. The Bot plays a full game (up to 6 guesses) attempting to find that answer.
  * 5. Aggregates stats (Wins, Losses, Guess Distribution).
  * 6. Prints a final "Tournament Report" to the console.
  *
  * PARAMETERS:
  * - p_master_dictionary: The full list of words loaded from disk.
  * - master_count: The number of words in the list.
  *
  * WHY:
  * This function is the "Scientist" of the application. It generates the empirical
  * data required to tune heuristics (like Rank Tolerance or Look Ahead Depth).
  */
void run_monte_carlo_simulation(const dictionary_entry_t* p_master_dictionary, int master_count);

#endif