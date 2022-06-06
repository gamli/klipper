#include "logging.h"


#include <algorithm>
#include <cstdarg>
#include <list>
#include <memory>
#include <string>
#include <cstdio>
#include <functional>
#include <iostream>
#include <numeric>
#include <regex>
#include <sstream>
#include <stack>
#include <utility>

#include "compiler.h"

using namespace std;

#define LOG_OSTREAM cerr

thread_local string TRY_CATCH_INDENT;
#define TRY try { TRY_CATCH_INDENT.push_back(' ');
#define CATCH(...) \
    TRY_CATCH_INDENT.pop_back(); \
    } catch (exception& e) \
    { \
        cout << "===>>> " << e.what() << endl __VA_ARGS__; \
        cerr << "===>>> " << e.what() << endl __VA_ARGS__; \
        throw; \
    } \
    catch (...) \
    { \
        cout << "===>>> " << "unknown error" << endl __VA_ARGS__; \
        cerr << "===>>> " << "unknown error" << endl __VA_ARGS__; \
        throw; \
    } \

// utility functions ###################################################################################################

void forEachLine(const string_view& str, const function<void(const string_view& sv, bool isLastLine)>& handleLine)
{
    stringstream stream((str.data()));
    string line;
    while (getline(stream, line, '\n'))
    {
        handleLine(line, stream.eof());
    }
}

size_t countCols(const string& str)
{
    size_t maxCols = 0;
    forEachLine(str, [maxCols](auto line, auto) mutable
    {
        maxCols = max(maxCols, line.length());
    });
    return maxCols;
}

size_t countOccurrences(const string& pattern, const string& str)
{
    const auto m = pattern.length();
    const auto n = str.length();
    size_t res = 0;

    if (m == 0 || n == 0)
    {
        return res;
    }

    /* A loop to slide pat[] one by one */
    for (size_t i = 0; i <= n - m; i++)
    {
        /* For current index i, check for
           pattern match */
        size_t j;
        for (j = 0; j < m; j++)
            if (str[i + j] != pattern[j])
                break;

        // if pat[0...M-1] = txt[i, i+1, ...i+M-1]
        if (j == m)
        {
            res++;
        }
    }
    return res;
}

size_t countRows(const string& str)
{
    return countOccurrences("\n", str) + 1;
}

template <class TIter, class TFnElement, class TFnSeparator>
void join(TIter begin, TIter end, TFnElement handleElement, TFnSeparator handleSeparator)
{
    TIter current = begin;
    TIter last;
    if (current != end)
    {
        handleElement(*current);
        last = current;
        ++current;
    }
    while (current != end)
    {
        handleSeparator(*last, *current);
        handleElement(*current);
        last = current;
        ++current;
    }
}

string join(const list<string>& parts, const string& sep = "")
{
    stringstream joined;
    join(
        parts.begin(),
        parts.end(),
        [&joined](const auto& part) { joined << part; },
        [&joined, sep](auto const&, auto const&) { joined << sep; });
    return joined.str();
}

// text classes ########################################################################################################

struct Dimensions
{
    Dimensions(const size_t rows, const size_t columns): Rows(rows), Cols(columns)
    {
    }

    size_t Rows, Cols;
};

class Text
{
public:
    virtual ~Text() = default;

    virtual void format(const function<void(const string_view& str)>& accumulator) = 0;

    virtual Dimensions dimensions() = 0;

    virtual void lines(const function<void(const string_view& str)>& handleLine) = 0;

    virtual unique_ptr<Text> clone() = 0;
};

class OneLineText final : public Text
{
    string _str;

public:
    explicit OneLineText(string str) : _str(std::move(str))
    {
        if (countOccurrences("\n", _str))
        {
            throw runtime_error("OneLineText must not contain new lines");
        }
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        accumulator(_str);
        CATCH(<< "OneLineText::format(): " << _str)
    }

    Dimensions dimensions() override
    {
        TRY
        return {1, _str.length()};
        CATCH(<< "OneLineText::dimensions(): " << _str)
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        handleLine(_str);
        CATCH(<< "OneLineText::lines(): " << _str)
    }

