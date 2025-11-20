/*
 * FILE: duplicate_dictionary.cpp
 *
 * WHAT:
 * Implements the logic for creating "Views" of the dictionary.
 * A "View" is a sorted array of pointers that references the master data
 * without duplicating the actual content.
 *
 * WHY:
 * Performance and Flexibility.
 * 1. Memory: A `dictionary_entry_t` is large. A pointer is small (8 bytes).
 * Sorting pointers moves significantly less memory than sorting structs.
 * 2. Multiple Sorts: We need to see the dictionary sorted by Entropy AND by Rank
 * simultaneously to make hybrid decisions. Views allow us to have two
 * different sorted lists pointing to the same underlying data source.
 */

#include "duplicate_dictionary.h"
#include <memory.h>
#include <stdlib.h>

 /*
  * FUNCTION: duplicate_dictionary_pointers
  *
  * WHAT:
  * Creates a new array of pointers pointing to the entries in the source dictionary,
  * then sorts that array using the provided comparator.
  *
  * PARAMETERS:
  * - p_source_dictionary: The master array of actual data.
  * - source_dictionary_count: How many items are in the master array.
  * - pp_target_pointer_array: Output. Will hold the new array of pointers.
  * - compare_func: The sorting rule (e.g., Sort by Entropy vs Sort by Rank).
  *
  * RETURNS:
  * - true if successful, false if memory allocation fails.
  *
  * WHY:
  * This is the engine behind the "Hybrid" strategy. It allows the bot to switch
  * between "Best Math Move" (Entropy Sort) and "Most Common Word" (Rank Sort)
  * instantly by looking at different views, without needing to re-sort the
  * massive master list every time it switches context.
  */
bool duplicate_dictionary_pointers(const dictionary_entry_t* p_source_dictionary,
    int source_dictionary_count,
    dictionary_pointer_array_t* pp_target_pointer_array,
    int (*compare_func)(const void*, const void*))
{
    // 1. Validate Inputs
    if (p_source_dictionary == NULL || source_dictionary_count <= 0 || pp_target_pointer_array == NULL || compare_func == NULL)
    {
        return false;
    }

    // 2. Allocate Memory for the View
    // We allocate an array of POINTERS (dictionary_entry_t*), not structs.
    dictionary_entry_t** p_target_pointer_array =
        (dictionary_entry_t**)malloc(sizeof(dictionary_entry_t*) * source_dictionary_count);

    if (p_target_pointer_array == NULL)
    {
        return false; // Out of memory
    }

    // 3. Initialize the View
    // Point each element in the new array to the corresponding element in the master array.
    // This creates the reference link.
    for (int i = 0; i < source_dictionary_count; i++)
    {
        // Cast away const for the pointer array, though logic should respect it.
        p_target_pointer_array[i] = (dictionary_entry_t*)&p_source_dictionary[i];
    }

    // 4. Sort the View
    // Use the standard library QuickSort.
    // Note: We are sorting elements of size `sizeof(pointer)`, using the custom comparator.
    qsort(p_target_pointer_array,
        source_dictionary_count,
        sizeof(dictionary_entry_t*),
        compare_func);

    // 5. Return the Result
    *pp_target_pointer_array = p_target_pointer_array;
    return true;
}