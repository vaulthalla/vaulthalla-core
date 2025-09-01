#include "CommandUsage.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <ranges>

#include <fmt/format.h>

namespace vh::shell {

// ---------- internal helpers (anonymous namespace) ----------

namespace {

std::string trimRight(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::vector<std::string> wrap(const std::string& s, int width) {
    const int W = std::max(20, width);
    std::vector<std::string> out;
    std::size_t i = 0, n = s.size();
    while (i < n) {
        // skip leading spaces
        while (i < n && std::isspace(static_cast<unsigned char>(s[i])) && s[i] != '\n') ++i;

        // hard break at newline
        if (i < n && s[i] == '\n') {
            out.emplace_back("");
            ++i;
            continue;
        }

        if (i >= n) break;

        const std::size_t end = std::min<std::size_t>(i + W, n);
        std::size_t break_pos = end;

        // prefer last space before end
        if (end < n && s[end] != ' ') {
            auto sp = s.rfind(' ', end);
            if (sp != std::string::npos && sp >= i) break_pos = sp;
        }

        if (break_pos == i) break_pos = end; // no space found

        std::string line = trimRight(s.substr(i, break_pos - i));
        out.push_back(std::move(line));

        // move i
        if (break_pos < n && s[break_pos] == ' ') i = break_pos + 1;
        else i = break_pos;
    }
    if (out.empty()) out.emplace_back("");
    return out;
}

void emitWrapped(std::ostringstream& out, const std::string& text,
                 std::size_t indent, int width) {
    const auto lines = wrap(text, width - static_cast<int>(indent));
    for (const auto& ln : lines) {
        out << std::string(indent, ' ') << ln << "\n";
    }
}

std::size_t computeKeyWidth(const std::vector<Entry>& items, std::size_t cap, bool show_aliases) {
    auto visibleLen = [](const std::string& s) -> std::size_t { return s.size(); };
    std::size_t w = 0;
    for (const auto& it : items) {
        const std::string full = show_aliases
            ? fmt::format("{}{}", it.label,
                          it.aliases.empty() ? "" : fmt::format(" | {}", fmt::join(it.aliases, " | ")))
            : it.label;
        w = std::max<std::size_t>(w, visibleLen(full));
    }
    return std::min(w, cap);
}

std::string padRight(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

void emitTwoCol(std::ostringstream& out,
                const std::vector<Entry>& items,
                std::size_t indent, std::size_t gap,
                int width, std::size_t keyw,
                const ColorTheme& theme,
                bool show_aliases) {
    if (items.empty()) return;
    const int rightw = width - static_cast<int>(indent + keyw + gap);
    for (const auto& it : items) {
        const std::string keyPlain = show_aliases
            ? fmt::format("{}{}", it.label,
                          it.aliases.empty() ? "" : fmt::format(" | {}", fmt::join(it.aliases, " | ")))
            : it.label;
        const auto desc_lines = wrap(it.desc, std::max(20, rightw));

        // first line
        out << std::string(indent, ' ');
        // Colorize the key, but align using plain-length padding.
        out << theme.K() << padRight(keyPlain, keyw) << theme.R();
        out << std::string(gap, ' ');
        if (!desc_lines.empty()) {
            out << desc_lines[0] << "\n";
            // continuation lines
            for (std::size_t i = 1; i < desc_lines.size(); ++i) {
                out << std::string(indent + keyw + gap, ' ') << desc_lines[i] << "\n";
            }
        } else {
            out << "\n";
        }
    }
}

void emitTwoColSection(std::ostringstream& out,
                       const std::string& title,
                       const std::vector<Entry>& items,
                       std::size_t indent, std::size_t gap, int width,
                       std::size_t max_key_col,
                       const ColorTheme& theme,
                       bool show_aliases) {
    if (items.empty()) return;
    out << theme.H() << title << theme.R() << "\n";
    const auto keyw = computeKeyWidth(items, max_key_col, show_aliases);
    emitTwoCol(out, items, indent, gap, width, keyw, theme, show_aliases);
    out << "\n";
}

std::string escapePipes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '|') out += "\\|";
        else out += c;
    }
    return out;
}

void emitMarkdownTable(std::ostringstream& md, const std::vector<Entry>& items, bool show_aliases) {
    if (items.empty()) return;
    md << "| Option | Description |\n";
    md << "|:------ |:----------- |\n";
    for (const auto& it : items) {
        const std::string keyCell = show_aliases
            ? fmt::format("{}{}", fmt::format("`{}`", escapePipes(it.label)),
                          it.aliases.empty() ? "" :
                              fmt::format(" ({})",
                                          fmt::format("{}", fmt::join(
                                              [&]() {
                                                  std::vector<std::string> quoted;
                                                  quoted.reserve(it.aliases.size());
                                                  for (auto& a : it.aliases) quoted.push_back("`" + escapePipes(a) + "`");
                                                  return quoted;
                                              }(), ", "))))
            : fmt::format("`{}`", escapePipes(it.label));
        md << "| " << keyCell << " | " << escapePipes(it.desc) << " |\n";
    }
}

std::string shortOr(const std::string& s, const std::string& fallback) {
    for (const char c : s) if (!std::isspace(static_cast<unsigned char>(c))) return s;
    return fallback;
}

}