    unique_ptr<Text> clone() override
    {
        TRY
        return make_unique<OneLineText>(_str);
        CATCH(<< "OneLineText::clone(): " << _str)
    }
};

class MultiText final : public Text
{
    list<unique_ptr<Text>> _texts;

public:
    explicit MultiText()
    {
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        join(
            _texts.begin(),
            _texts.end(),
            [accumulator](const auto& text) { text->format(accumulator); },
            [accumulator](const auto&, const auto&) { accumulator("\n"); });
        CATCH(<< "MultiText::format(): " << _texts.size() << " texts")
    }

    Dimensions dimensions() override
    {
        TRY
        size_t textsCols = 0, textsRows = 0;
        join(_texts.begin(), _texts.end(),
             [&textsCols, &textsRows](const auto& text)
             {
                 auto dimensions = text->dimensions();
                 textsCols = max(textsCols, dimensions.Cols);
                 textsRows += dimensions.Rows;
             },
             [&textsRows](const auto& last, const auto& next)
             {
                 //textsRows++;
             });


        return {textsRows, textsCols};
        CATCH(<< "MultiText::dimensions(): " << _texts.size() << " texts")
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        for_each(_texts.begin(), _texts.end(), [handleLine](const auto& text)
        {
            text->lines(handleLine);
        });
        CATCH(<< "MultiText::lines(): " << _texts.size() << " texts")
    }

    unique_ptr<Text> clone() override
    {
        TRY
        auto clone = make_unique<MultiText>();
        for (const auto& text : _texts)
        {
            clone->add(text->clone());
        }
        return clone;
        CATCH(<< "MultiText::clone(): " << _texts.size() << " texts")
    }

    void add(unique_ptr<Text> text)
    {
        TRY
        _texts.push_back(std::move(text));
        CATCH(<< "MultiText::add(): " << _texts.size() << " texts")
    }

    void add(const MultiText* multiText)
    {
        TRY
        for (const auto& text : multiText->_texts)
        {
            _texts.push_back(text->clone());
        }
        CATCH(<< "MultiText::add(): " << _texts.size() << " texts")
    }
};

class IndentedText final : public Text
{
    unique_ptr<Text> _text;
    string _indent;

public:
    explicit IndentedText(unique_ptr<Text> text): _text(std::move(text)), _indent("   ")
    {
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        stringstream formattedText;
        _text->format([&formattedText](auto str)
        {
            formattedText << str;
        });
        forEachLine(formattedText.str(), [accumulator, this](auto line, auto isLastLine)
        {
            accumulator(_indent);
            accumulator(line);
            if (!isLastLine)
            {
                accumulator("\n");
            }
        });
        CATCH(<< "IndentedText::format()")
    }

    Dimensions dimensions() override
    {
        TRY
        auto textDimensions = _text->dimensions();
        return {textDimensions.Rows, textDimensions.Cols + _indent.length()};
        CATCH(<< "IndentedText::dimensions()")
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        _text->lines([handleLine, this](auto line) { handleLine(_indent + line.data()); });
        CATCH(<< "IndentedText::lines()")
    }

    unique_ptr<Text> clone() override
    {
        TRY
        return make_unique<IndentedText>(_text->clone());
        CATCH(<< "IndentedText::clone()")
    }
};

class ColumnText final : public Text
{
    list<unique_ptr<Text>> _columnTexts;
    string _separator;

public:
    ColumnText(string separator): _separator(std::move(separator))
    {
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        const auto dim = dimensions();

        list<stringstream> lines(dim.Rows);


        join(
            _columnTexts.begin(),
            _columnTexts.end(),
            [&lines](const auto& current) mutable
            {
                auto textCols = current->dimensions().Cols;

                auto lineIter = lines.begin();
                current->lines([&textCols, &lineIter](const auto& line) mutable
                {
                    *lineIter++ << line << string(max(static_cast<size_t>(0), textCols - line.length()), ' ');
                });
                while (lineIter != lines.end())
                {
                    *lineIter << string(textCols, ' ');
                    ++lineIter;
                }
            },
            [&lines, this](const auto&, const auto&) mutable
            {
                for (auto& line : lines)
                {
                    line << _separator;
                }
            });

        join(
            lines.begin(),
            lines.end(),
            [accumulator](const auto& current)
            {
                accumulator(current.str());
            },
            [accumulator](const auto&, const auto&)
            {
                accumulator("\n");
            });
        CATCH(<< "ColumnText::format(): " << _columnTexts.size() << " column texts with separator " << _separator)
    }

