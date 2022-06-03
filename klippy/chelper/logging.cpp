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
#include <sstream>
#include <stack>
#include <utility>

using namespace std;

class NullBuffer final : public streambuf
{
public:
    int overflow(const int c) override { return c; }
};

class NullStream final : public ostream
{
public:
    NullStream() : ostream(&_sb)
    {
    }

private:
    NullBuffer _sb;
};

thread_local static size_t DOUT_INDENT = 0;

class DOutIndent
{
public:
    DOutIndent()
    {
        DOUT_INDENT += 3;
    }

    ~DOutIndent()
    {
        DOUT_INDENT -= 3;
    }
};

#define dout_indent const auto indent = DOutIndent();
//#define dout cout << string(DOUT_INDENT, ' ') <<
#define dout NullStream() <<

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
    explicit OneLineText(string str) : _str(move(str))
    {
        dout_indent
        dout "OneLineText::ctor(): \"" << _str << "\"" << endl;
        if (countOccurrences("\n", _str))
        {
            throw runtime_error("OneLineText must not contain new lines");
        }
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "OneLineText::format()" << endl;
        accumulator(_str);
    }

    Dimensions dimensions() override
    {
        dout_indent
        return {1, _str.length()};
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent
        handleLine(_str);
    }

    unique_ptr<Text> clone() override
    {
        dout_indent
        return make_unique<OneLineText>(_str);
    }
};

class MultiText final : public Text
{
    list<unique_ptr<Text>> _texts;

public:
    explicit MultiText()
    {
        dout_indent
        dout "MultiText::ctor() " << _texts.size() << " texts" << endl;
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "MultiText::format()" << endl;

        join(
            _texts.begin(),
            _texts.end(),
            [accumulator](const auto& text) { text->format(accumulator); },
            [accumulator](const auto&, const auto&) { accumulator("\n"); });
    }

    Dimensions dimensions() override
    {
        dout_indent
        dout "MultiText::dimensions()" << endl;

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

        dout "MultiText::dimensions():result " << textsRows << ", " << textsCols << endl;

        return {textsRows, textsCols};
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent
        dout "MultiText::lines()" << endl;

        for_each(_texts.begin(), _texts.end(), [handleLine](const auto& text)
        {
            text->lines(handleLine);
        });
    }

    unique_ptr<Text> clone() override
    {
        dout_indent
        dout "MultiText::clone()" << endl;

        auto clone = make_unique<MultiText>();
        for (const auto& text : _texts)
        {
            clone->add(text->clone());
        }
        return clone;
    }

    void add(unique_ptr<Text> text)
    {
        dout_indent
        dout "MultiText::add(unique_ptr<Text>)" << endl;

        _texts.push_back(move(text));
    }

    void add(const MultiText* multiText)
    {
        dout_indent
        dout "MultiText::add(MultiText*)" << endl;

        for (const auto& text : multiText->_texts)
        {
            _texts.push_back(text->clone());
        }
    }
};

class IndentedText final : public Text
{
    unique_ptr<Text> _text;
    string _indent;

public:
    explicit IndentedText(unique_ptr<Text> text): _text(move(text)), _indent("   ")
    {
        dout_indent
        dout "IndentedText::ctor()" << endl;
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "IndentedText::format()" << endl;

        stringstream formattedText;
        _text->format([&formattedText](auto str)
        {
            formattedText << str;
        });
        forEachLine(formattedText.str(), [accumulator, this](auto line, auto isLastLine)
        {
            dout "IndentedText::format()::handleLine: indenting " << line << endl;

            accumulator(_indent);
            accumulator(line);
            if (!isLastLine)
            {
                accumulator("\n");
            }
        });
    }

    Dimensions dimensions() override
    {
        dout_indent

        auto textDimensions = _text->dimensions();
        return {textDimensions.Rows, textDimensions.Cols + _indent.length()};
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent

        _text->lines([handleLine, this](auto line) { handleLine(_indent + line.data()); });
    }

    unique_ptr<Text> clone() override
    {
        dout_indent

        return make_unique<IndentedText>(_text->clone());
    }
};