// ---------- CommandUsage internals ----------

bool CommandUsage::matches(const std::string& alias) const {
    return std::ranges::any_of(aliases, [&](const std::string& a){ return a == alias; });
}

std::string CommandUsage::primary() const {
    if (aliases.empty()) throw std::runtime_error("CommandUsage::primary() called with no aliases");
    return aliases[0];
}

std::string CommandUsage::joinSubcommandsInline(const std::string& sep) const {
    if (subcommands.empty()) throw std::runtime_error("joinSubcommandsInline called with no subcommands");
    std::ostringstream ss;
    for (const auto& sc : subcommands) {
        if (ss.tellp() > 0) ss << sep;
        ss << sc->primary();
        if (pluralAliasImpliesList) ss << sep << sc->primary() << "s";
    }
    return ss.str();
}

std::string CommandUsage::joinAliasesInline_(const std::string& sep) const {
    if (aliases.empty()) throw std::runtime_error("joinAliasesInline_ called with empty aliases");
    std::ostringstream ss;
    for (const auto& a : aliases) ss << sep << a;
    if (pluralAliasImpliesList) ss << sep << primary() << "s";
    return ss.str();
}

std::string CommandUsage::joinAliasesCode_() {
    if (aliases.empty()) return fmt::format("`{}`", primary());
    std::ostringstream ss;
    ss << "`" << primary() << "` (";
    for (std::size_t i = 0; i < aliases.size(); ++i) {
        if (i) ss << ", ";
        ss << "`" << aliases[i] << "`";
    }
    if (pluralAliasImpliesList) ss << ", `" << primary() << "s`";
    ss << ")";
    return ss.str();
}

std::string CommandUsage::constructCmdString() const {
    std::ostringstream ss;
    if (parent) ss << parent->constructCmdString() << " ";
    ss << primary();
    return ss.str();
}

std::string CommandUsage::normalizePositional_(const std::string& s) {
    // If caller already used <> or [] leave it; else wrap as <name>
    if (s.find('<') != std::string::npos || s.find('[') != std::string::npos) return s;
    return fmt::format("<{}>", s);
}

std::string CommandUsage::bracketizeIfNeeded_(const std::string& s, bool square) {
    // If caller already bracketed, don't double-wrap
    if (s.find('<') != std::string::npos || s.find('[') != std::string::npos) return s;
    return square ? fmt::format("[{}]", s) : fmt::format("<{}>", s);
}

std::string CommandUsage::buildSynopsis_() const {
    if (synopsis) return *synopsis;

    std::ostringstream syn;

    // ns + command (with aliases inline if enabled)
    const auto nsWithAliases = show_aliases ? joinAliasesInline_() : primary();

    const bool hasCmd = !aliases.empty();
    const auto cmdWithAliases = hasCmd ? (show_aliases ? joinAliasesInline_() : primary())
        : std::string{};

    // Prefix with binary name for clarity in synopsis
    syn << "vh ";
    syn << nsWithAliases;
    if (hasCmd) syn << " " << cmdWithAliases;

    // positionals (labels only)
    for (const auto& p : positionals) syn << " " << normalizePositional_(p.label);
    // required flags (primary only keeps synopsis readable)
    for (const auto& r : required)    syn << " " << bracketizeIfNeeded_(r.label, /*square=*/false);
    // optional flags (primary only)
    for (const auto& o : optional)    syn << " " << bracketizeIfNeeded_(o.label, /*square=*/true);
    return syn.str();
}