    Dimensions dimensions() override
    {
        TRY
        size_t maxRows = 0, totalCols = 0;

        join(
            _columnTexts.begin(),
            _columnTexts.end(),
            [&maxRows, &totalCols](const auto& current) mutable
            {
                const auto dim = current->dimensions();

                maxRows = max(maxRows, dim.Rows);
                totalCols += dim.Cols;
            },
            [&totalCols,this](const auto&, const auto&) mutable
            {
                totalCols += _separator.length();
            });


        return {maxRows, totalCols};
        CATCH(<< "ColumnText::dimensions(): " << _columnTexts.size() << " column texts with separator " << _separator)
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        stringstream stream;
        format([handleLine, &stream](auto str) mutable
        {
            for (size_t i = 0; i < str.length(); i++)
            {
                auto c = str[i];
                if (c == '\n')
                {
                    handleLine(stream.str());
                    stream.str("");
                    stream.clear();
                }
                else
                {
                    stream << c;
                }
            }
        });

        if (const auto lastLine = stream.str(); lastLine.length())
        {
            handleLine(lastLine);
        }
        CATCH(<< "ColumnText::lines(): " << _columnTexts.size() << " column texts with separator " << _separator)
    }

    unique_ptr<Text> clone() override
    {
        TRY
        auto clone = make_unique<ColumnText>(_separator);
        for (const auto& columnText : _columnTexts)
        {
            clone->add(columnText->clone());
        }
        return clone;
        CATCH(<< "ColumnText::clone(): " << _columnTexts.size() << " column texts with separator " << _separator)
    }

    void add(unique_ptr<Text> text)
    {
        TRY
        _columnTexts.push_back(std::move(text));
        CATCH(<< "ColumnText::add(): " << _columnTexts.size() << " column texts with separator " << _separator)
    }
};

