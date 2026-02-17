#include <bits/stdc++.h>
using namespace std;

struct ExecutionContext {
    unordered_map<string, long long> variables;
    vector<long long> outputs;
};

static inline string trim(const string &s) {
    size_t i = 0, j = s.size();
    while (i < j && isspace(static_cast<unsigned char>(s[i]))) ++i;
    while (j > i && isspace(static_cast<unsigned char>(s[j - 1]))) --j;
    return s.substr(i, j - i);
}

static inline vector<string> split(const string &s) {
    vector<string> res;
    string cur;
    istringstream iss(s);
    while (iss >> cur) res.push_back(cur);
    return res;
}

static inline bool isIntegerToken(const string &tok) {
    if (tok.empty()) return false;
    size_t start = (tok[0] == '-' ? 1 : 0);
    if (start == tok.size()) return false;
    for (size_t i = start; i < tok.size(); ++i) if (!isdigit(static_cast<unsigned char>(tok[i]))) return false;
    return true;
}

static inline long long evalToken(const string &tok, const ExecutionContext &ctx) {
    if (isIntegerToken(tok)) return stoll(tok);
    auto it = ctx.variables.find(tok);
    if (it != ctx.variables.end()) return it->second;
    // Default uninitialized variables to 0
    return 0;
}

struct Node {
    virtual ~Node() = default;
    virtual void execute(ExecutionContext &ctx) const = 0;
};

struct PrintNode : Node {
    string operand; // variable name or integer literal
    explicit PrintNode(string op) : operand(std::move(op)) {}
    void execute(ExecutionContext &ctx) const override {
        long long v = evalToken(operand, ctx);
        ctx.outputs.push_back(v);
    }
};

struct ForNode : Node {
    string iterVar;
    string startTok;
    string endTok;
    vector<unique_ptr<Node>> body;

    ForNode(string iv, string st, string en)
        : iterVar(std::move(iv)), startTok(std::move(st)), endTok(std::move(en)) {}

    void execute(ExecutionContext &ctx) const override {
        long long s = evalToken(startTok, ctx);
        long long e = evalToken(endTok, ctx);
        if (s <= e) {
            for (long long i = s; i <= e; ++i) {
                ctx.variables[iterVar] = i;
                for (const auto &n : body) n->execute(ctx);
            }
        } else {
            // If start > end, run zero times
        }
    }
};

enum class CmpOp { EQ, NE, LT, GT };

struct Condition {
    string left;
    string right;
    CmpOp op;
    bool eval(const ExecutionContext &ctx) const {
        long long l = evalToken(left, ctx);
        long long r = evalToken(right, ctx);
        switch (op) {
            case CmpOp::EQ: return l == r;
            case CmpOp::NE: return l != r;
            case CmpOp::LT: return l < r;
            case CmpOp::GT: return l > r;
        }
        return false;
    }
};

struct IfNode : Node {
    Condition cond;
    vector<unique_ptr<Node>> yesBlock;
    vector<unique_ptr<Node>> noBlock;
    explicit IfNode(Condition c) : cond(std::move(c)) {}
    void execute(ExecutionContext &ctx) const override {
        const auto &blk = cond.eval(ctx) ? yesBlock : noBlock;
        for (const auto &n : blk) n->execute(ctx);
    }
};