// ---------- CommandUsage impl ----------

std::string CommandUsage::str() const {
    const int tw = term_width > 40 ? term_width : 100;
    constexpr std::size_t indent = 2;
    constexpr std::size_t gap = 2;

    std::ostringstream out;

    emitTwoColSection(out, "Required:", required, indent, gap, tw, max_key_col, theme, show_aliases);
    emitTwoColSection(out, "Optional:", optional, indent, gap, tw, max_key_col, theme, show_aliases);
    for (const auto& g : groups)
        emitTwoColSection(out, g.title + ":", g.items, indent, gap, tw, max_key_col, theme, show_aliases);

    if (!examples.empty()) {
        out << theme.H() << "Examples:" << theme.R() << "\n";
        for (const auto& ex : examples) {
            emitWrapped(out, fmt::format("$ {}", ex.cmd), indent, tw);
            if (!ex.note.empty()) emitWrapped(out, ex.note, indent + 2, tw);
            out << "\n";
        }
    }

    return basicStr(true) + out.str();
}

std::string CommandUsage::basicStr(const bool splitHeader) const {
    if (aliases.empty()) throw std::runtime_error("CommandUsage::basicStr() called with no command");

    const int tw = term_width > 40 ? term_width : 100;

    std::ostringstream out;

    {
        std::ostringstream head;

        if (!show_aliases) head << constructCmdString();
        else head << "[" << joinAliasesInline_() << "]";

        if (!subcommands.empty()) {
            head << " ";
            if (subcommands.size() == 1) head << subcommands[0]->primary();
            else head << "(" << joinSubcommandsInline() << ")";
        }

        out << theme.C() << head.str() << theme.R();
        if (!description.empty()) out << " - " << shortOr(description, "");
        out << "\n";
    }

    if (splitHeader) out << "\n";
    out << theme.H() << "Usage:" << theme.R();
    {
        const auto syn = buildSynopsis_();
        const bool needsSpace = syn.contains("\n");
        if (needsSpace) out << "\n";
        emitWrapped(out, syn, needsSpace ? 2 : 1, tw);
        out << "\n";
    }

    return out.str();
}

std::string CommandUsage::markdown() const {
    std::ostringstream md;

    if (aliases.empty()) throw std::runtime_error("CommandUsage::markdown() called with no command");

    // Title
    {
        std::ostringstream head;
        head << constructCmdString();
        md << fmt::format("# `{}`\n\n", head.str());
    }

    if (!description.empty()) {
        md << description << "\n\n";
    }

    if (show_aliases) {
        if (!aliases.empty()) {
            md << "**Command aliases:** ";
            for (std::size_t i = 0; i < aliases.size(); ++i) {
                if (i) md << ", ";
                md << "`" << aliases[i] << "`";
            }
            md << "\n\n";
        }
    }

    // Synopsis
    md << "## Usage\n\n";
    md << "```bash\n" << buildSynopsis_() << "\n```\n\n";

    // Sections
    if (!required.empty()) {
        md << "## Required Options\n\n";
        emitMarkdownTable(md, required, show_aliases);
        md << "\n";
    }
    if (!optional.empty()) {
        md << "## Optional Options\n\n";
        emitMarkdownTable(md, optional, show_aliases);
        md << "\n";
    }
    for (const auto& g : groups) {
        if (!g.items.empty()) {
            md << "## " << g.title << "\n\n";
            emitMarkdownTable(md, g.items, show_aliases);
            md << "\n";
        }
    }

    if (!examples.empty()) {
        md << "## Examples\n\n";
        for (const auto& ex : examples) {
            md << "```bash\n" << ex.cmd << "\n```\n";
            if (!ex.note.empty()) md << ex.note << "\n\n";
        }
    }

    return md.str();
}

}