class XmlText final : public Text
{
    string _tag, _openingTag, _closingTag, _attributeSeparator;
    list<pair<string, string>> _attrs;
    unique_ptr<MultiText> _inner;

public:
    XmlText(string tag): _tag(std::move(tag)), _inner(new MultiText())
    {
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        this->update();

        accumulator(_openingTag);
        _inner->format(accumulator);
        accumulator(_closingTag);
        CATCH(<< "XmlText::format():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    Dimensions dimensions() override
    {
        TRY
        this->update();

        const auto tagRows = countRows(_openingTag) + countRows(_closingTag);
        const auto tagCols = max(countCols(_openingTag), countCols(_closingTag));
        const auto innerDimensions = _inner->dimensions();
        return {
            tagRows + innerDimensions.Rows,
            max(tagCols, innerDimensions.Cols + 3),
        };
        CATCH(<< "XmlText::dimensions():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        this->update();

        forEachLine(_openingTag, [handleLine](const auto& line, auto) { handleLine(line); });
        _inner->lines(handleLine);
        forEachLine(_closingTag, [handleLine](const auto& line, auto) { handleLine(line); });
        CATCH(<< "XmlText::lines():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    unique_ptr<Text> clone() override
    {
        TRY
        auto clone = make_unique<XmlText>(_tag);
        for (const auto& attr : _attrs)
        {
            clone->attr(attr);
        }
        clone->inner(_inner->clone());
        return clone;
        CATCH(<< "XmlText::clone():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    void attr(const pair<string, string>& attr)
    {
        TRY
        this->attr(attr.first, attr.second);
        CATCH(<< "XmlText::attr():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    void attr(string name, string value)
    {
        TRY
        _attrs.emplace_back(std::move(name), std::move(value));
        CATCH(<< "XmlText::attr():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    void inner(unique_ptr<Text> inner)
    {
        TRY
        _inner->add(make_unique<IndentedText>(std::move(inner)));
        CATCH(<< "XmlText::inner():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

private:
    void update()
    {
        TRY
        const auto attributeCols =
            accumulate(
                _attrs.begin(),
                _attrs.end(),
                0,
                [](auto cols, auto nv)
                {
                    return cols + nv.first.length() + nv.second.length();
                });
        _attributeSeparator = attributeCols > 50 ? "\n   " : " ";

        stringstream openingTagStream;
        openingTagStream << "<" << _tag;
        for (const auto& [fst, snd] : _attrs)
        {
            openingTagStream << _attributeSeparator << fst << "=\"" << snd << "\"";
        }
        openingTagStream << ">\n";
        _openingTag = openingTagStream.str();

        _closingTag = "\n</" + _tag + ">";
        CATCH(<< "XmlText::update():" << endl
            << "tag: " << _tag << endl
            << "opening tag: " << _openingTag << endl
            << "closing tag: " << _closingTag << endl
            << "_attributeSeparator: |||" << _attributeSeparator << "|||" << endl
            << debugAttrs() << endl)
    }

    string debugAttrs()
    {
        stringstream ss;
        for (const auto& [fst, snd] : _attrs)
        {
            ss << "(" << fst << ", " << snd << ")";
        }
        return ss.str();
    }
};

class ValuesText final : public Text
{
    MultiText *_names, *_values;
    unique_ptr<ColumnText> _columns;

public:
    ValuesText(): _names(new MultiText), _values(new MultiText), _columns(new ColumnText(" = "))
    {
        _columns->add(unique_ptr<MultiText>(_names));
        _columns->add(unique_ptr<MultiText>(_values));
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        TRY
        _columns->format(accumulator);
        CATCH(<< "ValuesText::format()")
    }

    Dimensions dimensions() override
    {
        TRY
        return _columns->dimensions();
        CATCH(<< "ValuesText::dimensions()")
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        TRY
        _columns->lines(handleLine);
        CATCH(<< "ValuesText::lines()")
    }

    unique_ptr<Text> clone() override
    {
        TRY
        auto clone = make_unique<ValuesText>();
        clone->_names->add(_names);
        clone->_values->add(_values);
        return clone;
        CATCH(<< "ValuesText::clone()")
    }

    void add(const char* name, struct text value) const
    {
        TRY
        const auto valueTextRef = static_cast<Text*>(value.Ref);
        const auto valueRows = valueTextRef->dimensions().Rows;
        
        this->_names->add(make_unique<OneLineText>(name));
        for (size_t i = 1; i < valueRows; ++i)
        {
            this->_names->add(make_unique<OneLineText>(string(strlen(name) - 1, ' ') + "⮱"));
        }

        this->_values->add(unique_ptr<Text>(valueTextRef));
        CATCH(<< "ValuesText::add()")
    }

    void add(const char* name, const char* value) const
    {
        TRY
        this->_names->add(make_unique<OneLineText>(name));
        this->_values->add(make_unique<OneLineText>(value));
        CATCH(<< "ValuesText::add()")
    }
};

// C interface #########################################################################################################


thread_local static vector<text> log_c_stack;

void log_c_push_container(const text container)
{
    log_c_stack.push_back(container);
}

void log_c_pop_container()
{
    log_c_stack.pop_back();
}

EXTERNC void log_c_root(text t)
{
    if (!log_c_stack.empty())
    {
        throw runtime_error("log_c_root(): context stack not empty");
    }
    log_c_push_container(t);
}

template <class TText>
void log_c_add_child(const function<void(const text& topContext)>& adder)
{
    try
    {
        if (log_c_stack.empty())
        {
            log_c_root(log_xml("default-root"));
        }

        const auto& topContext = log_c_stack.back();

        if (dynamic_cast<TText*>(static_cast<Text*>(topContext.Ref)) == nullptr)
        {
            throw runtime_error(
                "log_c_add_child(): top context has the wrong type: got '" +
                string(typeid(topContext.Ref).name()) +
                "' expected '" + typeid(TText).name() + "'");
        }

        adder(topContext);
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
        cerr << e.what() << endl;
        throw;
    }
    catch (...)
    {
        cout << "unknown error" << endl;
        cerr << "unknown error" << endl;
        throw;
    }
}

void log_c_add_child_generic(const text& child)
{
    try
    {
        log_c_add_child<Text>([child](const auto& topContext)
        {
            const auto topContextRef = static_cast<Text*>(topContext.Ref);
            if (dynamic_cast<MultiText*>(topContextRef) != nullptr)
            {
                log_multi_add(topContext, child);
            }
            else if (dynamic_cast<ColumnText*>(topContextRef) != nullptr)
            {
                log_columns_add(topContext, child);
            }
            else if (dynamic_cast<XmlText*>(topContextRef) != nullptr)
            {
                log_xml_inner(topContext, child);
            }
            else
            {
                throw runtime_error("log_c_add_child(): top context is not a container type");
            }
        });
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
        cerr << e.what() << endl;
        throw;
    }
    catch (...)
    {
        cout << "unknown error" << endl;
        cerr << "unknown error" << endl;
        throw;
    }
}

EXTERNC void log_print(struct text text)
{
    const auto t = static_cast<Text*>(text.Ref);
    t->format([](const auto& str)
    {
        LOG_OSTREAM << str;
    });
    cerr << endl;
    delete t;
}

EXTERNC void __visible log_c_print()
{
    try
    {
        if (log_c_stack.empty())
        {
            return;
        }

        const auto root = log_c_stack[0];

        const auto t = static_cast<Text*>(root.Ref);
        t->format([](const auto& str)
        {
            LOG_OSTREAM << str;
        });
        LOG_OSTREAM << endl;
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
        cerr << e.what() << endl;
        throw;
    }
    catch (...)
    {
        cout << "unknown error" << endl;
        cerr << "unknown error" << endl;
        throw;
    }
}

EXTERNC void __visible log_c_discard()
{
    while(!log_c_stack.empty())
    {
        auto [Ref] = log_c_stack.back();
        delete static_cast<Text*>(Ref);
        log_c_stack.pop_back();
    }
}

EXTERNC void log_c_end()
{
    log_c_pop_container();
}

EXTERNC void log_c_t(text t)
{
    log_c_add_child_generic(t);
}

// ONE

EXTERNC text log_one(const char* fmt, ...)
{
    try
    {
        va_list args1, args2;
        va_start(args1, fmt);
        va_copy(args2, args1);
        const auto estimatedSize = vsnprintf(nullptr, 0, fmt, args1); // NOLINT(clang-diagnostic-format-nonliteral)
        const auto buffer = new char[estimatedSize + 1];
        const auto actualSize = vsnprintf(buffer, estimatedSize + 1, fmt, args2); // NOLINT(clang-diagnostic-format-nonliteral)
        if (actualSize < 0 || actualSize >= estimatedSize + 1)
        {
            throw runtime_error(
                "log_one() error actual: " + to_string(actualSize) + ", estimated: " + to_string(estimatedSize));
        }
        buffer[actualSize] = '\0';
        const auto oneLineText = new OneLineText(buffer);
        delete[] buffer;
        va_end(args1);
        va_end(args2);
        return {oneLineText};
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
        cerr << e.what() << endl;
        throw;
    }
    catch (...)
    {
        cout << "unknown error" << endl;
        cerr << "unknown error" << endl;
        throw;
    }
}

EXTERNC void log_c_one(const char* fmt, ...)
{
    try
    {
        va_list args1, args2;
        va_start(args1, fmt);
        va_copy(args2, args1);
        const auto estimatedSize = vsnprintf(nullptr, 0, fmt, args1); // NOLINT(clang-diagnostic-format-nonliteral)
        const auto buffer = new char[estimatedSize + 1];
        const auto actualSize = vsnprintf(buffer, estimatedSize + 1, fmt, args2); // NOLINT(clang-diagnostic-format-nonliteral)
        if (actualSize < 0 || actualSize >= estimatedSize + 1)
        {
            throw runtime_error(
                "log_one() error actual: " + to_string(actualSize) + ", estimated: " + to_string(estimatedSize));
        }
        buffer[actualSize] = '\0';
        const auto oneLineText = new OneLineText(buffer);
        delete[] buffer;
        va_end(args1);
        va_end(args2);
        log_c_add_child_generic({oneLineText});
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
        cerr << e.what() << endl;
        throw;
    }
    catch (...)
    {
        cout << "unknown error" << endl;
        cerr << "unknown error" << endl;
        throw;
    }
}

// MULTI

EXTERNC text log_multi()
{
    return {new MultiText()};
}

EXTERNC void log_multi_add(struct text multi_text, struct text text_to_add)
{
    static_cast<MultiText*>(multi_text.Ref)->add(unique_ptr<Text>(static_cast<Text*>(text_to_add.Ref)));
}


EXTERNC void log_c_multi()
{
    const auto multi = log_multi();
    log_c_add_child_generic(multi);
    log_c_push_container(multi);
}

// INDENT

EXTERNC text log_indent(struct text text)
{
    return {new IndentedText(unique_ptr<Text>(static_cast<Text*>(text.Ref)))};
}

// COLUMNS

EXTERNC text log_columns(const char* separator)
{
    return {new ColumnText(separator)};
}

EXTERNC void log_columns_add(struct text column_text, struct text column_to_add)
{
    static_cast<ColumnText*>(column_text.Ref)->add(unique_ptr<Text>(static_cast<Text*>(column_to_add.Ref)));
}


EXTERNC void log_c_columns(const char* separator)
{
    const auto columns = log_columns(separator);
    log_c_add_child_generic(columns);
    log_c_push_container(columns);
}

// XML

EXTERNC text log_xml(const char* tag)
{
    return {new XmlText(tag)};
}

EXTERNC void log_xml_attr(struct text xml_text, const char* name, const char* value)
{
    static_cast<XmlText*>(xml_text.Ref)->attr(name, value);
}

EXTERNC void log_xml_inner(struct text xml_text, struct text inner_text)
{
    static_cast<XmlText*>(xml_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(inner_text.Ref)));
}


EXTERNC void log_c_xml(const char* tag)
{
    const auto xml = log_xml(tag);
    log_c_add_child_generic(xml);
    log_c_push_container(xml);
}

EXTERNC void log_c_xml_attr(const char* name, const char* value)
{
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_xml_attr(topContext, name, value); });
}

// SECTION

EXTERNC text log_section(const char* name)
{
    const auto sectionText = log_xml("section");
    log_xml_attr(sectionText, "name", name);
    return sectionText;
}

EXTERNC void log_section_content(struct text section_text, struct text content)
{
    static_cast<XmlText*>(section_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(content.Ref)));
}


EXTERNC void log_c_section(const char* name)
{
    const auto section = log_section(name);
    log_c_add_child_generic(section);
    log_c_push_container(section);
}

// FUNCTION

EXTERNC struct text log_function(const char* name)
{
    const auto functionText = log_xml("function");
    log_xml_attr(functionText, "name", name);
    return functionText;
}

EXTERNC void log_function_params(struct text function_text, struct text param_text)
{
    static_cast<XmlText*>(function_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(param_text.Ref)));
}

EXTERNC void log_function_body(struct text function_text, struct text body_text)
{
    static_cast<XmlText*>(function_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(body_text.Ref)));
}

EXTERNC void log_function_return(struct text function_text, struct text return_text)
{
    static_cast<XmlText*>(function_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(return_text.Ref)));
}


EXTERNC void log_c_function(const char* name)
{
    const auto function = log_function(name);
    log_c_add_child_generic(function);
    log_c_push_container(function);
}

EXTERNC void log_c_function_params()
{
    const auto params = log_xml("params");
    log_c_add_child_generic(params);
    log_c_push_container(params);
}

EXTERNC void log_c_function_body()
{
    const auto body = log_xml("body");
    log_c_add_child_generic(body);
    log_c_push_container(body);
}

EXTERNC void log_c_function_return()
{
    const auto ret = log_xml("return");
    log_c_add_child_generic(ret);
    log_c_push_container(ret);
}

// LOOP

EXTERNC struct text log_loop(const char* name)
{
    const auto loopText = log_xml("loop");
    log_xml_attr(loopText, "name", name);
    return loopText;
}

EXTERNC struct text log_loop_iter(int n)
{
    const auto iterText = log_xml("iteration");
    log_xml_attr(iterText, "n", to_string(n).c_str());
    return iterText;
}


EXTERNC void log_c_loop(const char* name)
{
    const auto loop = log_loop(name);
    log_c_add_child_generic(loop);
    log_c_push_container(loop);
}

EXTERNC void log_c_loop_iter(int n)
{
    const auto iter = log_loop_iter(n);
    log_c_add_child_generic(iter);
    log_c_push_container(iter);
}

// VALUES

EXTERNC text log_values_tag(const char* tag)
{
    //return {new ValuesText()};
    return log_xml(tag);
}

EXTERNC struct text log_values()
{
    return log_values_tag("values");
}

EXTERNC void log_values_add(struct text values_text, const char* name, const char* value)
{
    //static_cast<ValuesText*>(values_text.Ref)->add(name, value);
    const auto val = log_xml("val");
    log_xml_attr(val, "n", name);
    log_xml_attr(val, "v", value);
    log_xml_inner(values_text, val);
}

EXTERNC void log_values_add_t(struct text values_text, const char* name, struct text value)
{
    //static_cast<ValuesText*>(values_text.Ref)->add(name, value);
    const auto val = log_xml("val");
    log_xml_attr(val, "n", name);
    log_xml_inner(val, value);
    log_xml_inner(values_text, val);
}

EXTERNC void log_values_add_i(struct text values_text, const char* name, int value)
{
    log_values_add(values_text, name, to_string(value).c_str());
}

EXTERNC void log_values_add_d(struct text values_text, const char* name, double value)
{
    log_values_add(values_text, name, to_string(value).c_str());
}

EXTERNC void log_values_add_b(struct text values_text, const char* name, int value)
{
    log_values_add(values_text, name, value ? "true" : "false");
}


EXTERNC void log_c_values_tag(const char* tag)
{
    const auto values = log_values_tag(tag);
    log_c_add_child_generic(values);
    log_c_push_container(values);
}

EXTERNC void log_c_values()
{
    log_c_values_tag("values");
}

EXTERNC void log_c_values_add(const char* name, const char* value)
{
    //log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add(topContext, name, value); });
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_values_add(topContext, name, value); });
}

EXTERNC void log_c_values_add_t(const char* name, struct text value)
{
    //log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_t(topContext, name, value); });
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_values_add_t(topContext, name, value); });
}

EXTERNC void log_c_values_add_i(const char* name, int value)
{
    // log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_i(topContext, name, value); });
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_values_add_i(topContext, name, value); });
}

