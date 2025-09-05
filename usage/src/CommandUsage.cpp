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
                       const std::size_t indent, const std::size_t gap, const int width,
                       const std::size_t max_key_col,
                       const ColorTheme& theme,
                       const bool show_aliases) {
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

}

// ---------- CommandUsage internals ----------

bool CommandUsage::matches(const std::string& alias) const {
    return std::ranges::any_of(aliases, [&](const std::string& a){ return a == alias; });
}

std::string CommandUsage::primary() const {
    if (aliases.empty()) throw std::runtime_error("CommandUsage::primary() called with no aliases");
    return aliases[0];
}

std::string CommandUsage::joinAliasesInline_(const std::string& sep) const {
    if (aliases.empty()) throw std::runtime_error("joinAliasesInline_ called with empty aliases");
    std::ostringstream ss;
    if (aliases.size() > 1 || pluralAliasImpliesList) ss << "[";
    for (unsigned int i = 0; i < aliases.size(); ++i) {
        ss << aliases[i];
        if (i + 1 < aliases.size()) ss << sep;
    }
    if (pluralAliasImpliesList) ss << sep << primary() << "s";
    if (aliases.size() > 1 || pluralAliasImpliesList) ss << "]";
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
    if (const auto p = parent.lock(); p.get()) ss << p->constructCmdString() << " ";
    const auto cmdWithAliases = show_aliases ? joinAliasesInline_() : primary();
    ss << cmdWithAliases;
    return ss.str();
}

std::string CommandUsage::normalizePositional_(const std::string& s) {
    // If caller already used <> or [] leave it; else wrap as <name>
    if (s.find('<') != std::string::npos || s.find('[') != std::string::npos) return s;
    return fmt::format("<{}>", s);
}

std::vector<const CommandUsage*> CommandUsage::lineage_() const {
    std::vector<const CommandUsage*> chain;
    const CommandUsage* cur = this;
    while (cur) {
        chain.push_back(cur);
        auto p = cur->parent.lock();
        cur = p.get();
    }
    std::ranges::reverse(chain.begin(), chain.end());
    return chain;
}

std::string CommandUsage::tokenFor_(const CommandUsage* node) const {
    // Use node’s own alias policy
    return show_aliases ? node->joinAliasesInline_(" | ") : node->primary();
}

bool CommandUsage::sameAliases_(const CommandUsage* a, const CommandUsage* b) {
    if (!a || !b) return false;
    return a->aliases == b->aliases && a->pluralAliasImpliesList == b->pluralAliasImpliesList;
}

std::string CommandUsage::join_(const std::vector<std::string>& v, std::string_view sep) {
    std::ostringstream ss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) ss << sep;
        ss << v[i];
    }
    return ss.str();
}

void CommandUsage::appendWrapped_(std::ostringstream& out,
                               const std::string& text,
                               const size_t width,
                               const size_t indentAfterFirst) {
    if (width == 0) { out << text; return; }
    size_t col = 0;
    bool firstLine = true;
    for (size_t i = 0; i < text.size(); ) {
        if (col == 0 && !firstLine && indentAfterFirst) {
            out << std::string(indentAfterFirst, ' ');
            col += indentAfterFirst;
        }
        const size_t nextSpace = text.find(' ', i);
        std::string_view word = (nextSpace == std::string::npos)
            ? std::string_view(text).substr(i)
            : std::string_view(text).substr(i, nextSpace - i);
        if (const size_t need = word.size() + (col ? 1 : 0); col + need > width && col > 0) {
            out << "\n";
            col = 0;
            firstLine = false;
            continue;
        }
        if (col) { out << ' '; ++col; }
        out << word;
        col += word.size();
        if (nextSpace == std::string::npos) break;
        i = nextSpace + 1;
    }
}

// If Entry has aliases, treat them as a <choice|list|...>
std::string CommandUsage::renderEntryPrimary_(const Entry& e, bool squareIfOptional) {
    // e.label can be "--flag <arg>" or just "--flag" or "name"
    // If e.aliases non-empty, treat them as the value choices.
    std::string rhs;
    if (!e.aliases.empty()) rhs = "<" + join_(e.aliases, "|") + ">";

    // If the label already contains <...> or [...] keep it verbatim
    const bool preWrapped = (e.label.find('<') != std::string::npos) || (e.label.find('[') != std::string::npos);
    std::string base = e.label;
    if (!preWrapped && !rhs.empty()) base += " " + rhs;

    // Wrap only if caller didn’t already
    if (!preWrapped) {
        if (squareIfOptional) return "[" + base + "]";
        return "<" + base + ">";
    }
    return base; // already bracketed upstream
}