class ColumnText final : public Text
{
    list<unique_ptr<Text>> _columnTexts;
    string _separator;

public:
    ColumnText(string separator): _separator(move(separator))
    {
        dout_indent
        dout "ColumnText::ctor(): "
            << _columnTexts.size()
            << " texts separated by "
            << _separator
            << "\n";
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "ColumnText::format()" << endl;

        const auto dim = dimensions();

        list<stringstream> lines(dim.Rows);

        dout "ColumnText::format()::lines = " << dim.Rows << endl;

        join(
            _columnTexts.begin(),
            _columnTexts.end(),
            [&lines](const auto& current) mutable
            {
                dout_indent
                dout "ColumnText::format()::handleElement" << endl;

                auto textCols = current->dimensions().Cols;
                dout "ColumnText::format()::handleElement: textCols=" << textCols << endl;
                auto lineIter = lines.begin();
                current->lines([&textCols, &lineIter](const auto& line) mutable
                {
                    dout_indent
                    dout "ColumnText::format()::handleElement::line: " << line << endl;
                    *lineIter++ << line << string(max(static_cast<size_t>(0), textCols - line.length()), ' ');
                });
                while (lineIter != lines.end())
                {
                    dout_indent
                    *lineIter << string(textCols, ' ');
                    ++lineIter;
                }
            },
            [&lines, this](const auto&, const auto&) mutable
            {
                dout_indent
                dout "ColumnText::format()::handleSeparator" << endl;

                for (auto& line : lines)
                {
                    dout_indent
                    line << _separator;
                }
            });

        join(
            lines.begin(),
            lines.end(),
            [accumulator](const auto& current)
            {
                dout_indent
                accumulator(current.str());
            },
            [accumulator](const auto&, const auto&)
            {
                dout_indent
                accumulator("\n");
            });
    }

