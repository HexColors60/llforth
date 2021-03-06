//
// Created by Ryosuke Iwanaga on 2018-11-02.
//

#include "engine.h"
#include "dict.h"
#include "words.h"
#include "stack.h"
#include "util.h"
#include "lib.h"

extern "C" {
    void* create_reader(int, char**);
    int read_word_from_reader(void*, char*, int);
    void destroy_reader(void*);
}

Reader::Reader(int argc, char** argv) {
    raw = create_reader(argc, argv);
}

Reader::~Reader() {
    destroy_reader(raw);
}

std::optional<std::string> Reader::read() {
    char buf[1024];
    char* ptr = buf;
    int num = read_word_from_reader(raw, ptr, 1024);
    if (num == 0) {
        switch(buf[0]) {
            case 0:
            case 10:
                return read();
            default:
                return std::nullopt;
        }
    }
    std::string s(buf);
    assert(s.size() == num);
    return s;
}

struct Token {
    enum Type {
        String,
        Br,
        Label,
        Colon,
        Name,
        Semicolon,
        BrLabel,
        Immediate,
        Lit,
        DoubleQuote,
        QuoteString,
    } type;
    std::string value;

    static Token get(const std::string& str, const Type& type) {
        return Token{.value=str, .type=type};
    }

    static Token get(const std::string& str) {
        std::smatch label_match;
        Type type;
        if (std::regex_match(str, label_match, std::regex("(\\..+):"))) {
            return get(label_match[1], Label);
        } else {
            if (str == ":") { type = Colon; }
            else if (str == ";") { type = Semicolon; }
            else if (str == "branch" || str == "0branch") { type = Br; }
            else if (str == "immediate") { type = Immediate; }
            else if (str == "'") { type = Lit; }
            else if (str == ".\"") { type = DoubleQuote; }
            else { type = String; }
            return get(str, type);
        }
    }
};

std::ostream& operator<<(std::ostream& os, const Token& token) {
    return os << token.type << " " << token.value;
}

struct Tokenizer {
    std::vector<Token> tokens = {};
    Reader* reader;

    explicit Tokenizer(Reader* _reader) { reader = _reader; }

    void run() {
        while (auto str = reader->read()) {
            auto token = Token::get(*str);
            tokens.emplace_back(token);
            switch (token.type) {
                case Token::Colon: add_next(Token::Name); break;
                case Token::Br: add_next(Token::BrLabel); break;
                case Token::Lit: add_next(Token::String); break;
                case Token::DoubleQuote: add_next(Token::QuoteString); break;
                default: break;
            }
        }
    }

    void add_next(const Token::Type& type) {
        if (auto next = reader->read()) {
            auto next_token = Token::get(*next, type);
            tokens.emplace_back(next_token);
        } else {
            assert(false);
        }
    }
};

struct WordDefinition {
    struct Code {
        enum Type {Word, Int, BrLabel, String} type;
        std::string value;
        Constant* xt;
    };

    std::string name;
    bool is_immediate = false;
    std::map<std::string, int> labels = {};
    std::vector<Code> codes = {};

    void add_token(const Token& token) {
        switch (token.type) {
            case Token::Name:
                name = token.value; break;
            case Token::Label:
                labels[token.value] = (int)codes.size(); break;
            case Token::BrLabel:
                codes.push_back(Code{.type=Code::BrLabel, .value=token.value}); break;
            case Token::Br:
            case Token::String:
                add_string(token.value); break;
            case Token::DoubleQuote:
            case Token::Lit:
                add_string("lit"); break;
            case Token::QuoteString:
                codes.push_back(Code{.type=Code::String, .value=token.value});
                add_string("prints");
                break;
            default:
                assert(false); break;
        }
    }

    void add_string(const std::string& value) {
        auto found = dict::Dictionary.find(value);
        if (found == dict::Dictionary.end()) {
            codes.push_back(Code{.type=Code::Word, .xt=words::Lit.xt, .value="lit"});
            codes.push_back(Code{.type=Code::Int, .value=value});
        } else {
            codes.push_back(Code{.type=Code::Word, .xt=found->second.xt, .value=value});
        }
    }

    void compile() {
        add_string("exit");
        auto compiled = std::vector<std::variant<Constant*,int>>();
        for (auto code: codes) {
            switch (code.type) {
                case Code::BrLabel: {
                    auto found = labels.find(code.value);
                    if (found == labels.end()) {
                        assert(false);
                    } else {
                        compiled.push_back(found->second);
                    }
                    break;
                }
                case Code::Word: {
                    assert(code.xt != nullptr);
                    compiled.push_back(code.xt);
                    break;
                }
                case Code::Int: {
                    try {
                        auto xt = words::GetConstantIntToXtPtr(std::stoi(code.value));
                        compiled.push_back(xt);
                    } catch(...) {
                        assert(false);
                    }
                    break;
                }
                case Code::String: {
                    auto xt = words::GetConstantStrToXtPtr(code.value);
                    compiled.push_back(xt);
                    break;
                }
                default:
                    assert(false);
            }
        }
        dict::AddColonWord(name, words::Docol.addr, compiled, is_immediate);
    }
};

std::ostream& operator<<(std::ostream& os, const WordDefinition::Code& code) {
    return os << code.value;
}

std::ostream& operator<<(std::ostream& os, const WordDefinition& word) {
    os << word.name << " ";
    //for (auto t: word.tokens) { os << t << ", "; }
    //for (auto p: word.labels) { os << p.first << "=>" << p.second << " "; }
    for (auto c: word.codes) { os << c << " "; }
    return os;
}

static std::vector<WordDefinition> Parse(const std::vector<Token>& tokens) {
    std::vector<WordDefinition> words = {};
    WordDefinition def;
    bool is_def = false;
    auto it = tokens.begin();
    while (it != tokens.end()) {
        auto token = *it++;
        switch (token.type) {
            case Token::Colon: {
                assert(!is_def);
                is_def = true;
                def = WordDefinition();
                break;
            }
            case Token::Semicolon: {
                assert(is_def);
                is_def = false;
                auto next = *it++;
                if (next.type == Token::Immediate) {
                    def.is_immediate = true;
                } else {
                    it--;
                }
                words.push_back(def);
                break;
            }
            default: {
                assert(is_def);
                def.add_token(token);
                break;
            }
        }
    }
    return words;
}

static void MainLoop(int argc, char** argv) {
    Reader reader(argc, argv);
    Tokenizer tokenizer(&reader);
    tokenizer.run();
    auto words = Parse(tokenizer.tokens);
    for (auto w: words) {
        w.compile();
        std::cerr << w << std::endl;
    }
}

int main(int argc, char** argv) {
    core::CreateModule("main");
    engine::Initializers = {
            dict::Initialize,
            stack::Initialize,
            words::Initialize,
    };
    engine::Finalizers = {
            dict::Finalize,
    };
    engine::Initialize();

    MainLoop(argc, argv);

    engine::Finalize();
    core::DumpModule();
}