std::string CommandUsage::buildSynopsis_() const {
    if (synopsis) return *synopsis;

    std::ostringstream syn;
    syn << binName_;

    const auto chain = lineage_();
    constexpr size_t startIdx = 1;  // Skip root (vh) at index 0

    for (size_t i = startIdx; i < chain.size(); ++i) {
        // Collapse adjacent nodes if they render to the same token set
        if (i > startIdx && sameAliases_(chain[i], chain[i - 1])) continue;
        syn << ' ' << tokenFor_(chain[i]);
    }

    for (const auto& p : positionals) syn << ' ' << normalizePositional_(p.label);
    for (const auto& r : required) syn << ' ' << renderEntryPrimary_(r, false);
    for (const auto& o : optional) syn << ' ' << renderEntryPrimary_(o, true);

    return syn.str();
}

// ---------- CommandUsage impl ----------

static std::string lineBreak(const int tw = 100, const int padding = 3) {
    if (tw - 2 * padding < 10) return std::string(tw, '-');
    return std::string(padding, ' ') + std::string(tw - 2 * padding, '-') + std::string(padding, ' ');
}

void CommandUsage::emitCommand(std::ostringstream& out, const std::shared_ptr<CommandUsage>& command, const bool spaceLines) const {
    const auto cmd = command ? command : shared_from_this();
    const int tw = term_width > 40 ? term_width : 100;

    {
        std::ostringstream head;

        if (!show_aliases || aliases.size() < 2) head << cmd->constructCmdString();
        else head << cmd->constructCmdString();

        out << theme.C() << head.str() << theme.R() << "\n";
        if (spaceLines) out << "\n";
        out << theme.A2() << "Description:" << theme.R();
        const auto needsSpace = cmd->description.contains("\n");
        if (needsSpace) out << "\n";
        if (cmd->description.empty()) out << "  No description provided.\n";
        else emitWrapped(out, cmd->description, needsSpace ? 2 : 1, tw);
    }

    if (spaceLines) out << "\n";
    out << theme.H() << "Usage:" << theme.R();
    {
        const auto syn = cmd->buildSynopsis_();
        const bool needsSpace = syn.contains("\n");
        if (needsSpace) out << "\n";
        emitWrapped(out, syn, needsSpace ? 2 : 1, tw);
    }
    out << "\n";
}


std::string CommandUsage::str() const {
    const int tw = term_width > 40 ? term_width : 100;
    constexpr std::size_t indent = 2;
    constexpr std::size_t gap = 2;

    std::ostringstream out;

    const auto emit = [&](const std::shared_ptr<CommandUsage>& command = nullptr) {
        const auto cmd = command ? command : shared_from_this();

        emitCommand(out, command, true);

        emitTwoColSection(out, "Required:", cmd->required, indent, gap, tw, max_key_col, theme, show_aliases);
        emitTwoColSection(out, "Optional:", cmd->optional, indent, gap, tw, max_key_col, theme, show_aliases);
        for (const auto& g : cmd->groups)
            emitTwoColSection(out, g.title + ":", g.items, indent, gap, tw, max_key_col, theme, show_aliases);

        if (!examples.empty()) {
            out << theme.A3() << "Examples:" << theme.R() << "\n";
            for (const auto& ex : cmd->examples) {
                emitWrapped(out, fmt::format("$ {}", ex.cmd), indent, tw);
                if (!ex.note.empty()) emitWrapped(out, ex.note, indent + 2, tw);
                out << "\n";
            }
        }
    };

    emit();
    for (unsigned int i = 0; i < subcommands.size(); ++i) {
        emit(subcommands[i]);
        if (i + 1 < subcommands.size()) out << "\n" << lineBreak() << "\n\n";
    }

    return out.str();
}

std::string CommandUsage::basicStr() const {
    if (aliases.empty()) throw std::runtime_error("CommandUsage::basicStr() called with no command");

    std::ostringstream out;
    out << "\n";

    emitCommand(out);
    for (const auto& c : subcommands) emitCommand(out, c);

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

    if (!description.empty()) md << description << "\n\n";

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
