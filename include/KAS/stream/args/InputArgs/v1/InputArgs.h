/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_STREAM_INPUT_ARGS_V1_H
#define KASLIBCXX_STREAM_INPUT_ARGS_V1_H

#include <cstddef>
#include <iostream>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kas::stream::args::v1 {

/**
 * @brief Parsed command-line arguments.
 *
 * InputArgs splits argv into two views:
 *  - positional arguments, accessible by index via getIndex();
 *  - key-value arguments of the form "--key=value" or "-k=v",
 *    accessible by key via getKey()/hasKey().
 *
 * All returned std::string_view values point into the original argv
 * storage. The caller must guarantee that argv (and the strings it
 * points to) outlive the InputArgs object. InputArgs does not copy
 * or own the argument text.
 *
 * @par Lifetime
 * The constructor stores non-owning string_views into argv. If argv
 * is mutated or the underlying buffers are freed, all previously
 * returned views become dangling. Typical usage (argv from main) is
 * safe because argv lives for the whole program.
 *
 * @par Thread safety
 * InputArgs is not thread-safe. Read-only accessors may be called
 * from multiple threads only if no other thread mutates the object;
 * the object has no mutating public API after construction.
 *
 * @par Example
 * @code
 * kas::stream::args::v1::InputArgs args(argc, argv);
 * if (auto v = args.getKey("--output")) {
 *     use(*v);
 * }
 * @endcode
 */
class InputArgs final {
public:
    /**
     * @brief Parses the command line.
     *
     * @param count Number of arguments (argc).
     * @param argv  Array of arguments; must be non-null and contain at
     *              least count entries. The strings pointed to by
     *              argv must outlive *this.
     *
     * @note Parsing rules (what counts as a positional argument and
     *       what counts as a key-value pair) are defined by the
     *       implementation and are not fixed by this declaration.
     */
    explicit InputArgs(int count, char* argv[]);

    /**
     * @brief Returns the positional argument at the given index.
     *
     * @param index Zero-based index into the positional arguments.
     * @return The argument as a string_view, or std::nullopt if index
     *         is out of range. The view points into argv storage.
     */
    [[nodiscard]] std::optional<std::string_view> getIndex(std::size_t index) const;

    /**
     * @brief Returns the value associated with a key.
     *
     * @param key Key name, e.g. "--output" or "-o".
     * @return The value as a string_view, or std::nullopt if the key
     *         is absent. The view points into argv storage.
     */
    [[nodiscard]] std::optional<std::string_view> getKey(std::string_view key) const;

    /**
     * @brief Returns true if the given key is present.
     *
     * @param key Key name, e.g. "--output".
     */
    [[nodiscard]] bool hasKey(std::string_view key) const;

    /**
     * @brief Returns the number of positional arguments.
     */
    [[nodiscard]] std::size_t countIndex() const noexcept;

    /**
     * @brief Returns the number of key-value pairs.
     */
    [[nodiscard]] std::size_t countKeys() const noexcept;

    /**
     * @brief Prints the parsed arguments for debugging.
     *
     * @param out Output stream to write to. Must be valid.
     */
    void print(std::ostream& out) const;

private:
    std::vector<std::string_view> m_args; ///< Positional arguments (non-owning views into argv).
    std::unordered_map<std::string_view, std::string_view> m_keys; ///< Key-value pairs (non-owning views into argv).
};

} // namespace kas::stream::args::v1

#endif