EXTERNC void log_c_values_add_d(const char* name, double value)
{
    // log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_d(topContext, name, value); });
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_values_add_d(topContext, name, value); });
}

EXTERNC void log_c_values_add_b(const char* name, int value)
{
    // log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_b(topContext, name, value); });
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_values_add_b(topContext, name, value); });
}

// OTHER

EXTERNC struct text log_value_move(::move* move)
{
    struct text multi = log_multi();
    struct text values = log_values();
    log_multi_add(multi, values);
    log_values_add_d(values, "print_time", move->print_time);
    log_values_add_d(values, "move_t", move->move_t);
    log_values_add_d(values, "start_v", move->start_v);
    log_values_add_d(values, "half_accel", move->half_accel);
    log_values_add_t(values, "start_pos", log_value_coord(move->start_pos));
    log_values_add_t(values, "axes_r", log_value_coord(move->axes_r));
    log_values_add_b(values, "is_backlash_compensation_move", move->is_backlash_compensation_move);
    return multi;
}

//
// void test_fn()
// {
// LOG_C_CONTEXT
//     
//     LOG_C_SECTION("TEST_SECTION")
//
//     if(true)
//     {
// LOG_C_CONTEXT
//         LOG_C_SECTION("INNER SECTION")
//         log_c_one("Innere Zeile 1");
//     }
//     log_c_one("Zeile eins");
//     log_c_one("Zeile zwei");
// }
//
// int main()
// {
//     log_c_root(log_xml("Wurzel"));  
//
//     test_fn();    
//
//     log_c_print();    
//
//     return 0;
// }


//
// void scoped(int * pvariable) {
//     printf("variable (%d) goes out of scope\n", *pvariable);
// }
//
// int main(void) {
//     printf("before scope\n");
//     {
//         int watched __attribute__((__cleanup__ (scoped)));
//         watched = 42;
//     }
//     printf("after scope\n");
// }