    Dimensions dimensions() override
    {
        dout_indent
        dout "ColumnText::dimensions()" << endl;

        size_t maxRows = 0, totalCols = 0;

        join(
            _columnTexts.begin(),
            _columnTexts.end(),
            [&maxRows, &totalCols](const auto& current) mutable
            {
                dout_indent
                const auto dim = current->dimensions();
                dout "ColumnText::dimensions()::handleElement: dim = " << dim.Rows << ", " << dim.Cols << endl;
                maxRows = max(maxRows, dim.Rows);
                totalCols += dim.Cols;
            },
            [&totalCols,this](const auto&, const auto&) mutable
            {
                dout_indent
                dout "ColumnText::dimensions()::handleSeparator" << endl;
                totalCols += _separator.length();
            });

        dout "ColumnText::dimensions()::return " << maxRows << ", " << totalCols << endl;
        return {maxRows, totalCols};
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent
        dout "ColumnText::lines()" << endl;

        stringstream stream;
        format([handleLine, &stream](auto str) mutable
        {
            dout_indent
            for (size_t i = 0; i < str.length(); i++)
            {
                auto c = str[i];
                if (c == '\n')
                {
                    dout "ColumnText::lines()::found line: " << stream.str() << endl;
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
            dout "ColumnText::lines()::last line: " << stream.str() << endl;
            handleLine(lastLine);
        }
    }

    unique_ptr<Text> clone() override
    {
        dout_indent
        dout "ColumnText::clone()" << endl;

        auto clone = make_unique<ColumnText>(_separator);
        for (const auto& columnText : _columnTexts)
        {
            clone->add(columnText->clone());
        }
        return clone;
    }

    void add(unique_ptr<Text> text)
    {
        dout_indent
        dout "ColumnText::add()" << endl;

        _columnTexts.push_back(move(text));
    }
};

class XmlText final : public Text
{
    string _tag, _openingTag, _closingTag, _attributeSeparator;
    list<pair<string, string>> _attrs;
    unique_ptr<MultiText> _inner;

public:
    XmlText(string tag): _tag(move(tag)), _inner(new MultiText())
    {
        dout_indent
        dout "XmlText::ctor(): tag " << _tag << endl;
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "XmlText::format()" << endl;

        this->update();

        accumulator(_openingTag);
        _inner->format(accumulator);
        accumulator(_closingTag);
    }

    Dimensions dimensions() override
    {
        dout_indent
        dout "XmlText::dimensions()" << endl;

        this->update();

        const auto tagRows = countRows(_openingTag) + countRows(_closingTag);
        const auto tagCols = max(countCols(_openingTag), countCols(_closingTag));
        const auto innerDimensions = _inner->dimensions();
        return {
            tagRows + innerDimensions.Rows,
            max(tagCols, innerDimensions.Cols + 3),
        };
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent

        this->update();

        forEachLine(_openingTag, [handleLine](const auto& line, auto) { handleLine(line); });
        _inner->lines(handleLine);
        forEachLine(_closingTag, [handleLine](const auto& line, auto) { handleLine(line); });
    }

    unique_ptr<Text> clone() override
    {
        dout_indent

        auto clone = make_unique<XmlText>(_tag);
        for (const auto& attr : _attrs)
        {
            clone->attr(attr);
        }
        clone->inner(_inner->clone());
        return clone;
    }

    void attr(const pair<string, string>& attr)
    {
        dout_indent

        this->attr(attr.first, attr.second);
    }

    void attr(string name, string value)
    {
        dout_indent

        _attrs.emplace_back(move(name), move(value));
    }

    void inner(unique_ptr<Text> inner) const
    {
        dout_indent

        _inner->add(make_unique<IndentedText>(move(inner)));
    }

private:
    void update()
    {
        dout_indent

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
    }
};

class ValuesText final : public Text
{
    MultiText *_names, *_values;
    unique_ptr<ColumnText> _columns;

public:
    ValuesText(): _names(new MultiText), _values(new MultiText), _columns(new ColumnText(" = "))
    {
        dout_indent
        dout "ValuesText::ctor()" << endl;

        _columns->add(unique_ptr<MultiText>(_names));
        _columns->add(unique_ptr<MultiText>(_values));
    }

    void format(const function<void(const string_view& str)>& accumulator) override
    {
        dout_indent
        dout "ValuesText::format()" << endl;

        _columns->format(accumulator);
    }

    Dimensions dimensions() override
    {
        dout_indent

        return _columns->dimensions();
    }

    void lines(const function<void(const string_view& str)>& handleLine) override
    {
        dout_indent

        _columns->lines(handleLine);
    }

    unique_ptr<Text> clone() override
    {
        dout_indent

        auto clone = make_unique<ValuesText>();
        clone->_names->add(_names);
        clone->_values->add(_values);
        return clone;
    }

    void add(const char* name, struct text value) const
    {
        dout_indent

        dout "ValuesText::add(\"" << name << "\", ?)" << endl;

        const auto valueTextRef = static_cast<Text*>(value.Ref);
        const auto valueRows = valueTextRef->dimensions().Rows;
        dout "ValuesText::add()::value rows = " << valueRows << endl;
        dout "ValuesText::add()::add name = \"" << name << "\"" << endl;
        this->_names->add(make_unique<OneLineText>(name));
        for (size_t i = 1; i < valueRows; ++i)
        {
            dout "ValuesText::add()::add empty row " << i << endl;
            this->_names->add(make_unique<OneLineText>(""));
        }
        dout "ValuesText::add()::add value" << endl;
        this->_values->add(unique_ptr<Text>(valueTextRef));
    }

    void add(const char* name, const char* value) const
    {
        dout_indent
        dout "ValuesText::add(\"" << name << "\", \"" << value << "\")" << endl;

        this->_names->add(make_unique<OneLineText>(name));
        this->_values->add(make_unique<OneLineText>(value));
    }
};

// C interface #########################################################################################################

EXTERNC void log_print(struct text text)
{
    dout_indent

    const auto t = static_cast<Text*>(text.Ref);
    t->format([](const auto& str)
    {
        cerr << str;
    });
    cerr << endl;
    delete t;
}

thread_local static stack<text> log_c_stack;

EXTERNC void log_c_print()
{
    dout_indent

    if (log_c_stack.empty())
    {
        return;
    }

    if (log_c_stack.size() > 1)
    {
        const auto numElements = log_c_stack.size();
        
        cerr << "too many elements on stack - printing from top to bottom" << endl << string(50, '#') << endl;

        while (!log_c_stack.empty())
        {
            const auto [Ref] = log_c_stack.top();
            cerr << string(50, '#') << endl;
            cerr << "# element " << log_c_stack.size() << endl;
            cerr << string(50, '#') << endl;
            const text clone = {static_cast<Text*>(Ref)->clone().release()};
            log_print(clone);
            cerr << string(50, '#') << endl;

            log_c_stack.pop();
            if (log_c_stack.empty())
            {
                delete static_cast<Text*>(Ref);
            }
        }

        throw runtime_error(
            string("there must be exactly one element or no element on the context stack, but there are ") + to_string(
                numElements));
    }

    const auto root = log_c_stack.top();
    log_c_stack.pop();

    log_print(root);
}

template <class TText>
void log_c_add_child(const function<void(const text& topContext)>& adder)
{
    dout_indent

    if (log_c_stack.empty())
    {
        log_c_root(log_xml("default-root"));
    }

    const auto& topContext = log_c_stack.top();

    if (dynamic_cast<TText*>(static_cast<Text*>(topContext.Ref)) == nullptr)
    {
        throw runtime_error("log_c_add_child(): top context has the wrong type");
    }

    adder(topContext);
}

void log_c_add_child_generic(const text& child)
{
    dout_indent
    dout "log_c_add_child_generic()" << endl;

    log_c_add_child<Text>([child](const auto& topContext)
    {
        dout_indent
        dout "log_c_add_child_generic(): adder" << endl;

        const auto topContextRef = static_cast<Text*>(topContext.Ref);
        if (dynamic_cast<MultiText*>(topContextRef) != nullptr)
        {
            dout_indent
            dout "log_c_add_child_generic(): log_multi_add()" << endl;

            log_multi_add(topContext, child);
        }
        else if (dynamic_cast<ColumnText*>(topContextRef) != nullptr)
        {
            dout_indent
            dout "log_c_add_child_generic(): log_columns_add()" << endl;

            log_columns_add(topContext, child);
        }
        else if (dynamic_cast<XmlText*>(topContextRef) != nullptr)
        {
            dout_indent
            dout "log_c_add_child_generic(): log_xml_inner()" << endl;

            log_xml_inner(topContext, child);
        }
        else
        {
            throw runtime_error("log_c_add_child(): top context is not a container type");
        }
    });
}

void log_c_push_container(const text container)
{
    dout_indent
    log_c_stack.push(container);
}

void log_c_pop_container()
{
    dout_indent
    log_c_stack.pop();
}

EXTERNC void log_c_root(text t)
{
    dout_indent
    if (!log_c_stack.empty())
    {
        throw runtime_error("log_c_root(): context stack not empty");
    }
    log_c_push_container(t);
}

EXTERNC text log_one(const char* fmt, ...)
{
    dout_indent
    va_list args;
    va_start(args, fmt);
    const auto size = vsnprintf(nullptr, 0, fmt, args); // NOLINT(clang-diagnostic-format-nonliteral)
    const auto buffer = new char[size + 1];
    vsnprintf(buffer, size + 1, fmt, args); // NOLINT(clang-diagnostic-format-nonliteral)
    const auto oneLineText = new OneLineText(buffer);
    delete[] buffer;
    va_end(args);
    return {oneLineText};
}

EXTERNC void log_c_one(const char* fmt, ...)
{
    dout_indent
    dout "log_c_one()" << endl;

    va_list args;
    va_start(args, fmt);
    const auto size = vsnprintf(nullptr, 0, fmt, args); // NOLINT(clang-diagnostic-format-nonliteral)
    const auto buffer = new char[size + 1];
    vsnprintf(buffer, size + 1, fmt, args); // NOLINT(clang-diagnostic-format-nonliteral)
    const auto oneLineText = new OneLineText(buffer);
    delete[] buffer;
    va_end(args);
    log_c_add_child_generic({oneLineText});
}

EXTERNC text log_multi()
{
    dout_indent
    return {(new MultiText())};
}

EXTERNC void log_c_multi_begin()
{
    dout_indent
    const auto multi = log_multi();
    log_c_add_child_generic(multi);
    log_c_push_container(multi);
}

EXTERNC void log_c_multi_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_multi_add(struct text multi_text, struct text text_to_add)
{
    dout_indent
    dout "log_multi_add()" << endl;
    static_cast<MultiText*>(multi_text.Ref)->add(unique_ptr<Text>(static_cast<Text*>(text_to_add.Ref)));
}

EXTERNC void log_c_multi_add(struct text text_to_add)
{
    dout_indent
    log_c_add_child<MultiText>([&text_to_add](auto& topContext) { log_multi_add(topContext, text_to_add); });
}

struct text log_multi(const list<text>& texts)
{
    dout_indent
    const auto multiText = new MultiText();
    for (const auto [Ref] : texts)
    {
        multiText->add(unique_ptr<Text>(static_cast<Text*>(Ref)));
    }
    return {multiText};
}

EXTERNC struct text log_multi_2(struct text txt1, struct text txt2)
{
    dout_indent
    return log_multi({txt1, txt2});
}

EXTERNC struct text log_multi_3(struct text txt1, struct text txt2, struct text txt3)
{
    dout_indent
    return log_multi({txt1, txt2, txt3});
}

EXTERNC struct text log_multi_4(struct text txt1, struct text txt2, struct text txt3, struct text txt4)
{
    dout_indent
    return log_multi({txt1, txt2, txt3, txt4});
}

EXTERNC struct text log_multi_5(struct text txt1, struct text txt2, struct text txt3, struct text txt4,
                                struct text txt5)
{
    dout_indent
    return log_multi({txt1, txt2, txt3, txt4, txt5});
}

EXTERNC text log_indent(struct text text)
{
    dout_indent
    return {new IndentedText(unique_ptr<Text>(static_cast<Text*>(text.Ref)))};
}

EXTERNC text log_columns(const char* separator)
{
    dout_indent
    return {new ColumnText(separator)};
}

EXTERNC void log_c_columns_begin(const char* separator)
{
    dout_indent
    const auto columns = log_columns(separator);
    log_c_add_child_generic(columns);
    log_c_push_container(columns);
}

EXTERNC void log_c_columns_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_columns_add(struct text column_text, struct text column_to_add)
{
    dout_indent
    static_cast<ColumnText*>(column_text.Ref)->add(unique_ptr<Text>(static_cast<Text*>(column_to_add.Ref)));
}

EXTERNC text log_xml(const char* tag)
{
    dout_indent
    return {new XmlText(tag)};
}

EXTERNC void log_c_xml_begin(const char* tag)
{
    dout_indent
    const auto xml = log_xml(tag);
    log_c_add_child_generic(xml);
    log_c_push_container(xml);
}

EXTERNC void log_c_xml_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_xml_attr(struct text xml_text, const char* name, const char* value)
{
    dout_indent
    static_cast<XmlText*>(xml_text.Ref)->attr(name, value);
}

void log_c_xml_attr(const char* name, const char* value)
{
    dout_indent
    log_c_add_child<XmlText>([name, value](auto& topContext) { log_xml_attr(topContext, name, value); });
}

EXTERNC void log_xml_inner(struct text xml_text, struct text inner_text)
{
    dout_indent
    static_cast<XmlText*>(xml_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(inner_text.Ref)));
}

void log_c_xml_inner(const text inner)
{
    dout_indent
    log_c_add_child<XmlText>([&inner](auto& topContext) { log_xml_inner(topContext, inner); });
}

EXTERNC text log_section(const char* name)
{
    dout_indent
    const auto sectionText = log_xml("section");
    log_xml_attr(sectionText, "name", name);
    return sectionText;
}

EXTERNC void log_c_section_begin(const char* name)
{
    dout_indent
    const auto section = log_section(name);
    log_c_add_child_generic(section);
    log_c_push_container(section);
}

EXTERNC void log_c_section_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_section_content(struct text section_text, struct text content)
{
    dout_indent
    static_cast<XmlText*>(section_text.Ref)->inner(unique_ptr<Text>(static_cast<Text*>(content.Ref)));
}

EXTERNC void log_c_section_content(struct text content)
{
    dout_indent
    log_c_add_child<XmlText>([&content](auto& topContext) { log_xml_inner(topContext, content); });
}

EXTERNC struct text log_function(const char* name)
{
    dout_indent
    const auto functionText = log_xml("function");
    log_xml_attr(functionText, "name", name);
    return functionText;
}

EXTERNC void log_c_function_begin(const char* name)
{
    dout_indent
    const auto function = log_function(name);
    log_c_add_child_generic(function);
    log_c_push_container(function);
}

EXTERNC void log_c_function_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_c_function_params_begin()
{
    dout_indent
    const auto params = log_xml("params");
    log_c_add_child_generic(params);
    log_c_push_container(params);
}

EXTERNC void log_c_function_params_end()
{
    log_c_pop_container();
}

EXTERNC void log_c_function_body_begin()
{
    dout_indent
    const auto body = log_xml("body");
    log_c_add_child_generic(body);
    log_c_push_container(body);
}

EXTERNC void log_c_function_body_end()
{
    log_c_pop_container();
}

EXTERNC void log_c_function_return_begin()
{
    dout_indent
    const auto ret = log_xml("return");
    log_c_add_child_generic(ret);
    log_c_push_container(ret);
}

EXTERNC void log_c_function_return_end()
{
    log_c_pop_container();
}

EXTERNC struct text log_loop(const char* name)
{
    dout_indent
    const auto loopText = log_xml("loop");
    log_xml_attr(loopText, "name", name);
    return loopText;
}

EXTERNC void log_c_loop_begin(const char* name)
{
    dout_indent
    const auto loop = log_loop(name);
    log_c_add_child_generic(loop);
    log_c_push_container(loop);
}

EXTERNC void log_c_loop_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC struct text log_loop_iter(int n)
{
    dout_indent
    const auto iterText = log_xml("iteration");
    log_xml_attr(iterText, "n", to_string(n).c_str());
    return iterText;
}

EXTERNC void log_c_loop_iter_begin(int n)
{
    dout_indent
    const auto iter = log_loop_iter(n);
    log_c_add_child_generic(iter);
    log_c_push_container(iter);
}

EXTERNC void log_c_loop_iter_end()
{
    dout_indent
    log_c_pop_container();
}

text log_values()
{
    dout_indent
    return {new ValuesText()};
}

EXTERNC void log_c_values_begin()
{
    dout_indent
    const auto values = log_values();
    log_c_add_child_generic(values);
    log_c_push_container(values);
}

EXTERNC void log_c_values_end()
{
    dout_indent
    log_c_pop_container();
}

EXTERNC void log_values_add(struct text values_text, const char* name, const char* value)
{
    dout_indent
    static_cast<ValuesText*>(values_text.Ref)->add(name, value);
}

void log_c_values_add(const char* name, const char* value)
{
    dout_indent
    log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add(topContext, name, value); });
}