static Condition parseCondition(const string &line) {
    // line starts with "if "
    string expr = trim(line.substr(2)); // remove leading "if"
    // Try operators in order of multi-char first
    vector<pair<string, CmpOp>> ops = {{"==", CmpOp::EQ}, {"!=", CmpOp::NE}, {"<", CmpOp::LT}, {">", CmpOp::GT}};
    for (const auto &p : ops) {
        const string &sym = p.first;
        size_t pos = expr.find(sym);
        if (pos != string::npos) {
            string L = trim(expr.substr(0, pos));
            string R = trim(expr.substr(pos + sym.size()));
            return Condition{L, R, p.second};
        }
    }
    // Fallback: always false
    return Condition{"0", "1", CmpOp::EQ};
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<string> rawLines;
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        string t = trim(line);
        if (!t.empty()) rawLines.push_back(t);
    }

    if (rawLines.size() < 3) return 0;

    // Last two lines are variables and their values
    vector<string> varNames = split(rawLines[rawLines.size() - 2]);
    vector<string> valTokens = split(rawLines.back());

    vector<string> codeLines(rawLines.begin(), rawLines.end() - 2);

    ExecutionContext ctx;
    for (size_t i = 0; i < varNames.size(); ++i) {
        long long v = 0;
        if (i < valTokens.size() && isIntegerToken(valTokens[i])) v = stoll(valTokens[i]);
        ctx.variables[varNames[i]] = v;
    }

    vector<unique_ptr<Node>> program;
    vector<vector<unique_ptr<Node>>*> blockStack;
    blockStack.push_back(&program);

    struct IfOpen { IfNode *node; };
    vector<IfOpen> ifStack;
    enum class Compound { ROOT, FOR, IF };
    vector<Compound> compoundStack;
    compoundStack.push_back(Compound::ROOT);
    enum class Branch { IF_YES, IF_NO };
    vector<Branch> branchStack;

    auto appendNode = [&](unique_ptr<Node> n) {
        blockStack.back()->push_back(std::move(n));
    };

    for (size_t i = 0; i < codeLines.size(); ++i) {
        const string &ln = codeLines[i];
        if (ln.rfind("print ", 0) == 0) {
            string tok = trim(ln.substr(6));
            appendNode(make_unique<PrintNode>(tok));
        } else if (ln.rfind("for ", 0) == 0) {
            // for iter start end
            vector<string> parts = split(ln);
            if (parts.size() == 4) {
                auto f = make_unique<ForNode>(parts[1], parts[2], parts[3]);
                ForNode *ptr = f.get();
                appendNode(std::move(f));
                compoundStack.push_back(Compound::FOR);
                blockStack.push_back(&ptr->body);
            }
        } else if (ln.rfind("if ", 0) == 0) {
            Condition c = parseCondition(ln);
            auto in = make_unique<IfNode>(c);
            IfNode *ptr = in.get();
            appendNode(std::move(in));
            compoundStack.push_back(Compound::IF);
            ifStack.push_back({ptr});
            // next "Yes"/"No" will direct where to push statements
        } else if (ln == "Yes") {
            if (!ifStack.empty()) {
                // Start filling yes branch
                blockStack.push_back(&ifStack.back().node->yesBlock);
                branchStack.push_back(Branch::IF_YES);
            }
        } else if (ln == "No") {
            // Switch to no branch for current if
            if (!branchStack.empty() && branchStack.back() == Branch::IF_YES) {
                branchStack.pop_back();
                blockStack.pop_back();
            }
            if (!ifStack.empty()) {
                blockStack.push_back(&ifStack.back().node->noBlock);
                branchStack.push_back(Branch::IF_NO);
            }
        } else if (ln == "end") {
            if (!compoundStack.empty()) {
                Compound top = compoundStack.back();
                if (top == Compound::FOR) {
                    compoundStack.pop_back();
                    if (!blockStack.empty()) blockStack.pop_back();
                } else if (top == Compound::IF) {
                    // Close any open branch for this if
                    if (!branchStack.empty() &&
                        (branchStack.back() == Branch::IF_YES || branchStack.back() == Branch::IF_NO)) {
                        branchStack.pop_back();
                        if (!blockStack.empty()) blockStack.pop_back();
                    }
                    compoundStack.pop_back();
                    if (!ifStack.empty()) ifStack.pop_back();
                } else {
                    // ROOT end should not occur; ignore
                }
            }
        } else {
            // Unknown line; ignore
        }
    }

    for (const auto &n : program) n->execute(ctx);
    for (long long v : ctx.outputs) cout << v << "\n";
    return 0;
}