EXTERNC void log_values_add_t(struct text values_text, const char* name, struct text value)
{
    dout_indent
    dout "log_values_add_t()" << endl;

    static_cast<ValuesText*>(values_text.Ref)->add(name, value);
}

void log_c_values_add_t(const char* name, struct text value)
{
    dout_indent
    dout "log_c_values_add_t()" << endl;

    log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_t(topContext, name, value); });
}

EXTERNC void log_values_add_i(struct text values_text, const char* name, int value)
{
    dout_indent
    log_values_add(values_text, name, to_string(value).c_str());
}

EXTERNC void log_c_values_add_i(const char* name, int value)
{
    dout_indent
    log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_i(topContext, name, value); });
}

EXTERNC void log_values_add_d(struct text values_text, const char* name, double value)
{
    dout_indent
    log_values_add(values_text, name, to_string(value).c_str());
}

EXTERNC void log_c_values_add_d(const char* name, double value)
{
    dout_indent
    log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_d(topContext, name, value); });
}

EXTERNC void log_values_add_b(struct text values_text, const char* name, int value)
{
    dout_indent
    log_values_add(values_text, name, value ? "true" : "false");
}

EXTERNC void log_c_values_add_b(const char* name, int value)
{
    dout_indent
    log_c_add_child<ValuesText>([name, value](auto& topContext) { log_values_add_b(topContext, name, value); });
}


// int main()
// {
//     log_c_root(log_xml("Wurzel"));
//
//     // log_c_columns_begin(" <#> ");
//     //
//     // log_c_columns_begin("|");
//     // log_c_multi_begin();
//     // log_c_one("Sub Spalte 1 Zeile 1");
//     // log_c_one("Sub Spalte 1 Zeile 2");
//     // log_c_multi_end();
//     // log_c_columns_end();
//     //
//     // log_c_columns_end();
//
//
//     // log_c_multi_begin();
//     //
//     // log_c_one("Zeile 1");
//     // log_c_one("Zeile 2");
//     //
//     // log_c_multi_begin();    
//     // log_c_one("Zeile 3.1");
//     // log_c_one("Zeile 3.2");
//     // log_c_one("Zeile 3.3");
//     // log_c_one("Zeile 3.4");        
//     // log_c_multi_end();
//     //
//     // log_c_one("Zeile 4");
//     // log_c_one("Zeile 5");
//     //
//     //
//     // log_c_multi_end();
//
//
//     log_c_one("Hugo");
//     log_c_function_begin("foo");
//
//     log_c_function_params(log_one("my param"));
//
//     log_c_function_params(log_one("your param"));
//
//     log_c_loop_begin("my loop");
//
//     log_c_loop_iter_begin(1);
//
//     log_c_columns_begin(" <#> ");
//
//     log_c_one("Hugo");
//
//     log_c_one("Hase");
//
//     log_c_multi_begin();
//
//     log_c_one("Spalte 3 Zeile 1");
//     log_c_one("Spalte 3 Zeile 2");
//     log_c_one("Spalte 3 Zeile 3");
//     log_c_one("Spalte 3 Zeile 4");
//
//     log_c_multi_end();
//
//     log_c_one("Spalte 4");
//
//     log_c_columns_begin("|");
//
//     log_c_multi_begin();
//
//     log_c_one("Sub Spalte 1 Zeile 1");
//     log_c_one("Sub Spalte 1 Zeile 2");
//
//     log_c_multi_end();
//
//     log_c_multi_begin();
//
//     log_c_one("Sub Spalte 2 Zeile 1");
//     log_c_one("Sub Spalte 2 Zeile 2");
//
//     log_c_multi_end();
//
//     log_c_columns_end();
//
//     log_c_values_begin();
//
//     log_c_values_add_b("isses wahr", true);
//     log_c_values_add_d("bouble", 24.7345);
//     const auto xmlValue = log_xml("xml-value");
//     log_xml_inner(xmlValue, log_one("wert im we"));
//     log_c_values_add_t("isses wahr", xmlValue);
//     log_c_values_add_i("integeres", 4711);
//     log_c_values_add("stresa", "ich bin ein Stirargafagf");
//     log_c_values_add("bummleum", "bobbela");
//
//     log_c_values_end();
//
//     log_c_multi_begin();
//
//     log_c_one("Zeile 1");
//     log_c_one("Zeile 2");
//     log_c_one("Zeile 3 mit ganz langem Text");
//     log_c_one("blubber");
//
//     log_c_multi_end();
//
//     log_c_columns_end();
//
//     log_c_loop_end();
//
//     log_c_loop_end();
//
//     log_c_function_end();
//
//
//     dout "BEGIN PRINT ##################################################################" << endl;
//
//     log_c_print();
//
//     dout "## END" << endl;
//
//     return 0;
// }
